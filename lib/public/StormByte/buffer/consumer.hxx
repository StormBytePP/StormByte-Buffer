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
	class STORMBYTE_BUFFER_PUBLIC Consumer final: public ReadOnly {
		friend class Producer;
		public:
			/**
			 * @brief Copy constructor.
			 * @param other Source Consumer to copy from.
			 * @details Copies the Consumer instance, sharing the same underlying buffer.
			 *          Both instances will read from the same SharedFIFO and share the
			 *          read position state.
			 */
			inline Consumer(const Consumer& other) noexcept {
				m_buffer = other.m_buffer;
			}

			/**
			 * @brief Move constructor.
			 * @param other Source Consumer to move from.
			 * @details Transfers ownership of the buffer from the moved-from Consumer.
			 */
			inline Consumer(Consumer&& other) noexcept {
				m_buffer = std::move(other.m_buffer);
			}

			/**
			 * @brief Destructor.
			 */
			~Consumer() noexcept										= default;

			/**
			 * @brief Copy assignment operator.
			 * @return Reference to this Consumer.
			 */
			inline Consumer& operator=(const Consumer& other) noexcept {
				if (this != &other)
					m_buffer = other.m_buffer;
				return *this;
			}

			/**
			 * @brief Move assignment operator.
			 * @param other Source Consumer to move from.
			 * @return Reference to this Consumer.
			 */
			inline Consumer& operator=(Consumer&& other) noexcept {
				if (this != &other)
					m_buffer = std::move(other.m_buffer);
				return *this;
			}

			/**
			 * @brief Equality comparison.
			 * @return true if both Consumers share the same underlying buffer.
			 */
			inline bool operator==(const Consumer& other) const noexcept {
				return m_buffer.get() == other.m_buffer.get();
			}

			/**
			 * @brief Inequality comparison.
			 * @return true if Consumers have different underlying buffers.
			 */
			inline bool operator!=(const Consumer& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Gets available bytes for reading.
			 * @return Number of bytes available from the current read position.
			 */
			inline std::size_t 											AvailableBytes() const noexcept override {
				return m_buffer->AvailableBytes();
			}

			/**
			 * @brief Clean buffer data from start to read position.
			 * @see Size(), Empty()
			 */
			inline void 												Clean() noexcept override {
				m_buffer->Clean();
			}

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions,
			 *          and restores capacity to the initial value requested in the constructor.
			 * @see Size(), Empty()
			 */
			inline void 												Clear() noexcept override {
				m_buffer->Clear();
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

			/**
			 * @brief Drop bytes in the buffer
			 * @param count Number of bytes to drop.
			 * @return true if the bytes were successfully dropped, false otherwise.
			 * @see Read()
			 */
			inline bool 												Drop(const std::size_t& count) noexcept override {
				return m_buffer->Drop(count);
			}

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size()
			 */
			inline bool 												Empty() const noexcept override {
				return m_buffer->Empty();
			}

			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			inline bool 												EoF() const noexcept override {
				return m_buffer->EoF();
			}

			/**
			 * @brief Destructive read that removes data from the buffer into an existing vector.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer Vector to fill with extracted bytes; resized as needed.
			 * @return bool indicating success or failure.
			 * @note For base class is the same than Read
			 */
			inline bool 												Extract(const std::size_t& count, DataType& outBuffer) noexcept override {
				return m_buffer->Extract(count, outBuffer);
			}

			/**
			 * @brief Destructive read that removes data from the buffer into a FIFO.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer WriteOnly to fill with extracted bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 												Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept {
				return m_buffer->Extract(count, outBuffer);
			}

			/**
			 * @brief Read all bytes until end-of-file into an existing buffer.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 */
			inline void													ExtractUntilEoF(DataType& outBuffer) noexcept override {
				m_buffer->ExtractUntilEoF(outBuffer);
			}

			/**
			 * @brief Read all bytes until end-of-file into a WriteOnly buffer.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 */
			inline void													ExtractUntilEoF(WriteOnly& outBuffer) noexcept override {
				m_buffer->ExtractUntilEoF(outBuffer);
			}

			/**
			 * @brief Check if the buffer is readable (not in error state).
			 * @return true if readable, false if buffer is in error state.
			 * @details A buffer becomes unreadable when SetError() is called. Use in
			 *          combination with AvailableBytes() to check if there is data
			 *          pending to read.
			 * @see SetError(), IsWritable(), AvailableBytes(), EoF()
			 */
			inline bool 												IsReadable() const noexcept override {
				return m_buffer->IsReadable();
			}

			/**
			 * @brief Check if the buffer is writable (not closed and not in error state).
			 * @return true if writable, false if closed or in error state.
			 * @details A buffer becomes unwritable when Close() or SetError() is called.
			 * @see Close(), SetError(), IsReadable()
			 */
			inline bool 												IsWritable() const noexcept {
				return m_buffer->IsWritable();
			}

			/**
			 * @brief Check if the buffer is in an error state.
			 * @return true if the buffer is in error state, false otherwise.
			 */
			inline bool 												HasError() const noexcept {
				return m_buffer->HasError();
			}

			/**
			 * @brief Read bytes into an existing buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 												Read(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return m_buffer->Read(count, outBuffer);
			}

			/**
			 * @brief Read bytes into a WriteOnly buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 												Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return m_buffer->Read(count, outBuffer);
			}

			/**
			 * @brief Read all bytes until end-of-file into an existing buffer.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 */
			inline void													ReadUntilEoF(DataType& outBuffer) const noexcept override {
				m_buffer->ReadUntilEoF(outBuffer);
			}

			/**
			 * @brief Read all bytes until end-of-file into a WriteOnly buffer.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 */
			inline void													ReadUntilEoF(WriteOnly& outBuffer) const noexcept override {
				m_buffer->ReadUntilEoF(outBuffer);
			}

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return bool indicating success or failure.
			 * @details Similar to Read(), but does not advance the read position.
			 *          Allows inspecting upcoming data without consuming it.
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call returns all available bytes. If no
			 *            bytes are available, a `ReadError` is returned.
			 *          - If `count > 0`: the call returns exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, a
			 *            `ReadError` is returned.
			 *
			 * @see Read(), Seek()
			 */
			inline bool 												Peek(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return m_buffer->Peek(count, outBuffer);
			}

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return bool indicating success or failure.
			 * @details Similar to Read(), but does not advance the read position.
			 *          Allows inspecting upcoming data without consuming it.
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call returns all available bytes. If no
			 *            bytes are available, a `ReadError` is returned.
			 *          - If `count > 0`: the call returns exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, a
			 *            `ReadError` is returned.
			 *
			 * @see Read(), Seek()
			 */
			inline bool 												Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return m_buffer->Peek(count, outBuffer);
			}

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Unused for base class; included for API consistency.
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
			 * If Position is set to Absolute and offset is negative the operation is noop
			 */
			inline void 												Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
				m_buffer->Seek(offset, mode);
			}

			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Capacity(), Empty()
			 */
			inline std::size_t 											Size() const noexcept override {
				return m_buffer->Size();
			}

		private:
			std::shared_ptr<SharedFIFO> m_buffer;						///< Underlying shared FIFO buffer.

			/**
			 * @brief Construct a Consumer with an existing SharedFIFO buffer.
			 * @param buffer Shared pointer to the SharedFIFO buffer to consume from.
			 * @details Private constructor only accessible by Producer (friend class).
			 *          Creates a new Consumer instance that shares the given buffer.
			 *          Consumers cannot be created directly; use Producer::Consumer()
			 *          to obtain a Consumer instance.
			 */
			inline Consumer(std::shared_ptr<SharedFIFO> buffer): m_buffer(buffer) {}
	};
}
