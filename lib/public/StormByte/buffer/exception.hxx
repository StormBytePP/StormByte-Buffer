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
     * @details Thrown when a write operation exceeds the buffer's capacity
     */
    class STORMBYTE_BUFFER_PUBLIC InsufficientData: public Exception {
        public:
            using Exception::Exception;
    };

	/**
	 * @class ReadError
	 * @brief Exception class for read errors from buffers.
	 * 
	 * @details Thrown when a read operation fails
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadError: public Exception {
		public:
			using Exception::Exception;
	};

	/**
	 * @class WriteError
	 * @brief Exception class for write errors to buffers.
	 * 
	 * @details Thrown when a write operation fails
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteError: public Exception {
		public:
			using Exception::Exception;
	};
}