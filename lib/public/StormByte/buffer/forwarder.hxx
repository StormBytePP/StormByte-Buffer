#pragma once

#include <StormByte/buffer/fifo.hxx>
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
	 * @brief A lightweight forwarding buffer that delegates reads and writes to provided handlers.
	 *
	 * @details The `Forwarder` does not store bytes internally. Instead it forwards
	 *          read and write operations to external callables provided at construction
	 *          time. This is useful to encapsulate complex read/write environments,
	 *          e.g. when integrating with network I/O, files, or other custom backends
	 *          where the buffer logic is implemented outside the library.
	 *
	 *          Construction options:
	 *          - Provide only a read function: the forwarder will accept `Read()`
	 *            calls and will return an error for `Write()` operations.
	 *          - Provide only a write function: the forwarder will accept `Write()`
	 *            calls and will return an error for `Read()` operations.
	 *          - Provide both: read and write are both forwarded to the corresponding
	 *            handlers.
	 *
	 *          The read/write callables use the `ExternalReadFunction` and
	 *          `ExternalWriteFunction` type aliases from `typedefs.hxx`. When a
	 *          handler is not supplied the forwarder uses an internal noop handler
	 *          that returns an appropriate `ReadError`/`WriteError` (i.e. the
	 *          operation results in an error rather than throwing an exception).
	 *
	 * @note Copy and move operations are deleted to avoid accidental sharing
	 *       of handler state; create a new `Forwarder` if you need a different
	 *       configuration.
	 */
	class STORMBYTE_BUFFER_PUBLIC Forwarder {
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
			 * @brief Perform a non-owning read forwarded to the configured handler.
			 * @param count Number of bytes to read. A value of `0` is treated as a
			 *              no-op and requests zero bytes; the forwarder will simply
			 *              invoke the configured read handler with `0`. Use a
			 *              non-zero `count` to request bytes from the external source.
			 * @return Expected containing a vector of bytes on success, or a `ReadError` on failure.
			 *
			 * @note Unlike `FIFO::Read(0)` which returns all available bytes, `Forwarder::Read(0)`
			 *       does not imply "return all" because the forwarder does not own the
			 *       underlying storage and cannot know how many bytes the external source
			 *       would provide. Callers must supply an explicit `count` when they want
			 *       data from the external handler.
			 *
			 * @note The blocking behaviour and interpretation of `count` is defined by
			 *       the provided `ExternalReadFunction` implementation.
			 */
			ExpectedData<ReadError> Read(const std::size_t& count) const;

			/**
			 * @brief Forward a vector of bytes to the configured write handler.
			 * @param data Vector of bytes to write.
			 * @return Expected<void, WriteError> indicating success or failure.
			 */
			ExpectedVoid<WriteError> Write(const std::vector<std::byte>& data);

			/**
			 * @brief Forward the contents of a `Buffer::FIFO` to the write handler.
			 * @param data FIFO whose readable contents will be forwarded. If `data.Read(0)`
			 *             returns an error, the call is treated as a noop and success is
			 *             returned.
			 * @return Expected<void, WriteError> indicating success or failure of the write.
			 */
			ExpectedVoid<WriteError> Write(const Buffer::FIFO& data);

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