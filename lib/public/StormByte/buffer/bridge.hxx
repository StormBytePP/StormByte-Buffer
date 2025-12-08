#pragma once

#include <StormByte/buffer/generic.hxx>

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
	 * @class Bridge
	 * @brief Simple pass-through adapter that forwards bytes from a read buffer to a write buffer.
	 */
	class STORMBYTE_BUFFER_PUBLIC Bridge final {
		public:
			/**
			 * @brief Construct a Bridge with both read and write handlers.
			 * @param readFunc  Callable used for reads.
			 * @param writeFunc Callable used for writes.
			 * @param chunk_size Size of each read/write chunk.
			 * @note The constructor is noexcept and stores the provided callables by
			 *       value into the base `Forwarder` implementation.
			 */
			inline Bridge(ReadOnly& in, WriteOnly& out, const std::size_t& chunk_size) noexcept:
				m_readHandler(in),
				m_writeHandler(out),
				m_chunk_size(chunk_size) {}

			/**
			 * @brief Copy constructor (deleted).
			 * @details Bridge is non-copyable because it owns or references external
			 *          handler callables; copying could lead to surprising sharing or
			 *          lifetime issues. Create a new `Bridge` instance explicitly
			 *          when you need a separate configuration.
			 */
			Bridge(const Bridge& other) 									= delete;

			/**
			 * @brief Move constructor
			 * @details Moving a `Bridge` is disallowed to avoid accidental transfer of
			 *          handler ownership and to keep semantics simple and explicit.
			 */
			inline Bridge(Bridge&& other) noexcept							= default;

			/**
			 * @brief Destructor.
			 * @details Defaulted destructor. `Bridge` performs no special cleanup beyond
			 *          what its base `Forwarder` and contained callables perform.
			 */
			~Bridge() 														= default;

			/**
			 * @brief Copy assignment (deleted).
			 */
			Bridge& operator=(const Bridge& other) 							= delete;

			/**
			 * @brief Move assignment (deleted).
			 */
			inline Bridge& operator=(Bridge&& other) noexcept 				= default;

			/**
			 * @brief Gets available bytes for reading.
			 * @return Number of bytes available from the current read position.
			 */
			inline std::size_t 												AvailableBytes() const noexcept {
				return m_readHandler.get().AvailableBytes();
			}

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions,
			 *          and restores capacity to the initial value requested in the constructor.
			 * @see Size(), Empty()
			 */
			inline void 													Clear() noexcept {
				m_readHandler.get().Clear();
			}

			/**
			 * @brief Drop bytes in the buffer
			 * @param count Number of bytes to drop.
			 * @return true if the bytes were successfully dropped, false otherwise.
			 * @see Read()
			 */
			inline bool 													Drop(const std::size_t& count) noexcept {
				return m_readHandler.get().Drop(count);
			}

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size()
			 */
			inline bool 													Empty() const noexcept {
				return m_readHandler.get().Empty();
			}

			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			inline bool 													EoF() const noexcept {
				return m_readHandler.get().EoF();
			}

			/**
			 * @brief Check if the buffer is readable.
			 * @return true if the buffer can be read from, false otherwise.
			 */
			inline bool 													IsReadable() const noexcept {
				return m_readHandler.get().IsReadable();
			}

			/**
			 * @brief Check if the buffer is writable.
			 * @return true if the buffer can accept write operations, false if closed or in error state.
			 */
			inline bool 													IsWritable() const noexcept {
				return m_writeHandler.get().IsWritable();
			}

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error if insufficient data.
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
			inline bool 													Peek(const std::size_t& count, DataType& outBuffer) const noexcept {
				return m_readHandler.get().Peek(count, outBuffer);
			}

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error if insufficient data.
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
			inline bool 													Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept {
				return m_readHandler.get().Peek(count, outBuffer);
			}

			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Capacity(), Empty()
			 */
			inline std::size_t 												Size() const noexcept {
				return m_readHandler.get().Size();
			}

			/**
			 * @brief Passthrough all bytes until end-of-file from read handler to write handler.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			bool 															Passthrough() noexcept;

			/**
			 * @brief Passthrough bytes from read handler to write handler.
			 * @param count Number of bytes to read
			 * @note If count is zero all available bytes will be processed.
			 * But this does not guarantee EoF, to read until EoF use Passthrough()
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			bool 															Passthrough(const std::size_t& count) noexcept;
			
		private:
			std::reference_wrapper<ReadOnly> m_readHandler;					///< Underlying read handler
			std::reference_wrapper<WriteOnly> m_writeHandler;				///< Underlying write handler
			std::size_t m_chunk_size;										///< Size of each read/write chunk
	};		
}