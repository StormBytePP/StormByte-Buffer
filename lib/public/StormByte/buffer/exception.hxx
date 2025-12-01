#pragma once

#include <StormByte/buffer/visibility.h>
#include <StormByte/exception.hxx>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	// Generic Buffer exceptions
    class STORMBYTE_BUFFER_PUBLIC Exception: public StormByte::Exception {
        public:
            using StormByte::Exception::Exception;
    };

    /**
     * @class BufferOverflow
     * @brief Exception class for buffer overflow errors.
     *
     * The `BufferOverflow` exception is thrown when an operation attempts to write
     * more data to a buffer than it can hold. This exception ensures that buffer
     * integrity is maintained by preventing overflows.
     *
     * Inherits all functionality from the `StormByte::Buffer::Exception` class.
     */
    class STORMBYTE_BUFFER_PUBLIC InsufficientData: public Exception {
        public:
            using Exception::Exception;
    };

	/**
	 * @class ReaderExhausted
	 * @brief Exception class for reader exhaustion errors.
	 * The `ReaderExhausted` exception is thrown when a read operation is attempted
	 * on a function that will not return more data, indicating that the reader has been exhausted.
	 */
	class STORMBYTE_BUFFER_PUBLIC ReaderExhausted: public Exception {
		public:
			using Exception::Exception;
	};
}