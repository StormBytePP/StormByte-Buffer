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
			template <typename... Args>
			Exception(const std::string& component, std::format_string<Args...> fmt, Args&&... args):
			StormByte::Exception("Buffer::" +component, fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class Error
	 * @brief General exception class for buffer errors.
	 */
	class STORMBYTE_BUFFER_PUBLIC Error: public Exception {
		public:
			using Exception::Exception;
	};

	/**
	 * @class ReadError
	 * @brief Exception class for read errors from buffers.
	 * 
	 * @details Thrown when a read operation fails
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadError: public Error {
		public:
			template <typename... Args>
			ReadError(std::format_string<Args...> fmt, Args&&... args):
			Error("Buffer::ReadError", fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class WriteError
	 * @brief Exception class for write errors to buffers.
	 * 
	 * @details Thrown when a write operation fails
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteError: public Error {
		public:
			template <typename... Args>
			WriteError(std::format_string<Args...> fmt, Args&&... args):
			Error("Buffer::WriteError", fmt, std::forward<Args>(args)...) {}
	};
}