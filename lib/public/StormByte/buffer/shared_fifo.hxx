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
    class STORMBYTE_BUFFER_PUBLIC SharedFIFO final: public FIFO {
        public:
            /**
             * @brief Construct a SharedFIFO with optional initial capacity.
             * @param capacity Initial number of bytes to allocate in the buffer.
             *        Behaves like @ref FIFO and may grow as needed.
             */
            SharedFIFO() noexcept;

            SharedFIFO(const SharedFIFO&) = delete;
            SharedFIFO& operator=(const SharedFIFO&) = delete;
            SharedFIFO(SharedFIFO&&) = delete;
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
             * @brief Virtual destructor.
             */
            virtual ~SharedFIFO() = default;

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
             * @return A vector containing the requested bytes, or error.
             * @details Blocks until @p count bytes are available from the current read position,
             *          or until the buffer becomes unreadable (closed or error). If buffer
             *          becomes unreadable before the requested bytes are available the call
             *          returns an error. See the base `FIFO::Read()` for the core (content-only)
             *          semantics.
             * @see FIFO::Read(), Wait(), IsReadable()
             */
            ExpectedData<InsufficientData> Read(std::size_t count = 0) const;

            /**
             * @brief Thread-safe blocking extract from the buffer.
             * @param count Number of bytes to extract; 0 extracts all available immediately.
             * @return A vector containing the extracted bytes, or error.
             * @details Blocks until @p count bytes are available, or until the buffer becomes
             *          unreadable (closed or error). If buffer becomes unreadable before the
             *          requested bytes are available the call returns an error. See the base
             *          `FIFO::Extract()` for the core (content-only) semantics.
             * @see FIFO::Extract(), Wait(), IsReadable()
             */
            ExpectedData<InsufficientData> Extract(std::size_t count = 0);

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return true if written, false if closed.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
            bool Write(const std::vector<std::byte>& data);

            /**
             * @brief Thread-safe append of another FIFO's full contents.
             * @param other FIFO whose contents will be appended (const reference).
             * @return true if the write succeeded, false if the SharedFIFO is closed or in error.
             * @details Acquires the internal mutex, checks `m_closed` and `m_error`,
             * then delegates to the base `FIFO::Write(const FIFO&)` to append the
             * entire contents of `other`. Notifies waiting readers after the write.
             */
            bool Write(const FIFO& other) override;

            /**
             * @brief Thread-safe append by moving another FIFO's full contents.
             * @param other FIFO rvalue whose contents will be moved into this SharedFIFO.
             * @return true if the write succeeded, false if the SharedFIFO is closed or in error.
             * @details Acquires the internal mutex, checks `m_closed` and `m_error`,
             * then delegates to the base `FIFO::Write(FIFO&&)` to perform an
             * efficient move/steal of `other`'s contents. Notifies waiting readers
             * after the write. Marked `noexcept` to reflect the base rvalue overload.
             */
            bool Write(FIFO&& other) noexcept override;

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data String to append to the FIFO.
			 * @return true if written, false if closed.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
            bool Write(const std::string& data);

			/**
			 * @brief Thread-safe clear of all buffer contents.
			 * @see FIFO::Clear()
			 */
            void Clear() noexcept;

			/**
			 * @brief Thread-safe clean of buffer data from start to read position.
			 * @see FIFO::Clean()
			 */
            void Clean() noexcept;

			/**
			 * @brief Thread-safe seek operation.
			 * @details Notifies waiting readers after seeking.
			 * @see FIFO::Seek()
			 */
            void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept;
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
