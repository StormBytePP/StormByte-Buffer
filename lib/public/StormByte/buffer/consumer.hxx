#pragma once

#include <StormByte/buffer/shared_fifo.hxx>

#include <memory>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
    /**
     * @class Consumer
     * @brief Read-only interface for consuming data from a shared FIFO buffer.
     *
     * @par Overview
     *  Consumer provides a read-only interface to a SharedFIFO buffer.
     *  Multiple Consumer instances can share the same underlying buffer,
     *  allowing multiple consumers to read data concurrently in a thread-safe manner.
     *  Consumers can only be created through a Producer instance.
     *
     * @par Thread safety
     *  All read operations are thread-safe as they delegate to the underlying
     *  SharedFIFO which is fully thread-safe. Multiple consumers can safely
     *  read from the same buffer concurrently.
     *
     * @par Blocking behavior
     *  - @ref Read() blocks until the requested number of bytes are available
     *    or the buffer becomes unreadable (closed or error). If count is 0, returns
     *    all available data from the current read position without blocking.
     *  - @ref Extract() blocks until the requested number of bytes are available
     *    or the buffer becomes unreadable (closed or error). If count is 0, returns
     *    all available data immediately and clears the buffer.
     *
     * @par Producer-Consumer relationship
     *  Consumer instances cannot be created directly. They must be obtained from
     *  a Producer using Producer::Consumer(). This ensures proper buffer sharing
     *  between producers and consumers.
     *
     * @see Producer
     */
    class STORMBYTE_BUFFER_PUBLIC Consumer {
		friend class Producer;
        public:
            /**
             * @brief Copy constructor.
             * @details Copies the Consumer instance, sharing the same underlying buffer.
             *          Both instances will read from the same SharedFIFO and share the
             *          read position state.
             */
            Consumer(const Consumer&) = default;

            /**
             * @brief Move constructor.
             * @details Transfers ownership of the buffer from the moved-from Consumer.
             */
            Consumer(Consumer&&) = default;

            /**
             * @brief Destructor.
             */
            virtual ~Consumer() = default;

			/**
             * @brief Copy assignment operator.
             * @return Reference to this Consumer.
             */
            Consumer& operator=(const Consumer&) = default;

			/**
             * @brief Move assignment operator.
             * @return Reference to this Consumer.
             */
            Consumer& operator=(Consumer&&) = default;

			bool operator==(const Consumer& other) const noexcept = delete;
			bool operator!=(const Consumer& other) const noexcept = delete;

			/**
			 * @brief Get the number of bytes available for non-blocking read.
			 * @return The number of bytes that can be read from the current read position
			 *         without blocking.
			 * @details Returns the amount of data available for immediate Read() operations.
			 *          Useful for checking if data is available before attempting a blocking read.
			 * @see SharedFIFO::AvailableBytes(), Size(), Read()
			 */
			inline std::size_t AvailableBytes() const noexcept { return m_buffer->AvailableBytes(); }

			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see SharedFIFO::Size(), Empty()
			 */
			inline std::size_t Size() const noexcept { return m_buffer->Size(); }

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size()
			 */
			inline bool Empty() const noexcept { return m_buffer->Empty(); }

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data and resets positions. Affects all consumers
			 *          sharing this buffer.
			 * @see SharedFIFO::Clear()
			 */
			inline void Clear() noexcept { m_buffer->Clear(); }

			/**
			 * @brief Non-destructive read from the buffer (blocks until data available).
			 * @param count Number of bytes to read; 0 reads all available without blocking.
			 * @return Expected containing a vector with the requested bytes, or an error.
			 * @details **Blocks** until count bytes available or buffer becomes unreadable
			 *          (closed or error) (if count > 0). Data remains in buffer and can be
			 *          re-read using Seek().
			 * @see SharedFIFO::Read(), Extract(), Seek(), IsReadable()
			 */

			inline ExpectedData<InsufficientData> Read(std::size_t count = 0) { return m_buffer->Read(count); }
			
			/**
			* @brief Destructive read that removes data from the buffer (blocks until data available).
			* @param count Number of bytes to extract; 0 extracts all available without blocking.
			* @return Expected containing a vector with the extracted bytes, or an error.
			* @details **Blocks** until count bytes available or buffer becomes unreadable
			*          (closed or error) (if count > 0). Removes data from buffer.
			*          Multiple consumers share data fairly.
			* @see SharedFIFO::Extract(), Read(), IsReadable()
			*/
			inline ExpectedData<InsufficientData> Extract(std::size_t count = 0) { return m_buffer->Extract(count); }
			
			/**
			 * @brief Check if the buffer is readable (not in error state).
			 * @return true if readable, false if buffer is in error state.
			 * @details When not readable, blocked Read()/Extract() calls wake up and return
			 *          an error. A buffer becomes unreadable via SetError().
			 * @see SetError(), IsWritable(), EoF()
			 */
			inline bool IsReadable() const noexcept { return m_buffer->IsReadable(); }

			/**
			 * @brief Check if the buffer is writable (not closed and not in error state).
			 * @return true if writable, false if closed or in error state.
			 * @details While a consumer cannot write, it might be useful to know
			 *          if it can expect further data to arrive or not. A buffer becomes
			 *          unwritable via Close() or SetError().
			 * @see Close(), SetError(), IsReadable()
			 */
			inline bool IsWritable() const noexcept { return m_buffer->IsWritable(); }

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Position::Absolute or Position::Relative.
			 * @details Changes where subsequent Read() operations start. Position is
			 *          clamped to valid range. Does not affect stored data.
			 * @see SharedFIFO::Seek(), Read()
			 */
			inline void Seek(const std::size_t& position, const Position& mode) { m_buffer->Seek(position, mode); }

			/**
			 * @brief Removes count bytes from the read position.
			 * @param count Number of bytes to skip; 0 is a noop.
			 * @details Advances the read position by count bytes without reading data.
			 *          Clamped to AvailableBytes(). Does not affect stored data.
			 * @see SharedFIFO::Skip(), Read()
			 */
			inline void Skip(const std::size_t& count) { m_buffer->Skip(count); }

			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is unreadable and no bytes available, false otherwise.
			 * @details Returns true when IsReadable() is false AND AvailableBytes() == 0,
			 *          indicating no more data can be read from this buffer.
			 * @see IsReadable(), IsWritable(), AvailableBytes()
			 */
			inline bool EoF() const noexcept { return m_buffer->EoF(); }

			/**
			 * @brief Read all available data until end-of-file (non-destructive).
			 *
			 * This method repeatedly performs non-destructive `Read()` operations and
			 * accumulates the returned bytes into a fresh `Buffer::FIFO` which is
			 * returned by value. The call will continue until the underlying buffer
			 * reaches end-of-file (see `Consumer::EoF()`) or a read error occurs.
			 *
			 * Behavioural guarantees:
			 * - Non-destructive (data preserved): the bytes stored in the underlying
			 *   buffer are NOT removed by this call. The consumer's read position,
			 *   however, is advanced to the end of the stream as the data is read.
			 *   In other words, `Consumer::Size()` (total stored bytes) will remain
			 *   unchanged but subsequent non-destructive reads will start from the
			 *   end-of-stream (so `AvailableBytes()` may report 0 after completion).
			 * - Blocking: this method may block internally while waiting for data from
			 *   producers. It will return when the buffer is closed and all available
			 *   data has been read, or when a read error is observed.
			 * - Partial results on error: if a read error occurs, the returned `FIFO`
			 *   contains any bytes successfully read before the error was detected.
			 *
			 * Use this when you want a snapshot of the full stream without consuming
			 * the data from the shared buffer.
			 *
			 * @return A `Buffer::FIFO` containing the concatenated bytes read until EOF
			 *         (or until an error occurred).
			 */
			Buffer::FIFO ReadUntilEoF();

			/**
			 * @brief Extract all available data until end-of-file (destructive).
			 *
			 * This method repeatedly performs destructive `Extract()` operations and
			 * accumulates the returned bytes into a fresh `Buffer::FIFO` which is
			 * returned by value. The call will continue until the underlying buffer
			 * reaches end-of-file (see `Consumer::EoF()`) or a read error occurs.
			 *
			 * Behavioural guarantees:
			 * - Destructive: bytes returned by this call are removed from the
			 *   underlying buffer. After successful completion the original
			 *   `Consumer::Size()` will reflect that the extracted bytes have been
			 *   removed (typically zero if the buffer was fully consumed). The
			 *   consumer's read position will be adjusted accordingly (typically to
			 *   the start/end consistent with an empty buffer).
			 * - Blocking: this method may block internally while waiting for data from
			 *   producers. It will return when the buffer is closed and all available
			 *   data has been extracted, or when a read error is observed.
			 * - Partial results on error: if a read error occurs, the returned `FIFO`
			 *   contains any bytes successfully extracted before the error was detected.
			 *
			 * Use this when you want to consume the entire stream and obtain its
			 * contents in a single contiguous `FIFO` instance.
			 *
			 * @return A `Buffer::FIFO` containing the concatenated bytes extracted
			 *         until EOF (or until an error occurred).
			 */
			Buffer::FIFO ExtractUntilEoF();

        private:
            /** @brief Shared pointer to the underlying thread-safe FIFO buffer. */
            std::shared_ptr<SharedFIFO> m_buffer { std::make_shared<SharedFIFO>() };

			/**
             * @brief Construct a Consumer with an existing SharedFIFO buffer.
             * @param buffer Shared pointer to the SharedFIFO buffer to consume from.
             * @details Private constructor only accessible by Producer (friend class).
             *          Creates a new Consumer instance that shares the given buffer.
             *          Consumers cannot be created directly; use Producer::Consumer()
             *          to obtain a Consumer instance.
             */
            inline Consumer(std::shared_ptr<SharedFIFO> buffer): m_buffer(std::move(buffer)) {}
    };
}
