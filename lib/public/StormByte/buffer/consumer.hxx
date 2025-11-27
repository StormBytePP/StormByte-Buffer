#pragma once

#include <StormByte/buffer/shared_fifo.hxx>

#include <memory>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for different buffers
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
     *    or the buffer is closed. If count is 0, returns all available data
     *    from the current read position without blocking.
     *  - @ref Extract() blocks until the requested number of bytes are available
     *    or the buffer is closed. If count is 0, returns all available data
     *    immediately and clears the buffer.
     *
     * @par Producer-Consumer relationship
     *  Consumer instances cannot be created directly. They must be obtained from
     *  a Producer using Producer::Consumer(). This ensures proper buffer sharing
     *  between producers and consumers.
     *
     * @see Producer
     */
    class STORMBYTE_BUFFER_PUBLIC Consumer final {
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
             * @brief Copy assignment operator.
             * @return Reference to this Consumer.
             */
            Consumer& operator=(const Consumer&) = default;

            /**
             * @brief Move constructor.
             * @details Transfers ownership of the buffer from the moved-from Consumer.
             */
            Consumer(Consumer&&) = default;

            /**
             * @brief Move assignment operator.
             * @return Reference to this Consumer.
             */
            Consumer& operator=(Consumer&&) = default;

            /**
             * @brief Destructor.
             */
            ~Consumer() = default;

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
		 * @details **Blocks** until count bytes available or buffer closed (if count > 0).
		 *          Data remains in buffer and can be re-read using Seek().
		 * @see SharedFIFO::Read(), Extract(), Seek()
		 */
		inline ExpectedData<InsufficientData> Read(std::size_t count = 0) { return m_buffer->Read(count); }		/**
		 * @brief Destructive read that removes data from the buffer (blocks until data available).
		 * @param count Number of bytes to extract; 0 extracts all available without blocking.
		 * @return Expected containing a vector with the extracted bytes, or an error.
		 * @details **Blocks** until count bytes available or buffer closed (if count > 0).
		 *          Removes data from buffer. Multiple consumers share data fairly.
		 * @see SharedFIFO::Extract(), Read()
		 */
		inline ExpectedData<InsufficientData> Extract(std::size_t count = 0) { return m_buffer->Extract(count); }			/**
			 * @brief Check if the buffer is closed for further writes.
			 * @return true if closed, false otherwise.
			 * @details When closed, blocked Read()/Extract() calls wake up and return
			 *          available data. Consumers can still read remaining data.
			 * @see Producer::Close(), SharedFIFO::Close()
			 */
			inline bool IsClosed() const noexcept { return m_buffer->IsClosed(); }

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Position::Absolute or Position::Relative.
			 * @details Changes where subsequent Read() operations start. Position is
			 *          clamped to valid range. Does not affect stored data.
			 * @see SharedFIFO::Seek(), Read()
			 */
			inline void Seek(const std::size_t& position, const Position& mode) { m_buffer->Seek(position, mode); }

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
