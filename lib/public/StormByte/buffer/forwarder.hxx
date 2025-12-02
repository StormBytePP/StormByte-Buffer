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
 	 *        actual I/O to user-provided callables.
 	 *
 	 * @details `Forwarder` publicly inherits from `SharedFIFO` and implements the
 	 *          same producer/consumer API surface, but it does not own or buffer data
 	 *          internally. Instead, reads and writes are forwarded to the callables
 	 *          supplied at construction time (`ExternalReadFunction` / `ExternalWriteFunction`).
 	 *
 	 *          Key behaviours:
 	 *          - `Read(count)` and `Extract(count)` invoke the configured read handler
 	 *            and return an `ExpectedData<Exception>` (the library's generic buffer
 	 *            `Exception` type is used for handler errors).
 	 *          - `Write(...)` overloads forward their data to the configured write
 	 *            handler and return `true` on success, `false` on failure.
 	 *          - Several methods that are meaningful for in-memory FIFOs are provided
 	 *            for API compatibility but act as no-ops on `Forwarder`: `Clear()`,
 	 *            `Clean()`, `Seek()`, and `Skip()`.
 	 *
 	 *          Important notes:
 	 *          - Unlike `FIFO::Read(0)` which returns all available bytes, `Forwarder::Read(0)`
 	 *            is treated as a request for zero bytes (a no-op) because the forwarder
 	 *            does not own an internal buffer and cannot determine the amount of data
 	 *            the external source would provide. Callers should request an explicit
 	 *            non-zero `count` when they want data from the external handler.
 	 *          - When a handler is not supplied for an operation the forwarder uses the
 	 *            `NoopReadFunction()` / `NoopWriteFunction()` which return an appropriate
 	 *            `Exception` via `StormByte::Unexpected`.
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
			 * @brief Forwarded read operation.
			 * @param count Number of bytes to request from the external source. A value of
			 *              `0` is treated as a no-op (requests zero bytes) — callers should
			 *              use a non-zero `count` to obtain data from the external handler.
			 * @return `ExpectedData<ReadError>` containing the bytes on success or an
			 *         `ReadError` describing a handler failure.
			 */
			ExpectedData<ReadError> Read(std::size_t count = 0) const override;

			/**
			 * @brief Compatibility overload that behaves like `Read()` for forwarders.
			 * @param count Number of bytes to request.
			 * @return `ExpectedData<ReadError>` with the requested bytes or an error.
			 *
			 * @note Present for API compatibility with `SharedFIFO` - it forwards to the
			 *       same underlying read handler.
			 */
			ExpectedData<ReadError> Extract(std::size_t count = 0) override;

			/**
			 * @brief Forward a vector of bytes to the configured write handler.
			 * @param data Bytes to write.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
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
			 * @brief No-op for Forwarder (kept for API compatibility).
			 * @details `Clear()` is meaningful for in-memory FIFOs. For `Forwarder` it does
			 *          nothing because the forwarder does not store data.
			 */
			void Clear() noexcept override;

			/**
			 * @brief No-op for Forwarder (kept for API compatibility).
			 */
			void Clean() noexcept override;

			/**
			 * @brief No-op seek for Forwarder (kept for API compatibility).
			 */
			void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief No-op skip for Forwarder (kept for API compatibility).
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