#pragma once

#include <StormByte/buffer/forwarder.hxx>

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
	 * @brief Simple pass-through adapter that forwards bytes from a read handler to a write handler.
	 *
	 * @details `Bridge` privately inherits from `Forwarder` and provides a convenience
	 *          operation `Passthrough()` that reads an explicit number of bytes from the
	 *          configured read handler and immediately forwards them to the write handler.
	 *
	 *          The class is intentionally final and non-copyable. It is lightweight and
	 *          does not buffer data itself — it relies on the external read/write handlers
	 *          supplied at construction time.
	 *
	 * @par Error handling
	 *          `Passthrough()` returns an `ExpectedVoid<Exception>` which contains an
	 *          `Exception` when either the read or write handler fails. The returned
	 *          exception message will include the underlying handler's error message.
	 *
	 * @note Use explicit non-zero byte counts with `Passthrough()`. A `count` of `0`
	 *       is treated as a no-op and does not invoke the read or write handlers.
	 */
	class STORMBYTE_BUFFER_PUBLIC Bridge final: private Forwarder {
		public:
			/**
			 * @brief Construct a Bridge with both read and write handlers.
			 * @param readFunc  Callable used for reads.
			 * @param writeFunc Callable used for writes.
			 */
			/**
			 * @brief Construct a Bridge with both read and write handlers.
			 * @param readFunc  Callable used for reads.
			 * @param writeFunc Callable used for writes.
			 * @note The constructor is noexcept and stores the provided callables by
			 *       value into the base `Forwarder` implementation.
			 */
			Bridge(const ExternalReadFunction& readFunc, const ExternalWriteFunction& writeFunc) noexcept;

			/**
			 * @brief Copy constructor (deleted).
			 * @details Bridge is non-copyable because it owns or references external
			 *          handler callables; copying could lead to surprising sharing or
			 *          lifetime issues. Create a new `Bridge` instance explicitly
			 *          when you need a separate configuration.
			 */
			Bridge(const Bridge& other) = delete;

			/**
			 * @brief Move constructor (deleted).
			 * @details Moving a `Bridge` is disallowed to avoid accidental transfer of
			 *          handler ownership and to keep semantics simple and explicit.
			 */
			Bridge(Bridge&& other) noexcept = delete;

			/**
			 * @brief Destructor.
			 * @details Defaulted destructor. `Bridge` performs no special cleanup beyond
			 *          what its base `Forwarder` and contained callables perform.
			 */
			virtual ~Bridge() = default;

			/**
			 * @brief Copy assignment (deleted).
			 */
			Bridge& operator=(const Bridge& other) = delete;

			/**
			 * @brief Move assignment (deleted).
			 */
			Bridge& operator=(Bridge&& other) noexcept = delete;

			/**
			 * @brief Read `count` bytes from the read handler and write them to the write handler.
			 * @param count Number of bytes to transfer. If `count == 0` the call is a no-op and
			 *              succeeds immediately.
			 * @return `ExpectedVoid<Exception>` empty on success, or containing an `Exception`
			 *         describing the failure from the read or write handler.
			 *
			 * @details This method performs a single read of exactly `count` bytes (as defined
			 *          by the configured `ExternalReadFunction`) and then forwards the returned
			 *          bytes to the configured `ExternalWriteFunction`. If the read handler
			 *          returns an error, `Passthrough()` returns an `Exception` indicating a
			 *          read failure. If the write handler returns an error, `Passthrough()`
			 *          returns an `Exception` indicating a write failure.
			 */
			ExpectedVoid<Exception> Passthrough(const std::size_t& count);
	};
			
}