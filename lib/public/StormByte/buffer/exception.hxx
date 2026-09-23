/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Buffer source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-Logger tree and
 * the rest of the StormByte suite it vendors), which remains under its own
 * license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
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
