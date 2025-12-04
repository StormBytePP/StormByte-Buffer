#pragma once

#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/buffer/typedefs.hxx>

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
 	 * @class Forwarder
 	 * @brief Adapter that exposes a `SharedFIFO`-compatible interface while delegating
 	 *        actual I/O to user-provided callables, with internal buffering.
 	 *
 	 * @details `Forwarder` publicly inherits from `SharedFIFO` and implements the
 	 *          same producer/consumer API surface. It maintains an internal buffer
 	 *          and only calls external functions when the buffer cannot satisfy
 	 *          the operation.
 	 *
	 *          Key behaviours:
	 *          - `Read(count)` first checks the internal buffer. If sufficient data
	 *            is available, it returns from the buffer without calling the external
	 *            read function. If more data is needed, it calls the external function
	 *            for the remaining bytes and combines both sources.
	 *          - `Write(...)` stores data in the internal buffer. Data remains buffered
	 *            and can be consumed by subsequent read operations.
	 *          - `Extract(count)` behaves like `Read()` but removes data from the buffer.
	 *          - `Clear()`, `Clean()`, `Seek()`, and `Skip()` operate on the internal buffer.
 	 *
 	 *          Example: `Read(2)` with 1 byte in buffer will read 1 byte from the
 	 *          external function and return both bytes combined.
 	 *
 	 *          Important notes:
 	 *          - `Read(0)` returns an error because the forwarder cannot determine how
 	 *            many bytes the external reader would provide. Always specify a count.
 	 *          - When a handler is not supplied for an operation the forwarder uses
 	 *            error functions that return appropriate errors via `StormByte::Unexpected`.
 	 *
 	 * @note Copy and move operations are deleted to avoid accidental sharing
 	 *       of handler state; create a new `Forwarder` if you need a different
 	 *       configuration.
	 */
	class STORMBYTE_BUFFER_PUBLIC Forwarder: public SharedFIFO {
		public:
			/**
			 * @brief Construct a Forwarder that only supports read operations.
			 * @param readFunc Callable used to service `Read()` requests. If omitted
			 *                 a default noop read function that returns a `ReadError`
			 *                 will be used.
			 */
			Forwarder(const ExternalReadFunction& readFunc) noexcept;

			/**
			 * @brief Construct a Forwarder that only supports write operations.
			 * @param writeFunc Callable used to service `Write()` requests. If omitted
			 *                  a default noop write function that returns a `WriteError`
			 *                  will be used.
			 */
			Forwarder(const ExternalWriteFunction& writeFunc) noexcept;

			/**
			 * @brief Construct a Forwarder with both read and write handlers.
			 * @param readFunc  Callable used for reads.
			 * @param writeFunc Callable used for writes.
			 */
			Forwarder(const ExternalReadFunction& readFunc, const ExternalWriteFunction& writeFunc) noexcept;
			Forwarder(const Forwarder& other)		= delete;
			Forwarder(Forwarder&& other) noexcept	= delete;
			virtual ~Forwarder()						= default;
			Forwarder& operator=(const Forwarder& other) = delete;
			Forwarder& operator=(Forwarder&& other) noexcept = delete;

			/**
			 * @brief Read operation with internal buffering.
			 * @param count Number of bytes to read. Must be greater than 0.
			 * @return `ExpectedData<ReadError>` containing the bytes on success or an
			 *         `ReadError` describing a failure.
			 * @details First checks the internal buffer. If sufficient data exists,
			 *          returns from buffer without calling external function. Otherwise,
			 *          reads remaining needed bytes from external function and combines
			 *          buffer data with external data.
			 * @note `count` must be > 0. Reading 0 bytes returns an error because the
			 *       forwarder cannot determine external reader size.
			 */
			ExpectedData<ReadError> Read(const std::size_t& count = 0) const override;

			/**
			 * @brief Destructive read operation with internal buffering.
			 * @param count Number of bytes to extract. Must be greater than 0.
			 * @return `ExpectedData<ReadError>` with the requested bytes or an error.
			 * @details Like `Read()` but removes data from the internal buffer after reading.
			 *          First checks buffer, then calls external function if needed.
			 * @note `count` must be > 0. Extracting 0 bytes returns an error.
			 */
			ExpectedData<ReadError> Extract(const std::size_t& count = 0) override;

			/**
			 * @brief Write bytes to the internal buffer.
			 * @param data Bytes to write.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
			 * @details Stores data in the internal buffer. The external write function
			 *          is not called; data remains buffered until explicitly flushed
			 *          or read operations consume it.
			 */
			ExpectedVoid<WriteError> Write(const std::vector<std::byte>& data) override;

			/**
			 * @brief Forward the contents of another `FIFO` to the write handler.
			 * @param other FIFO whose contents will be forwarded.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
			 */
			ExpectedVoid<WriteError> Write(const FIFO& other) override;

			/**
			 * @brief Forward the contents of an rvalue `FIFO` to the write handler.
			 * @param other FIFO to move from.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
			 */
			ExpectedVoid<WriteError> Write(FIFO&& other) noexcept override;

			/**
			 * @brief Convenience write overload for strings.
			 * @param data String whose bytes will be forwarded to the write handler.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
			 */
			ExpectedVoid<WriteError> Write(const std::string& data) override;

			/**
			 * @brief Clear the internal buffer.
			 * @details Removes all data from the internal buffer.
			 */
			void Clear() noexcept override;

			/**
			 * @brief Clean the internal buffer (removes read data).
			 */
			void Clean() noexcept override;

			/**
			 * @brief Seek within the internal buffer.
			 */
			void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Peek into the internal buffer without advancing read position.
			 * @param count Number of bytes to peek. Must be greater than 0.
			 * @return `ExpectedData<ReadError>` with the requested bytes or an error.
			 * @details Inspects upcoming data in the internal buffer without consuming it.
			 *          Does not call external read function.
			 * @note `count` must be > 0. Peeking 0 bytes returns an error.
			 */
			ExpectedData<ReadError> Peek(const std::size_t& count) const noexcept override;

			/**
			 * @brief Skip bytes in the internal buffer.
			 */
			void Skip(const std::size_t& count) noexcept override;

			/**
			 * @brief Return an `ExternalReadFunction` that always yields a `ReadError`.
			 * @return Callable suitable for use as a read handler that returns an error.
			 */
			static ExternalReadFunction ErrorReadFunction() noexcept;

			/**
			 * @brief Return a default read handler used when no read function supplied.
			 * @return Callable that returns a `ReadError` explaining no handler is defined.
			 */
			static ExternalReadFunction NoopReadFunction() noexcept;

			/**
			 * @brief Return an `ExternalWriteFunction` that always yields a `WriteError`.
			 * @return Callable suitable for use as a write handler that returns an error.
			 */
			static ExternalWriteFunction ErrorWriteFunction() noexcept;

			/**
			 * @brief Return a default write handler used when no write function supplied.
			 * @return Callable that returns a `WriteError` explaining no handler is defined.
			 */
			static ExternalWriteFunction NoopWriteFunction() noexcept;

		private:
			/**
			 * @brief Callable used to satisfy `Read()` requests.
			 *
			 * If the forwarder was constructed without a read handler this will be set
			 * to the `NoopReadFunction()` which returns an appropriate `ReadError`.
			 */
			ExternalReadFunction	 m_readFunction;

			/**
			 * @brief Callable used to satisfy `Write()` requests.
			 *
			 * If the forwarder was constructed without a write handler this will be set
			 * to the `NoopWriteFunction()` which returns an appropriate `WriteError`.
			 */
			ExternalWriteFunction	 m_writeFunction;
	};
			
}