#pragma once

#include <StormByte/buffer/visibility.h>
#include <StormByte/exception.hxx>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for different buffer types
 * including FIFO ring buffers, thread-safe shared buffers, and producer-consumer patterns.
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
}