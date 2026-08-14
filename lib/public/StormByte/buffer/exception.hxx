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
	/**
	 * @class Exception
	 * @brief Base exception type for the Buffer module.
	 *
	 * @details Prefixes the component name with @c "Buffer::" and forwards
	 *          a format string plus arguments to @ref StormByte::Exception.
	 */
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
	 * @brief Exception thrown when a read operation fails.
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadError: public Error {
		public:
			template <typename... Args>
			ReadError(std::format_string<Args...> fmt, Args&&... args):
			Error("Buffer::ReadError", fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class WriteError
	 * @brief Exception thrown when a write operation fails.
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteError: public Error {
		public:
			template <typename... Args>
			WriteError(std::format_string<Args...> fmt, Args&&... args):
			Error("Buffer::WriteError", fmt, std::forward<Args>(args)...) {}
	};
}
