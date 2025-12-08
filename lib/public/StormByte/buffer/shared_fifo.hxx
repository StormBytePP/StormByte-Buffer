#pragma once

#include <StormByte/buffer/fifo.hxx>

#include <condition_variable>
#include <mutex>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
			void  Wait(const std::size_t& n, std::unique_lock<std::recursive_mutex>& lock) const;
 * including FIFO buffers, thread-safe shared buffers, and producer-consumer patterns.
 */
namespace StormByte::Buffer {
	/**
	 * @class SharedFIFO
	 * @brief Thread-safe FIFO built on top of @ref FIFO.
	 *
	 * @par Overview
	 *  SharedFIFO wraps the non-thread-safe @ref FIFO with a mutex and a
	 *  condition variable to provide safe concurrent access from multiple
	*  producer/consumer threads. It preserves the byte-oriented FIFO
	*  semantics of @ref FIFO while adding blocking behavior for reads and
	*  extracts.
	*
	* @par Blocking semantics
	*  - @ref Read(const std::size_t&) blocks until the requested number of bytes are
	*    available from the current non-destructive read position, or until
	*    the FIFO is closed via @ref Close(). If @c count == 0, it returns
	*    immediately with all bytes available from the current read position.
	*  - @ref Extract(const std::size_t&) blocks until at least @c count bytes exist
	*    in the buffer (destructive), or until closed. If @c count == 0, it
	*    returns immediately with all available data and clears the buffer.
	*
	* @par Close behavior
	*  @ref Close() marks the FIFO as closed and notifies all waiting threads.
	*  Subsequent calls to @ref Write() are ignored. Waiters will wake and
	*  complete using whatever data is presently available (which may be none).
	*
	* @par Seek behavior
	*  @ref Seek() updates the internal non-destructive read position and
	*  notifies waiters so blocked readers can re-evaluate their predicates
	*  relative to the new position.
	*
	* @par Thread safety
	*  All public member functions of SharedFIFO are thread-safe. Methods that
	*  mutate internal state (Write/Extract/Clear/Close/Seek/Reserve) acquire
	*  the internal mutex. Read accessors also acquire the mutex to maintain
	*  consistency with the current head/tail/read-position state.
	*/
	class STORMBYTE_BUFFER_PUBLIC SharedFIFO: public FIFO {
		public:
			/**
			 * @brief Construct a SharedFIFO with optional initial capacity.
			 * @param capacity Initial number of bytes to allocate in the buffer.
			 *        Behaves like @ref FIFO and may grow as needed.
			 */
			SharedFIFO() noexcept 								= default;

			/**
			 * @brief Construct a SharedFIFO with initial data.
			 * @param data Initial byte vector to populate the FIFO.
			 */
			inline SharedFIFO(const DataType& data): FIFO(data) {}

			/**
			 * @brief Construct a SharedFIFO with initial data using move semantics.
			 * @param data Initial byte vector to move into the FIFO.
			 */
			inline SharedFIFO(DataType&& data) noexcept: FIFO(std::move(data)) {}

			/**
			 * @brief Construct FIFO from an input range.
			 * @tparam R Input range whose elements are convertible to `std::byte`.
			 * @param r The input range to copy from.
			 * @note This overload is constrained so that it does not participate when
			 *       the argument type is the library `DataType` to avoid ambiguity
			 *       with the existing `DataType` overloads.
			 */
			template<std::ranges::input_range R>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
				requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
				(!std::same_as<std::remove_cvref_t<R>, DataType>)
			inline SharedFIFO(const R& r) noexcept: FIFO(r), m_closed(false),
			m_error(false), m_error_message() {}

			/**
			 * @brief Construct FIFO from an rvalue range (moves when DataType rvalue).
			 * @tparam Rr Input range type; if it's `DataType` this will be moved into
			 *            the internal buffer, otherwise elements are converted.
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline SharedFIFO(Rr&& r) noexcept: FIFO(std::forward<Rr>(r)), m_closed(false),
			m_error(false), m_error_message() {}

			/**
			 * @brief Construct FIFO from a string view (does not include terminating NUL).
		 	*/
			inline SharedFIFO(std::string_view sv) noexcept: FIFO(sv), m_closed(false),
			m_error(false), m_error_message() {}

			/**
			 * @brief Construct a SharedFIFO by copying or moving from a FIFO.
			 * @param other Source FIFO to copy or move from.
			 */
			inline SharedFIFO(const FIFO& other): FIFO(other) {}

			/**
			 * @brief Construct a SharedFIFO by moving from a FIFO.
			 * @param other Source FIFO to move from; left empty after move.
			 */
			inline SharedFIFO(FIFO&& other) noexcept: FIFO(std::move(other)) {}

			/**
			 * @brief Copy constructors are deleted.
			 * @details `SharedFIFO` contains synchronization primitives and
			 *          shared internal state (mutex, condition variable, etc.).
			 *          Copying or moving instances would require careful transfer
			 *          or duplication of these primitives which is unsafe and
			 *          error-prone. To avoid accidental misuse and subtle
			 *          concurrency bugs these constructors are explicitly
			 *          deleted. If duplication of the stored bytes is required,
			 *          copy the data via the base `FIFO` and construct a new
			 *          `SharedFIFO`.
			 */
			SharedFIFO(const SharedFIFO&) 						= delete;
			
			/**
			 * @brief Move constructor
			 * @param other Source SharedFIFO to move from; left in a valid but
			 */
			SharedFIFO(SharedFIFO&& other) noexcept				= default;
			
			/**
			 * @brief Virtual destructor.
			 */
			virtual ~SharedFIFO() noexcept 						= default;

			/**
			 * @brief Copy assignment from FIFO.
			 * @param other Source FIFO to copy from.
			 * @return Reference to this SharedFIFO.
			 */
			SharedFIFO& operator=(const FIFO& other);

			/**
			 * @brief Move assignment from FIFO.
			 * @param other Source FIFO to move from.
			 * @return Reference to this SharedFIFO.
			 */
			SharedFIFO& operator=(FIFO&& other) noexcept;

			/**
			 * @brief Copy and move assignment operators are deleted.
			 * @details Assigning one `SharedFIFO` to another would imply transferring
			 *          or duplicating ownership of internal synchronization primitives
			 *          (mutex/condition variable) and internal buffer state. This
			 *          is unsafe and therefore these operators are explicitly
			 *          deleted. Use explicit data-copy via `FIFO` if needed.
			 */
			SharedFIFO& operator=(const SharedFIFO&) 			= delete;

			/**
			 * @brief Move assignment operator.
			 * @param other Source SharedFIFO to move from; left in a valid but
			 *              unspecified state.
			 * @return Reference to this SharedFIFO.
			 */
			SharedFIFO& operator=(SharedFIFO&&) noexcept;

			/**
			 * @brief Equality comparison (thread-safe).
			 *
			 * Compares this `SharedFIFO` with `other` while holding both internal
			 * mutexes. The comparison delegates to the base `FIFO::operator==`, so
			 * equality semantics follow the FIFO implementation.
			 */
			bool operator==(const SharedFIFO& other) const noexcept;

			/**
			 * @brief Inequality comparison.
			 *
			 * Negates `operator==`.
			 */
			inline bool operator!=(const SharedFIFO& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Get the number of bytes available for reading.
			 * @return Number of bytes available from the current read position.
			 */
			virtual std::size_t 								AvailableBytes() const noexcept override;

			/**
			 * @brief Thread-safe clean of buffer data from start to read position.
			 * @see FIFO::Clean()
			 */
			void 												Clean() noexcept override;

			/**
			 * @brief Thread-safe clear of all buffer contents.
			 * @see FIFO::Clear()
			 */
			virtual void 										Clear() noexcept override;

			/**
			 * @brief Access the internal data buffer.
			 * @note Warning: Not thread-safe. Caller must ensure exclusive access.
			 * @return Constant reference to the internal DataType buffer.
			 */
			inline virtual const DataType& 						Data() const noexcept override {
				return m_buffer;
			}

			/**
			 * @brief Thread-safe close for further writes.
			 * @details Marks buffer as closed, notifies all waiting threads. Subsequent writes
			 *          are ignored. The buffer remains readable until all data is consumed.
			 * @see FIFO::Close(), IsWritable()
			 */
			virtual void 										Close() noexcept;

			/**
			 * @brief Thread-safe drop operation.
			 * @return true if the bytes were successfully dropped, false otherwise.
			 * @details Notifies waiting readers after dropping.
			 * @see FIFO::Drop()
			 */
			virtual bool 										Drop(const std::size_t& count) noexcept override;
			
			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size(), AvailableBytes()
			 * @note Since buffer works with read positions, Empty() might return false
			 * 	   even if there is no unread data (i.e., when read position is at the end of the buffer).
			 */
			virtual bool 										Empty() const noexcept override;
			
			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			virtual bool 										EoF() const noexcept override;

			/**
			 * @brief Check if the buffer is in an error state.
			 * @return true if the buffer is in error state, false otherwise.
			 */
			bool 												HasError() const noexcept;

			/**
			 * @brief Produce a thread-safe hexdump of the buffer.
			 * @param collumns Number of columns per line (0 -> default 16).
			 * @param byte_limit Maximum number of bytes to include (0 -> no limit).
			 * @return A formatted dump string. Does not append a trailing newline.
			 * @details Acquires the internal mutex for reading so the dump represents
			 *          a consistent snapshot. The returned string begins with a
			 *          status line of the form `Status: opened|closed, ready|error`\n
			*          followed by the same output produced by `FIFO::HexDump()`.
			* Example output:
			* @code{.text}
			* Size: 13 bytes
			* Read Position: 0
			* Status: opened and ready
			* 00000000: 48 65 6C 6C 6F 2C 20 77 6F 72 6C 64 21           Hello, world!
			* @endcode
			*/
			virtual std::string 								HexDump(const std::size_t& collumns = 0, const std::size_t& byte_limit = 0) const noexcept override;

			/**
			 * @brief Check if the buffer is readable (not in error state).
			 * @return true if readable, false if buffer is in error state.
			 * @details A buffer becomes unreadable when SetError() is called. Use in
			 *          combination with AvailableBytes() to check if there is data
			 *          pending to read.
			 * @see SetError(), IsWritable(), AvailableBytes(), EoF()
			 */
			inline virtual bool 								IsReadable() const noexcept override { return !m_error; }

			/**
			 * @brief Check if the buffer is writable (not closed and not in error state).
			 * @return true if writable, false if closed or in error state.
			 * @details A buffer becomes unwritable when Close() or SetError() is called.
			 * @see Close(), SetError(), IsReadable()
			 */
			inline virtual bool 								IsWritable() const noexcept override { return !m_closed && !m_error; }

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Unused for base class; included for API consistency.
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
			 * If Position is set to Absolute and offset is negative the operation is noop
			 */
			virtual void 										Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Thread-safe error state setting.
			 * @details Marks buffer as erroneous (unreadable and unwritable), notifies all
			 *          waiting threads. Subsequent writes are ignored and reads will fail.
			 * @see FIFO::SetError(), IsReadable(), IsWritable()
			 */
			virtual void 										SetError() noexcept;
			
			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Empty(), AvailableBytes()
			 */
			virtual std::size_t 								Size() const noexcept override;

		protected:
			bool m_closed {false};    							///< Whether the SharedFIFO is closed for further writes.
			bool m_error {false};    							///< Whether the SharedFIFO is in an error state.
			std::string m_error_message;    					///< Optional error message associated with the error state.

		private:
			mutable std::mutex m_mutex;							///< Mutex protecting internal state.
			mutable std::condition_variable_any m_cv;			///< Condition variable for blocking reads/writes.

			/**
			 * @brief Produce a hexdump header with size and read position.
			 * @return ostringstream containing the hexdump header.
			 */
			std::ostringstream 									HexDumpHeader() const noexcept override;

			/**
			 * @brief Internal helper for read operations.
			 * @param count Number of bytes to read.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Additional flag for read operation (1: copy, 2: move)
			 * @return bool indicating success or failure.
			 * @details Shared logic for read operations that first checks the internal
			 *          buffer, then calls the external read function if needed.
			 */
			virtual bool 										ReadInternal(const std::size_t& count, DataType& outBuffer, const Operation& flag) noexcept override;
			/**
			 * @brief Internal helper for read operations.
			 * @param count Number of bytes to read.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Additional flag for read operation (1: copy, 2: move)
			 * @return `ExpectedVoid<Error>` indicating success or failure.
			 * @details Shared logic for read operations that first checks the internal
			 *          buffer, then calls the external read function if needed.
			 */
			virtual bool 										ReadInternal(const std::size_t& count, WriteOnly& outBuffer, const Operation& flag) noexcept override;

			/**
			 * @brief Wait until at least @p n bytes are available from the current read position
			 *        (or buffer becomes unreadable).
			 * @param n Number of bytes to wait for; if 0, returns immediately.
			 * @param lock The caller-held unique_lock for the internal mutex; the
			 *        method will wait using this lock and return with it still held.
			 * @note Wakes and returns when Close() or SetError() is called, even if the
			 *       requested @p n bytes are not available.
			 * @see Close(), SetError(), IsReadable()
			 */
			void 												Wait(const std::size_t& n, std::unique_lock<std::mutex>& lock) const;

			/**
			 * @brief Internal helper for write operations.
			 * @param dst Destination buffer to write into.
			 * @param src Source buffer to write from.
			 * @return `bool` indicating success or failure.
			 */
			virtual bool 										WriteInternal(const std::size_t& count, const DataType& src) noexcept override;

			/**
			 * @brief Internal helper for write operations.
			 * @param dst Destination buffer to write into.
			 * @param src Source buffer to write from.
			 * @return `bool` indicating success or failure.
			 */
			virtual bool 										WriteInternal(const std::size_t& count, DataType&& src) noexcept override;
	};
}
