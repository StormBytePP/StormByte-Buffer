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
	*  - @ref Read(std::size_t) blocks until the requested number of bytes are
	*    available from the current non-destructive read position, or until
	*    the FIFO is closed via @ref Close(). If @c count == 0, it returns
	*    immediately with all bytes available from the current read position.
	*  - @ref Extract(std::size_t) blocks until at least @c count bytes exist
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
			SharedFIFO() noexcept;

			/**
			 * @brief Copy and move constructors are deleted.
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
			SharedFIFO(const SharedFIFO&) = delete;
			
			SharedFIFO(SharedFIFO&&) = delete;
			
			/**
			 * @brief Virtual destructor.
			 */
			virtual ~SharedFIFO() = default;

			/**
			 * @brief Copy and move assignment operators are deleted.
			 * @details Assigning one `SharedFIFO` to another would imply transferring
			 *          or duplicating ownership of internal synchronization primitives
			 *          (mutex/condition variable) and internal buffer state. This
			 *          is unsafe and therefore these operators are explicitly
			 *          deleted. Use explicit data-copy via `FIFO` if needed.
			 */
			SharedFIFO& operator=(const SharedFIFO&) = delete;

			SharedFIFO& operator=(SharedFIFO&&) = delete;

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
			 * @name Thread-safe overrides
			 * @{
			 */

			/**
			 * @brief Thread-safe close for further writes.
			 * @details Marks buffer as closed, notifies all waiting threads. Subsequent writes
			 *          are ignored. The buffer remains readable until all data is consumed.
			 * @see FIFO::Close(), IsWritable()
			 */
			void Close() noexcept;

			/**
			 * @brief Thread-safe error state setting.
			 * @details Marks buffer as erroneous (unreadable and unwritable), notifies all
			 *          waiting threads. Subsequent writes are ignored and reads will fail.
			 * @see FIFO::SetError(), IsReadable(), IsWritable()
			 */
			void SetError() noexcept;

			/**
			 * @brief Thread-safe blocking read from the buffer.
			 * @param count Number of bytes to read; 0 reads all available immediately.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error.
			 * @details Blocks until @p count bytes are available from the current read position,
			 *          or until the buffer becomes unreadable (closed or error). If buffer
			 *          becomes unreadable before the requested bytes are available the call
			 *          returns an error. See the base `FIFO::Read()` for the core (content-only)
			 *          semantics.
			 * @see FIFO::Read(), Wait(), IsReadable()
			 */
			virtual ExpectedData<ReadError> Read(std::size_t count = 0) const override;

			/**
			 * @brief Thread-safe blocking zero-copy read from the buffer.
			 * @param count Number of bytes to read; 0 reads all available.
			 * @return std::span<const std::byte> view over the requested bytes, or empty span if error.
			 * @details Blocks until @p count bytes are available from the current read position,
			 *          or until the buffer becomes unreadable. Returns a non-owning view without
			 *          copying. The span is valid until the next modifying operation.
			 *          
			 *          WARNING: The returned span becomes invalid after any write, extract, or
			 *          clear operation. Since this is a thread-safe class, other threads may
			 *          modify the buffer at any time, so the span should be used immediately.
			 * @see FIFO::Span(), Read(), Wait()
			 */
			virtual ExpectedSpan<ReadError> Span(std::size_t count = 0) const noexcept override;

			/**
			 * @brief Thread-safe blocking extract from the buffer.
			 * @param count Number of bytes to extract; 0 extracts all available immediately.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error.
			 * @details Blocks until @p count bytes are available, or until the buffer becomes
			 *          unreadable (closed or error). If buffer becomes unreadable before the
			 *          requested bytes are available the call returns an error. See the base
			 *          `FIFO::Extract()` for the core (content-only) semantics.
			 * @see FIFO::Extract(), Wait(), IsReadable()
			 */
			virtual ExpectedData<ReadError> Extract(std::size_t count = 0) override;

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
			virtual ExpectedVoid<WriteError> Write(std::span<const std::byte> data) override;

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
			virtual ExpectedVoid<WriteError> Write(const std::vector<std::byte>& data) override;

			/**
			 * @brief Thread-safe write with move semantics for byte vector.
			 * @param data Byte vector rvalue to move into the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Thread-safe version that uses move semantics to efficiently
			 *          transfer data from the rvalue vector. Notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
			virtual ExpectedVoid<WriteError> Write(const std::vector<std::byte>&& data) override;

			/**
			 * @brief Thread-safe append of another FIFO's full contents.
			 * @param other FIFO whose contents will be appended (const reference).
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Acquires the internal mutex, checks `m_closed` and `m_error`,
			 * then delegates to the base `FIFO::Write(const FIFO&)` to append the
			 * entire contents of `other`. Notifies waiting readers after the write.
			 */
			virtual ExpectedVoid<WriteError> Write(const FIFO& other) override;

			/**
			 * @brief Thread-safe append by moving another FIFO's full contents.
			 * @param other FIFO rvalue whose contents will be moved into this SharedFIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Acquires the internal mutex, checks `m_closed` and `m_error`,
			 * then delegates to the base `FIFO::Write(FIFO&&)` to perform an
			 * efficient move/steal of `other`'s contents. Notifies waiting readers
			 * after the write. Marked `noexcept` to reflect the base rvalue overload.
			 */
			virtual ExpectedVoid<WriteError> Write(FIFO&& other) noexcept override;

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data String to append to the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
			virtual ExpectedVoid<WriteError> Write(const std::string& data) override;

			/**
			 * @brief Thread-safe clear of all buffer contents.
			 * @see FIFO::Clear()
			 */
			virtual void Clear() noexcept override;

			/**
			 * @brief Thread-safe clean of buffer data from start to read position.
			 * @see FIFO::Clean()
			 */
			virtual void Clean() noexcept override;

			/**
			 * @brief Thread-safe seek operation.
			 * @details Notifies waiting readers after seeking.
			 * @see FIFO::Seek()
			 */
			virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Thread-safe peek operation.
			 * @param count Number of bytes to peek; 0 peeks all available immediately.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error.
			 * @details Blocks until @p count bytes are available from the current read position,
			 *          or until the buffer becomes unreadable (closed or error). If buffer
			 *          becomes unreadable before the requested bytes are available the call
			 *          returns an error. See the base `FIFO::Peek()` for the core (content-only)
			 *          semantics.
			 * @see FIFO::Peek(), Wait(), IsReadable()
			 */
			virtual ExpectedData<ReadError> Peek(std::size_t count = 0) const noexcept override;

			/**
			 * @brief Thread-safe skip operation.
			 * @details Notifies waiting readers after skipping.
			 * @see FIFO::Skip()
			 */
			virtual void Skip(const std::size_t& count) noexcept override;
			/** @} */
			
			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			bool EoF() const noexcept override;

			/**
			 * @brief Check if the buffer is readable (not in error state).
			 * @return true if readable, false if buffer is in error state.
			 * @details A buffer becomes unreadable when SetError() is called. Use in
			 *          combination with AvailableBytes() to check if there is data
			 *          pending to read.
			 * @see SetError(), IsWritable(), AvailableBytes(), EoF()
			 */
			inline bool IsReadable() const noexcept { return !m_error; }

			/**
			 * @brief Check if the buffer is writable (not closed and not in error state).
			 * @return true if writable, false if closed or in error state.
			 * @details A buffer becomes unwritable when Close() or SetError() is called.
			 * @see Close(), SetError(), IsReadable()
			 */
			inline bool IsWritable() const noexcept { return !m_closed && !m_error; }

			/**
			 * @brief Produce a thread-safe hexdump of the buffer.
			 * @param collumns Number of columns per line (0 -> default 16).
			 * @param byte_limit Maximum number of bytes to include (0 -> no limit).
			 * @return A formatted dump string. Does not append a trailing newline.
			 * @details Acquires the internal mutex for reading so the dump represents
			 *          a consistent snapshot. The returned string begins with a
			 *          status line of the form `Status: opened|closed, ready|error`\n
			*          followed by the same output produced by `FIFO::HexDump()`.
			*
			* @code{.text}
			* // Example output:
			* // Status: opened, ready
			* // Read Position: 0
			* // 00000000: 48 65 6C 6C 6F 2C 20 77 6F 72 6C 64 21           Hello, world!
			* @endcode
			*/
			std::string HexDump(const std::size_t& collumns = 0, const std::size_t& byte_limit = 0) const noexcept override;

		private:
			bool m_closed;    	///< Whether the SharedFIFO is closed for further writes.
			bool m_error;    	///< Whether the SharedFIFO is in an error state.

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
			void Wait(std::size_t n, std::unique_lock<std::mutex>& lock) const;

			/** @brief Internal mutex guarding all state mutations and reads. */
			mutable std::mutex m_mutex;
			/** @brief Condition variable used to block until data is available or closed. */
			mutable std::condition_variable_any m_cv;
	};
}
