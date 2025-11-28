#pragma once

#include <StormByte/buffer/visibility.h>

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
}