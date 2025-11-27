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
            explicit SharedFIFO(std::size_t capacity = 0) noexcept : FIFO(capacity) {}

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
            /**
             * @brief Thread-safe version of FIFO::Capacity().
             * @return The current allocated capacity in bytes.
             * @see FIFO::Capacity()
             */
            std::size_t Capacity() const noexcept override;

            /**
             * @brief Thread-safe version of FIFO::Clear().
             * @details Clears all data and notifies waiting threads.
             * @see FIFO::Clear()
             */
            void Clear() noexcept override;

            /**
             * @brief Thread-safe version of FIFO::Reserve().
             * @param newCapacity Minimum capacity to ensure (in bytes).
             * @see FIFO::Reserve()
             */
            void Reserve(std::size_t newCapacity) override;

            /**
             * @brief Thread-safe version of FIFO::Write().
             * @param data Byte vector to append.
             * @details Notifies waiting consumers after writing.
             * @see FIFO::Write()
             */
            bool Write(const std::vector<std::byte>& data) override;

            /**
             * @brief Thread-safe version of FIFO::Write().
             * @param data String to append.
             * @details Notifies waiting consumers after writing.
             * @see FIFO::Write()
             */
            bool Write(const std::string& data);

            /**
             * @brief Thread-safe blocking version of FIFO::Read().
             * @param count Number of bytes to read; 0 reads all available without blocking.
             * @return A vector containing the requested bytes.
             * @details **Blocks** until count bytes are available or buffer is closed.
             *          If count=0, returns immediately with available data.
             * @see FIFO::Read(), Consumer::Read()
             */
            ExpectedData<InsufficientData> Read(std::size_t count = 0) override;

            /**
             * @brief Thread-safe blocking version of FIFO::Extract().
             * @param count Number of bytes to extract; 0 extracts all available without blocking.
             * @return A vector containing the extracted bytes.
             * @details **Blocks** until count bytes are available or buffer is closed.
             *          If count=0, returns immediately with available data.
             * @see FIFO::Extract(), Consumer::Extract()
             */
            ExpectedData<InsufficientData> Extract(std::size_t count = 0) override;
            /** @} */

            /**
             * @brief Thread-safe version of FIFO::Seek().
             * @param position The offset value to apply.
             * @param mode Position::Absolute or Position::Relative.
             * @details Notifies waiting readers after seeking.
             * @see FIFO::Seek()
             */
            void Seek(const std::size_t& position, const Position& mode) override;

            /**
             * @brief Close the FIFO and notify waiters.
             * @details Sets the closed flag. Further writes are ignored. Wakes all
             *          threads blocked in @ref Read() or @ref Extract().
             */
            void Close() noexcept;

        private:
            /**
             * @brief Wait until at least @p n bytes are available from the current read position (or closed).
             * @param n Number of bytes to wait for; if 0, returns immediately.
             * @param lock The caller-held unique_lock for the internal mutex; the
             *        method will wait using this lock and return with it still held.
             * @note Wakes and returns when @ref Close() is called, even if the
             *       requested @p n bytes are not available.
             */
            void Wait(std::size_t n, std::unique_lock<std::mutex>& lock);

            /** @brief Internal mutex guarding all state mutations and reads. */
            mutable std::mutex m_mutex;
            /** @brief Condition variable used to block until data is available or closed. */
            std::condition_variable m_cv;
    };
}
