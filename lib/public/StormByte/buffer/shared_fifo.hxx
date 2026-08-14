#pragma once

#include <StormByte/buffer/fifo.hxx>

#include <condition_variable>
#include <mutex>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
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
	 *  @ref Close() marks the FIFO as closed (via the base class) and notifies
	 *  all waiting threads. Subsequent calls to @ref Write() fail. Waiters will
	 *  wake and complete using whatever data is presently available (which may
	 *  be none).
	 *
	 * @par Error behavior
	 *  @ref SetError() puts the buffer into a permanent error state (unreadable
	 *  and unwritable) and wakes all waiters.
	 *
	 * @par Seek behavior
	 *  @ref Seek() updates the internal non-destructive read position and
	 *  notifies waiters so blocked readers can re-evaluate their predicates
	 *  relative to the new position.
	 *
	 * @par Thread safety
	 *  All public member functions of SharedFIFO are thread-safe. Methods that
	 *  mutate internal state (Write/Extract/Clear/Close/Seek) acquire the
	 *  internal mutex. Read accessors also acquire the mutex to maintain
	 *  consistency with the current state.
	 *
	 * @note The closed/error state is owned by the base @ref FIFO class.
	 *       SharedFIFO only protects access to it and notifies waiters.
	 */
	class STORMBYTE_BUFFER_PUBLIC SharedFIFO : public FIFO {
		public:
			/**
			 * @brief Construct a SharedFIFO with optional initial capacity.
			 * @details Behaves like @ref FIFO and may grow as needed.
			 */
			SharedFIFO() noexcept = default;

			/**
			 * @brief Construct a SharedFIFO with initial data.
			 * @param data Initial byte vector to populate the FIFO.
			 */
			inline SharedFIFO(const DataType& data) : FIFO(data) {}

			/**
			 * @brief Construct a SharedFIFO with initial data using move semantics.
			 * @param data Initial byte vector to move into the FIFO.
			 */
			inline SharedFIFO(DataType&& data) noexcept : FIFO(std::move(data)) {}

			/**
			 * @brief Construct SharedFIFO from an input range.
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
			inline SharedFIFO(const R& r) noexcept : FIFO(r) {}

			/**
			 * @brief Construct SharedFIFO from an rvalue range (moves when DataType rvalue).
			 * @tparam Rr Input range type; if it's `DataType` this will be moved into
			 *            the internal buffer, otherwise elements are converted.
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline SharedFIFO(Rr&& r) noexcept : FIFO(std::forward<Rr>(r)) {}

			/**
			 * @brief Construct SharedFIFO from a string view (does not include terminating NUL).
			 */
			inline SharedFIFO(std::string_view sv) noexcept : FIFO(sv) {}

			/**
			 * @brief Construct SharedFIFO from a C string pointer (null-terminated).
			 */
			inline SharedFIFO(const char* s) noexcept : FIFO(s) {}

			/**
			 * @brief Construct a SharedFIFO by copying from a FIFO.
			 * @param other Source FIFO to copy from.
			 */
			inline SharedFIFO(const FIFO& other) : FIFO(other) {}

			/**
			 * @brief Construct a SharedFIFO by moving from a FIFO.
			 * @param other Source FIFO to move from; left empty after move.
			 */
			inline SharedFIFO(FIFO&& other) noexcept : FIFO(std::move(other)) {}

			/**
			 * @brief Copy constructors are deleted.
			 * @details `SharedFIFO` contains synchronization primitives.
			 *          Copying instances would require careful transfer of these
			 *          primitives which is unsafe. These constructors are
			 *          explicitly deleted.
			 */
			SharedFIFO(const SharedFIFO&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source SharedFIFO to move from; left in a valid but
			 *              unspecified state.
			 */
			SharedFIFO(SharedFIFO&& other) noexcept = default;

			/**
			 * @brief Virtual destructor.
			 */
			virtual ~SharedFIFO() noexcept = default;

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
			 * @brief Copy assignment operator is deleted.
			 * @details Assigning one `SharedFIFO` to another would imply transferring
			 *          or duplicating ownership of internal synchronization primitives
			 *          (mutex/condition variable). This is unsafe and therefore this
			 *          operator is explicitly deleted.
			 */
			SharedFIFO& operator=(const SharedFIFO&) = delete;

			/**
			 * @brief Move assignment operator.
			 * @param other Source SharedFIFO to move from; left in a valid but
			 *              unspecified state.
			 * @return Reference to this SharedFIFO.
			 * @note Mutex and condition_variable are not movable; the implementation
			 *       leaves the object in a valid state.
			 */
			SharedFIFO& operator=(SharedFIFO&&) noexcept;

			/**
			 * @brief Equality comparison (thread-safe).
			 *
			 * Compares this `SharedFIFO` with `other` while holding both internal
			 * mutexes. The comparison delegates to the base `FIFO::operator==`.
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
			 * @brief Get the number of bytes available for reading (thread-safe).
			 * @return Number of bytes available from the current read position.
			 */
			virtual std::size_t AvailableBytes() const noexcept override;

			/**
			 * @brief Thread-safe clean of buffer data from start to read position.
			 * @see FIFO::Clean()
			 */
			void Clean() noexcept override;

			/**
			 * @brief Thread-safe clear of all buffer contents.
			 * @details Does **not** clear the closed/error flags.
			 * @see FIFO::Clear()
			 */
			virtual void Clear() noexcept override;

			/**
			 * @brief Access the internal data buffer.
			 * @warning Not thread-safe. Caller must ensure exclusive access.
			 * @return Constant reference to the internal DataType buffer.
			 */
			inline virtual const DataType& Data() const noexcept override {
				return m_buffer;
			}

			/**
			 * @brief Thread-safe close for further writes.
			 * @details Marks the buffer as closed (via the base class) and notifies
			 *          all waiting threads. Subsequent writes fail. The buffer remains
			 *          readable until all data is consumed.
			 * @see FIFO::Close(), IsWritable()
			 */
			virtual void Close() noexcept override;

			/**
			 * @brief Thread-safe drop operation.
			 * @param count Number of bytes to drop.
			 * @return true if the bytes were successfully dropped, false otherwise.
			 * @details Notifies waiting readers after dropping.
			 * @see FIFO::Drop()
			 */
			virtual bool Drop(const std::size_t& count) noexcept override;

			/**
			 * @brief Check if the buffer is empty (thread-safe).
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size(), AvailableBytes()
			 * @note Since the buffer works with read positions, Empty() might return
			 *       false even if there is no unread data.
			 */
			virtual bool Empty() const noexcept override;

			/**
			 * @brief Check if the reader has reached end-of-file (thread-safe).
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			virtual bool EoF() const noexcept override;

			/**
			 * @brief Check if the buffer is in an error state (thread-safe).
			 * @return true if the buffer is in error state, false otherwise.
			 */
			bool HasError() const noexcept;

			/**
			 * @brief Produce a thread-safe hexdump of the buffer.
			 * @param columns Number of columns per line (0 → default 16).
			 * @param byte_limit Maximum number of bytes to include (0 → no limit).
			 * @return A formatted dump string. Does not append a trailing newline.
			 * @details Acquires the internal mutex so the dump represents a consistent
			 *          snapshot. The returned string begins with size / position /
			 *          status followed by the hex/ASCII lines.
			 */
			virtual std::string HexDump(const std::size_t& columns = 0,
										const std::size_t& byte_limit = 0) const noexcept override;

			/**
			 * @brief Check if the buffer is readable (not in error state).
			 * @return true if readable, false if buffer is in error state.
			 * @details A buffer becomes unreadable when SetError() is called.
			 * @see SetError(), IsWritable(), AvailableBytes(), EoF()
			 */
			inline virtual bool IsReadable() const noexcept override {
				std::scoped_lock lock(m_mutex);
				return FIFO::IsReadable();
			}

			/**
			 * @brief Check if the buffer is writable (not closed and not in error state).
			 * @return true if writable, false if closed or in error state.
			 * @details A buffer becomes unwritable when Close() or SetError() is called.
			 * @see Close(), SetError(), IsReadable()
			 */
			inline virtual bool IsWritable() const noexcept override {
				std::scoped_lock lock(m_mutex);
				return FIFO::IsWritable();
			}

			/**
			 * @brief Move the read position for non-destructive reads (thread-safe).
			 * @param offset The offset value to apply.
			 * @param mode Absolute or Relative.
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
			 */
			virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Thread-safe error state setting.
			 * @details Marks the buffer as erroneous (unreadable and unwritable) via the
			 *          base class and notifies all waiting threads. Subsequent writes are
			 *          ignored and reads will fail.
			 * @see FIFO::SetError(), IsReadable(), IsWritable()
			 */
			virtual void SetError() noexcept override;

			/**
			 * @brief Get the current number of bytes stored in the buffer (thread-safe).
			 * @return The total number of bytes available for reading.
			 * @see Empty(), AvailableBytes()
			 */
			virtual std::size_t Size() const noexcept override;

		protected:
			// Closed/error state is owned by the base FIFO class.
			// SharedFIFO only protects access and notifies waiters.

		private:
			mutable std::mutex                 m_mutex;   ///< Mutex protecting internal state.
			mutable std::condition_variable_any m_cv;     ///< Condition variable for blocking reads.

			/**
			 * @brief Produce a hexdump header with size, read position and status.
			 * @return ostringstream containing the hexdump header.
			 * @note Delegates to the base implementation which already includes status.
			 */
			std::ostringstream HexDumpHeader() const noexcept override;

			/**
			 * @brief Internal helper for read operations (blocking).
			 * @param count Number of bytes to read.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Operation type (Extract / Read / Peek).
			 * @return true on success, false on error or insufficient data after close.
			 */
			virtual bool ReadInternal(const std::size_t& count, DataType& outBuffer,
									const Operation& flag) noexcept override;

			/**
			 * @brief Internal helper for read operations into a WriteOnly (blocking).
			 * @param count Number of bytes to read.
			 * @param outBuffer WriteOnly destination.
			 * @param flag Operation type (Extract / Read / Peek).
			 * @return true on success, false on error or insufficient data after close.
			 */
			virtual bool ReadInternal(const std::size_t& count, WriteOnly& outBuffer,
									const Operation& flag) noexcept override;

			/**
			 * @brief Wait until at least @p n bytes are available from the current
			 *        read position (or the buffer becomes closed/error).
			 * @param n Number of bytes to wait for; if 0, returns immediately.
			 * @param lock The caller-held unique_lock for the internal mutex; the
			 *        method will wait using this lock and return with it still held.
			 * @note Wakes and returns when Close() or SetError() is called, even if
			 *       the requested @p n bytes are not available.
			 * @see Close(), SetError(), IsReadable()
			 */
			void Wait(const std::size_t& n, std::unique_lock<std::mutex>& lock) const;

			/**
			 * @brief Internal helper for write operations (copy).
			 * @param count Number of bytes to write.
			 * @param src Source buffer.
			 * @return true on success, false if closed/error or other failure.
			 */
			virtual bool WriteInternal(const std::size_t& count, const DataType& src) noexcept override;

			/**
			 * @brief Internal helper for write operations (move).
			 * @param count Number of bytes to write.
			 * @param src Source buffer.
			 * @return true on success, false if closed/error or other failure.
			 */
			virtual bool WriteInternal(const std::size_t& count, DataType&& src) noexcept override;
	};
}
