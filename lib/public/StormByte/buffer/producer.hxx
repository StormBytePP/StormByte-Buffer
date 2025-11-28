#pragma once

#include <StormByte/buffer/consumer.hxx>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for different buffer types
 * including FIFO ring buffers, thread-safe shared buffers, and producer-consumer patterns.
 */
namespace StormByte::Buffer {
    /**
     * @class Producer
     * @brief Producer interface for writing data to a shared FIFO buffer.
     *
     * @par Overview
     *  Producer provides a write-only interface to a SharedFIFO buffer.
     *  Multiple Producer instances can share the same underlying buffer,
     *  allowing multiple producers to write data concurrently in a thread-safe manner.
     *
     * @par Thread safety
     *  All write operations are thread-safe as they delegate to the underlying
     *  SharedFIFO which is fully thread-safe.
     */
    class STORMBYTE_BUFFER_PUBLIC Producer final {
        public:
            /**
             * @brief Construct a Producer with a new SharedFIFO buffer.
             * @param buffer Shared pointer to the underlying SharedFIFO buffer. If not provided,
             *               a new SharedFIFO is created.
             * @details Creates a new Producer instance with its own SharedFIFO buffer.
             *          The buffer can be shared with Consumer instances.
             */
            inline Producer(std::shared_ptr<SharedFIFO> buffer = std::make_shared<SharedFIFO>()) noexcept: m_buffer(std::move(buffer)) {};

            /**
             * @brief Copy constructor.
             * @details Copies the Producer instance, sharing the same underlying buffer.
             *          Both instances will write to the same SharedFIFO.
             */
            Producer(const Producer&) = default;

            /**
             * @brief Copy assignment operator.
             * @return Reference to this Producer.
             */
            Producer& operator=(const Producer&) = default;

            /**
             * @brief Move constructor.
             * @details Transfers ownership of the buffer from the moved-from Producer.
             */
            Producer(Producer&&) = default;

            /**
             * @brief Move assignment operator.
             * @return Reference to this Producer.
             */
            Producer& operator=(Producer&&) = default;

            /**
             * @brief Destructor.
             */
            ~Producer() = default;

			/**
			 * @brief Close the buffer for further writes.
			 * @details Marks buffer as closed. Subsequent writes ignored. Wakes waiting consumers.
			 * @see SharedFIFO::Close(), Consumer::IsClosed()
			 */
			inline void Close() noexcept { m_buffer->Close(); }

			/**
			 * @brief Write bytes to the buffer.
			 * @param data Byte vector to append.
			 * @details Appends data to buffer. Ignored if closed. Notifies waiting consumers.
			 * @see SharedFIFO::Write(), Close()
			 */
			inline void Write(const std::vector<std::byte>& data) { m_buffer->Write(data); }
			
			/**
			 * @brief Write a string to the buffer.
			 * @param data String to append.
			 * @details Converts string to bytes and appends. Ignored if closed.
			 * @see SharedFIFO::Write(), Close()
			 */
			inline void Write(const std::string& data) { m_buffer->Write(data); }

			/**
			 * @brief Create a Consumer for reading from this Producer's buffer.
			 * @return A Consumer instance sharing the underlying buffer.
			 * @details Enables producer-consumer pattern. Consumer has read-only access
			 *          to the same SharedFIFO buffer this Producer writes to.
			 * @see Consumer
			 */
			inline class Consumer Consumer() {
				return { m_buffer };
			}
			
		private:
            /** @brief Shared pointer to the underlying thread-safe FIFO buffer. */
            std::shared_ptr<SharedFIFO> m_buffer;
    };
}
