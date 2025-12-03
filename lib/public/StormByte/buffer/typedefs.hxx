#pragma once

#include <StormByte/buffer/exception.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/expected.hxx>

#include <cstddef>
#include <functional>
#include <span>
#include <vector>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/** @brief Forward declaration of Consumer class. */
	class Consumer;
	
	/** @brief Forward declaration of Producer class. */
	class Producer;

	/**
	 * @brief Type alias for Expected containing byte vector data.
	 * @tparam Exception The exception type to use for error cases.
	 * 
	 * @details This type represents the result of buffer read/extract operations.
	 *          It returns either a vector of bytes on success, or an exception
	 *          wrapped in std::unexpected on failure (e.g., ReadError).
	 * 
	 * @see Expected, ReadError
	 */
	template<class Exception>
	using ExpectedData = Expected<std::vector<std::byte>, Exception>;

	template<class Exception>
	using ExpectedSpan = Expected<std::span<const std::byte>, Exception>;

	template<class Exception>
	using ExpectedVoid = Expected<void, Exception>;

	/**
	 * @brief Type alias for pipeline transformation functions.
	 * 
	 * @details Function signature for pipeline stage transformations that read
	 *          from a Consumer and write to a Producer, enabling data processing
	 *          in multi-stage pipelines.
	 * 
	 * @see Consumer, Producer, Pipeline
	 */
	using PipeFunction = std::function<void(Consumer, Producer, Logger::Log&)>;

	/**
	 * @brief Execution mode selector for pipeline processing.
	 *
	 * @details Defines how pipeline stages are scheduled when invoking
	 *          Pipeline::Process(). Use to control concurrency behavior:
	 *          - ExecutionMode::Sync  : All stages execute sequentially in the
	 *                                   caller's thread (no detached threads).
	 *          - ExecutionMode::Async : Each stage executes concurrently in its
	 *                                   own detached thread (previous default behavior).
	 *
	 * @note Async maximizes throughput via parallel stage execution; Sync can
	 *       simplify debugging and deterministic ordering.
	 * @see Pipeline::Process()
	 */
	enum class STORMBYTE_BUFFER_PUBLIC ExecutionMode {
		Sync,   ///< Sequential single-threaded execution of all stages.
		Async   ///< Concurrent detached-thread execution per stage.
	};

	using ExternalReadFunction = std::function<ExpectedData<ReadError>(const std::size_t&)>;
	using ExternalWriteFunction = std::function<ExpectedVoid<WriteError>(const std::vector<std::byte>&)>;
}