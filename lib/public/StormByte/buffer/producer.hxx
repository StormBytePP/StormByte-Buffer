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
	class STORMBYTE_BUFFER_PUBLIC Producer final: public WriteOnly {
		public:
			/**
			 * @brief Construct a Producer with a new SharedFIFO buffer.
			 * @details Creates a new Producer instance with its own underlying
			 *          SharedFIFO buffer for writing data.
			 */
			inline Producer() noexcept: m_buffer(std::make_shared<SharedFIFO>()) {};

			/**
			 * @brief Construct a Producer with an existing SharedFIFO buffer.
			 * @param buffer Shared pointer to the SharedFIFO buffer to produce to.
			 * @details Private constructor only accessible by Consumer (friend class).
			 *          Creates a new Producer instance that shares the given buffer.
			 */
			inline Producer(std::shared_ptr<SharedFIFO> buffer): m_buffer(buffer) {}

			/**
			 * @brief Construct a Producer from a Consumer's buffer.
			 * @details Creates a new Producer instance sharing the same underlying
			 *          SharedFIFO buffer as the provided Consumer.
			 */
			inline Producer(const Consumer& consumer): m_buffer(consumer.m_buffer) {}

			/**
			 * @brief Copy constructor.
			 * @param other Source Producer to copy from.
			 * @details Copies the Producer instance, sharing the same underlying buffer.
			 *          Both instances will write to the same SharedFIFO.
			 */
			inline Producer(const Producer& other) noexcept {
				m_buffer = other.m_buffer;
			}

			/**
			 * @brief Move constructor.
			 * @param other Source Producer to move from.
			 * @details Transfers ownership of the buffer from the moved-from Producer.
			 */
			inline Producer(Producer&& other) noexcept {
				m_buffer = std::move(other.m_buffer);
			}

			/**
			 * @brief Destructor.
			 */
			~Producer() noexcept										= default;

			/**
			 * @brief Copy assignment operator.
			 * @param other Source Producer to copy from.
			 * @return Reference to this Producer.
			 */
			inline Producer& operator=(const Producer& other) noexcept {
				if (this != &other)
					m_buffer = other.m_buffer;
				return *this;
			}

			/**
			 * @brief Move assignment operator.
			 * @return Reference to this Producer.
			 */
			inline Producer& operator=(Producer&& other) noexcept {
				if (this != &other)
					m_buffer = std::move(other.m_buffer);
				return *this;
			}

			/**
			 * @brief Equality comparison.
			 * @return true if both Producers share the same underlying buffer.
			 */
			inline bool operator==(const Producer& other) const noexcept {
				return m_buffer.get() == other.m_buffer.get();
			}

			/**
			 * @brief Inequality comparison.
			 * @return true if Producers have different underlying buffers.
			 */
			inline bool operator!=(const Producer& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Thread-safe close for further writes.
			 * @details Marks buffer as closed, notifies all waiting threads. Subsequent writes
			 *          are ignored. The buffer remains readable until all data is consumed.
			 * @see FIFO::Close(), IsWritable()
			 */
			inline void 												Close() noexcept {
				m_buffer->Close();
			}

			inline bool 												IsWritable() const noexcept override {
				return m_buffer->IsWritable();
			}

			/**
			 * @brief Thread-safe error state setting.
			 * @details Marks buffer as erroneous (unreadable and unwritable), notifies all
			 *          waiting threads. Subsequent writes are ignored and reads will fail.
			 * @see FIFO::SetError(), IsReadable(), IsWritable()
			 */
			inline void 												SetError() noexcept {
				m_buffer->SetError();
			}

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<WriteError> 							Write(const std::size_t& count, const DataType& data) noexcept override {
				return m_buffer->Write(count, data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<WriteError> 							Write(const std::size_t& count, DataType&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<Error> 									Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return m_buffer->Write(count, data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<Error> 									Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}

			/**
			 * @brief Write a string to the buffer.
			 * @param data String to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::size_t&, const DataType&), IsClosed()
			 */
			inline ExpectedVoid<WriteError>								Write(const std::string& data) noexcept override {
				return m_buffer->Write(data);
			}

			using WriteOnly::Write;
			
			/**
			 * @brief Create a Consumer for reading from this Producer's buffer.
			 * @return A Consumer instance sharing the underlying buffer.
			 * @details Enables producer-consumer pattern. Consumer has read-only access
			 *          to the same SharedFIFO buffer this Producer writes to.
			 * @see Consumer
			 */
			inline class Consumer										Consumer() {
				return { m_buffer };
			}
			
		protected:
			std::shared_ptr<SharedFIFO> m_buffer;
	};
}
