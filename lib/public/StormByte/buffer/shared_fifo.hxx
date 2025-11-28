#pragma once

#include <StormByte/buffer/fifo.hxx>

#include <condition_variable>
#include <mutex>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for different buffer types
 * including FIFO ring buffers, thread-safe shared buffers, and producer-consumer patterns.
 */
namespace StormByte::Buffer {
    /**
     * @class SharedFIFO
     * @brief Thread-safe FIFO built on top of @ref FIFO.
     *
     * @par Overview
     *  SharedFIFO wraps the non-thread-safe @ref FIFO with a mutex and a
     *  condition variable to provide safe concurrent access from multiple
     *  producer/consumer threads. It preserves the byte-oriented ring-buffer
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
             * @param capacity Initial number of slots to allocate in the ring buffer.
             *        Behaves like @ref FIFO: capacity may grow geometrically as needed.
             */
            SharedFIFO() noexcept = default;

            SharedFIFO(const SharedFIFO&) = delete;
            SharedFIFO& operator=(const SharedFIFO&) = delete;
            SharedFIFO(SharedFIFO&&) = delete;
            SharedFIFO& operator=(SharedFIFO&&) = delete;

            /**
             * @brief Virtual destructor.
             */
            virtual ~SharedFIFO() = default;

            /**
             * @name Thread-safe overrides
             * @{
             */
			void Close() noexcept override;

			/**
			 * @brief Thread-safe blocking read from the buffer.
			 * @param count Number of bytes to read; 0 reads all available immediately.
			 * @return A vector containing the requested bytes, or error.
			 * @details Blocks until @p count bytes are available from the current read position,
			 *          or until the FIFO is closed. If count == 0, returns immediately with
			 *          all available data. If closed before count bytes available, returns error.
			 * @see FIFO::Read(), Wait()
			 */
			ExpectedData<InsufficientData> Read(std::size_t count = 0) const override;

			/**
			 * @brief Thread-safe blocking extract from the buffer.
			 * @param count Number of bytes to extract; 0 extracts all available immediately.
			 * @return A vector containing the extracted bytes, or error.
			 * @details Blocks until @p count bytes are available, or until the FIFO is closed.
			 *          If count == 0, returns immediately with all available data and removes it.
			 *          If closed before count bytes available, returns error.
			 * @see FIFO::Extract(), Wait()
			 */
			ExpectedData<InsufficientData> Extract(std::size_t count = 0) override;

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return true if written, false if closed.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
			bool Write(const std::vector<std::byte>& data) override;

			/**
			 * @brief Thread-safe write to the buffer.
			 * @param data String to append to the FIFO.
			 * @return true if written, false if closed.
			 * @details Thread-safe version that notifies waiting readers after write.
			 * @see FIFO::Write()
			 */
			bool Write(const std::string& data) override;

			/**
			 * @brief Thread-safe clear of all buffer contents.
			 * @see FIFO::Clear()
			 */
			void Clear() noexcept override;

			/**
			 * @brief Thread-safe clean of buffer data from start to read position.
			 * @see FIFO::Clean()
			 */
			void Clean() noexcept override;

			/**
			 * @brief Thread-safe seek operation.
			 * @details Notifies waiting readers after seeking.
			 * @see FIFO::Seek()
			 */
			void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;
            /** @} */

        private:
            /**
             * @brief Wait until at least @p n bytes are available from the current read position (or closed).
             * @param n Number of bytes to wait for; if 0, returns immediately.
             * @param lock The caller-held unique_lock for the internal mutex; the
             *        method will wait using this lock and return with it still held.
             * @note Wakes and returns when @ref Close() is called, even if the
             *       requested @p n bytes are not available.
             */
            void Wait(std::size_t n, std::unique_lock<std::mutex>& lock) const;

            /** @brief Internal mutex guarding all state mutations and reads. */
            mutable std::mutex m_mutex;
            /** @brief Condition variable used to block until data is available or closed. */
            mutable std::condition_variable_any m_cv;
    };
}
