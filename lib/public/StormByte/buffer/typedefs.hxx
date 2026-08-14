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
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	class Consumer;		///< Forward declaration of the Consumer class.
	class Producer;		///< Forward declaration of the Producer class.
	class ReadOnly;		///< Forward declaration of the ReadOnly interface.
	class WriteOnly;	///< Forward declaration of the WriteOnly interface.

	/**
	 * @enum Position
	 * @brief Positioning mode for buffer seek / non-destructive read operations.
	 *
	 * Defines how offset values are interpreted by @c Seek() and related APIs.
	 *
	 * @see ReadOnly::Seek()
	 */
	enum class STORMBYTE_BUFFER_PUBLIC Position {
		/**
		 * @brief Absolute offset from the beginning of the buffer (position 0).
		 */
		Absolute,

		/**
		 * @brief Offset relative to the current logical read position.
		 */
		Relative
	};

	/**
	 * @brief Primary byte storage type used throughout the buffer API.
	 *
	 * @details Alias for @c std::vector&lt;std::byte&gt;. Serves as the fundamental
	 *          container for byte-oriented buffer implementations
	 *          (@ref FIFO, adapters, extract/read destinations, etc.).
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
	 *
	 * @see Pipeline::Process()
	 */
	enum class STORMBYTE_BUFFER_PUBLIC ExecutionMode {
		Sync,	///< Sequential execution on the caller’s thread.
		Async	///< One background thread; stages run sequentially on it.
	};
}
