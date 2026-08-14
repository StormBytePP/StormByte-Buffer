#pragma once

#include <StormByte/buffer/exception.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/expected.hxx>

#include <cstddef>
#include <functional>
#include <memory>
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
	class ReadOnly;					///< Forward declaration of ReadOnly class.
	class WriteOnly;				///< Forward declaration of WriteOnly class.

	/**
	 * @enum Position
	 * @brief Positioning mode for buffer seek / read operations.
	 *
	 * Defines how position values are interpreted in operations such as
	 * seeking or non-destructive reads.
	 */
	enum class STORMBYTE_BUFFER_PUBLIC Position {
		/**
		 * @brief Absolute offset from the beginning of the buffer (position 0).
		 */
		Absolute,

		/**
		 * @brief Offset relative to the current read position.
		 */
		Relative
	};

	/**
	 * @brief Primary byte storage type used throughout the buffer API.
	 *
	 * @details Alias for @c std::vector<std::byte>. Serves as the fundamental
	 *          container for byte-oriented buffer implementations.
	 */
	using DataType = std::vector<std::byte>;

	/**
	 * @enum ExecutionMode
	 * @brief How @ref Pipeline::Process schedules stages.
	 *
	 * - @c Sync  — all stages run sequentially on the caller’s thread
	 *              (no background threads).
	 * - @c Async — a **single** background thread runs all stages **in order**;
	 *              @ref Pipeline::Process returns immediately with the final
	 *              @ref Consumer. This is **not** one thread per stage.
	 *
	 * @note Prefer Async for production workloads; use Sync for deterministic
	 *       debugging.
	 * @see Pipeline::Process()
	 */
	enum class STORMBYTE_BUFFER_PUBLIC ExecutionMode {
		Sync,   ///< Sequential execution on the caller’s thread.
		Async   ///< One background thread; stages run sequentially on it.
	};
}
