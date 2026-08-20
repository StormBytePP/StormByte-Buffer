/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte.
 *
 * StormByte is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StormByte is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StormByte. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <StormByte/buffer/visibility.h>
#include <StormByte/exception.hxx>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class Exception
	 * @brief Base exception type for the Buffer module.
	 *
	 * @details Prefixes the component name with @c "Buffer::" and forwards
	 *          a C++20 format string plus arguments to @ref StormByte::Exception.
	 *
	 * @see Error, ReadError, WriteError
	 */
	class STORMBYTE_BUFFER_PUBLIC Exception: public StormByte::Exception {
		public:
			/**
			 * @brief Construct a Buffer exception.
			 * @tparam Args Format argument types.
			 * @param component Logical sub-component name (e.g. @c "FIFO", @c "Ring").
			 *                  Prefixed automatically with @c "Buffer::".
			 * @param fmt       C++20 format string.
			 * @param args      Format arguments.
			 */
			template <typename... Args>
			Exception(const std::string& component, std::format_string<Args...> fmt, Args&&... args):
			StormByte::Exception("Buffer::" + component, fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class Error
	 * @brief General exception class for buffer errors.
	 *
	 * @details Intermediate base for module-specific failures. Inherits constructors
	 *          from @ref Exception via @c using Exception::Exception.
	 *
	 * @see ReadError, WriteError
	 */
	class STORMBYTE_BUFFER_PUBLIC Error: public Exception {
		public:
			using Exception::Exception;
	};

	/**
	 * @class ReadError
	 * @brief Exception thrown when a buffer read / extract / peek operation fails.
	 *
	 * @details The component name is fixed to @c "Buffer::ReadError".
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadError: public Error {
		public:
			/**
			 * @brief Construct a read error with a format message.
			 * @tparam Args Format argument types.
			 * @param fmt  C++20 format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			ReadError(std::format_string<Args...> fmt, Args&&... args):
			Error("Buffer::ReadError", fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class WriteError
	 * @brief Exception thrown when a buffer write operation fails.
	 *
	 * @details The component name is fixed to @c "Buffer::WriteError".
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteError: public Error {
		public:
			/**
			 * @brief Construct a write error with a format message.
			 * @tparam Args Format argument types.
			 * @param fmt  C++20 format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			WriteError(std::format_string<Args...> fmt, Args&&... args):
			Error("Buffer::WriteError", fmt, std::forward<Args>(args)...) {}
	};
}
