#pragma once

#include <StormByte/buffer/consumer.hxx>

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
    class STORMBYTE_BUFFER_PUBLIC Producer {
        public:
            /**
             * @brief Construct a Producer with a new SharedFIFO buffer.
             * @details Creates a new Producer instance with its own underlying
			 *          SharedFIFO buffer for writing data.
             */
            inline Producer() noexcept: m_buffer(std::make_shared<SharedFIFO>()) {};

			/**
             * @brief Construct a Producer from a Consumer's buffer.
             * @details Creates a new Producer instance sharing the same underlying
			 *          SharedFIFO buffer as the provided Consumer.
             */
			inline Producer(const Consumer& consumer): m_buffer(consumer.m_buffer) {}

            /**
             * @brief Copy constructor.
             * @details Copies the Producer instance, sharing the same underlying buffer.
             *          Both instances will write to the same SharedFIFO.
             */
            Producer(const Producer&) = default;

            /**
             * @brief Destructor.
             */
            virtual ~Producer() = default;

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

			bool operator==(const Producer& other) const noexcept = delete;
			bool operator!=(const Producer& other) const noexcept = delete;

			/**
			 * @brief Close the buffer for further writes.
			 * @details Marks buffer as closed. Subsequent writes ignored. Wakes waiting consumers.
			 *          The buffer remains readable until all data is consumed.
			 * @see SharedFIFO::Close(), IsWritable()
			 */
			inline void Close() noexcept { m_buffer->Close(); }

			/**
			 * @brief Mark the buffer as erroneous, making it unreadable and unwritable.
			 * @details Sets the error state on the buffer. Subsequent writes will be ignored,
			 *          and consumers' read operations will fail. Wakes all waiting threads.
			 * @see SharedFIFO::SetError(), IsWritable(), Consumer::IsReadable()
			 */
			inline void SetError() noexcept { m_buffer->SetError(); }

			/**
			 * @brief Check if the buffer is writable (not closed and not in error state).
			 * @return true if writable, false if closed or in error state.
			 * @details A buffer becomes unwritable via Close() or SetError().
			 * @see Close(), SetError(), SharedFIFO::IsWritable()
			 */
			inline bool IsWritable() const noexcept { return m_buffer->IsWritable(); }

			/**
			 * @brief Write bytes to the buffer.
			 * @param data Byte vector to append.
			 * @details Appends data to buffer. Ignored if closed. Notifies waiting consumers.
			 * @see SharedFIFO::Write(), Close()
			 */
			inline bool Write(const std::vector<std::byte>& data) { return m_buffer->Write(data); }

			/**
			 * @brief Write another FIFO's contents to the buffer.
			 * @param other FIFO whose contents to append.
			 * @details Appends all data from `other` to the buffer. Ignored if closed.
			 * @see SharedFIFO::Write(), Close()
			 */
			inline bool Write(const FIFO& other) { return m_buffer->Write(other); }
			
			/**
			 * @brief Write a string to the buffer.
			 * @param data String to append.
			 * @details Converts string to bytes and appends. Ignored if closed.
			 * @see SharedFIFO::Write(), Close()
			 */
			inline bool Write(const std::string& data) { return m_buffer->Write(data); }

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
