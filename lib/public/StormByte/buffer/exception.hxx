/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * or later, as published by the Free Software Foundation.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 */

#pragma once

#include <StormByte/buffer/visibility.h>
#include <StormByte/exception.hxx>

#include <format>
#include <string>
#include <utility>

/**
 * @namespace StormByte::Buffer
 * @brief Buffer module of the StormByte suite.
 */
namespace StormByte::Buffer {
	/**
	 * @class Exception
	 * @brief Base exception type for the Buffer module.
	 *
	 * Prefixes messages with @c "StormByte::Buffer" and forwards them to
	 * @ref StormByte::Exception using @ref StormByte::Component.
	 *
	 * @see Error, ReadError, WriteError
	 */
	class STORMBYTE_BUFFER_PUBLIC Exception: public StormByte::Exception {
		public:
			/**
			 * @brief Construct a Buffer exception from an unformatted message.
			 * @param message Exception text.
			 */
			explicit Exception(const std::string& message):
				StormByte::Exception(StormByte::Component{"Buffer"}, "{}", message) {}

			/**
			 * @brief Construct a Buffer exception from a moved unformatted message.
			 * @param message Exception text.
			 */
			explicit Exception(std::string&& message):
				StormByte::Exception(StormByte::Component{"Buffer"}, "{}", std::move(message)) {}

			/**
			 * @brief Construct a Buffer exception with an explicit component.
			 * @tparam Args Format argument types.
			 * @param component Component name inserted after @c "StormByte::".
			 * @param fmt C++20 format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			Exception(StormByte::Component component, std::format_string<Args...> fmt, Args&&... args):
				StormByte::Exception(component, fmt, std::forward<Args>(args)...) {}

			/**
			 * @brief Construct a Buffer exception with a formatted message.
			 * @tparam Args Format argument types.
			 * @param fmt C++20 format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			Exception(std::format_string<Args...> fmt, Args&&... args):
				StormByte::Exception(StormByte::Component{"Buffer"}, fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class Error
	 * @brief General exception class for buffer errors.
	 *
	 * Intermediate base for module-specific failures. Inherits constructors
	 * from @ref Exception via @c using Exception::Exception.
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
	 * The component name is fixed to @c "Buffer::ReadError".
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadError: public Error {
		public:
			/**
			 * @brief Construct a read error from an unformatted message.
			 * @param message Exception text.
			 */
			explicit ReadError(const std::string& message):
				Error(StormByte::Component{"Buffer::ReadError"}, "{}", message) {}

			/**
			 * @brief Construct a read error from a moved unformatted message.
			 * @param message Exception text.
			 */
			explicit ReadError(std::string&& message):
				Error(StormByte::Component{"Buffer::ReadError"}, "{}", std::move(message)) {}

			/**
			 * @brief Construct a read error with a format message.
			 * @tparam Args Format argument types.
			 * @param fmt C++20 format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			ReadError(std::format_string<Args...> fmt, Args&&... args):
				Error(StormByte::Component{"Buffer::ReadError"}, fmt, std::forward<Args>(args)...) {}
	};

	/**
	 * @class WriteError
	 * @brief Exception thrown when a buffer write operation fails.
	 *
	 * The component name is fixed to @c "Buffer::WriteError".
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteError: public Error {
		public:
			/**
			 * @brief Construct a write error from an unformatted message.
			 * @param message Exception text.
			 */
			explicit WriteError(const std::string& message):
				Error(StormByte::Component{"Buffer::WriteError"}, "{}", message) {}

			/**
			 * @brief Construct a write error from a moved unformatted message.
			 * @param message Exception text.
			 */
			explicit WriteError(std::string&& message):
				Error(StormByte::Component{"Buffer::WriteError"}, "{}", std::move(message)) {}

			/**
			 * @brief Construct a write error with a format message.
			 * @tparam Args Format argument types.
			 * @param fmt C++20 format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			WriteError(std::format_string<Args...> fmt, Args&&... args):
				Error(StormByte::Component{"Buffer::WriteError"}, fmt, std::forward<Args>(args)...) {}
	};
}
