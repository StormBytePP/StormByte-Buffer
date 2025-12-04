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
	class Consumer;					///< Forward declaration of Consumer class.
	class Producer;					///< Forward declaration of Producer class.

	/**
	 * @enum Position
	 * @brief Specifies positioning mode for buffer operations.
	 *
	 * This enumeration defines how position values should be interpreted
	 * in buffer operations such as seeking or reading.
	 */
	enum class STORMBYTE_BUFFER_PUBLIC Position {
		/**
		 * @brief Absolute positioning from the beginning of the buffer.
		 *
		 * When this mode is used, position values are interpreted as
		 * offsets from the start of the buffer (position 0).
		 */
		Absolute,
		
		/**
		 * @brief Relative positioning from the current position.
		 *
		 * When this mode is used, position values are interpreted as
		 * offsets from the current read/write position.
		 */
		Relative
	};

	using DataType = std::vector<std::byte>;

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
}