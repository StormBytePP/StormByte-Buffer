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

#include <cstddef>
#include <string_view>

/**
 * @namespace StormByte
 * @brief Root namespace of the StormByte C++ suite.
 */
namespace StormByte {
	/**
	 * @namespace StormByte::Buffer
	 * @brief Buffer module of the StormByte suite.
	 */
	namespace Buffer {
		/**
		 * @namespace StormByte::Buffer::IO
		 * @brief Coordinated byte I/O: results and private backends.
		 */
		namespace IO {
			/**
			 * @enum Status
			 * @brief Outcome of a single I/O call.
			 *
			 * There is no would-block / EAGAIN value.
			 *
			 * @see Result, State, StormByte::Buffer::BufferedRead
			 */
			enum class STORMBYTE_BUFFER_PUBLIC Status {
				Ok,			///< The call completed as requested (see @ref Result::count).
				End,		///< Origin exhausted; @ref Result::count may be short.
				Error,		///< Origin failed during the call; destination untouched.
				Failed		///< Illegal call or dead session; destination untouched.
			};

			/**
			 * @enum State
			 * @brief Session state of a coordinated reader.
			 *
			 * Distinct from @ref Status, which is per-call.
			 *
			 * @see StormByte::Buffer::BufferedRead::State
			 */
			enum class STORMBYTE_BUFFER_PUBLIC State {
				Idle,		///< Armed. Ready to read until a pull proves otherwise.
				Missing,	///< Last Open: path does not exist.
				Directory,	///< Last Open: path is a directory.
				Permission,	///< Last Open: regular file exists; open denied.
				Fault,		///< Origin I/O error or corrupt mid-read.
				Unavailable	///< Ctor, after Close, or origin gone / unreachable.
			};

			/**
			 * @brief Enumerator name of @p status.
			 * @param status Per-call status.
			 * @return Stable name, or empty if unknown.
			 */
			[[nodiscard]] constexpr std::string_view ToString(Status status) noexcept {
				switch (status) {
					case Status::Ok:		return "Ok";
					case Status::End:		return "End";
					case Status::Error:		return "Error";
					case Status::Failed:	return "Failed";
				}
				return {};
			}

			/**
			 * @brief Enumerator name of @p state.
			 * @param state Session state.
			 * @return Stable name, or empty if unknown.
			 */
			[[nodiscard]] constexpr std::string_view ToString(State state) noexcept {
				switch (state) {
					case State::Idle:			return "Idle";
					case State::Missing:		return "Missing";
					case State::Directory:		return "Directory";
					case State::Permission:		return "Permission";
					case State::Fault:			return "Fault";
					case State::Unavailable:	return "Unavailable";
				}
				return {};
			}

			/**
			 * @struct Result
			 * @brief Per-call status plus how many bytes reached the destination.
			 *
			 * @c count is 0 when the destination FIFO was not written
			 * (@ref Status::Failed, @ref Status::Error, or @ref Status::End
			 * with no remaining bytes).
			 *
			 * @see Status, StormByte::Buffer::BufferedRead::Read
			 */
			struct STORMBYTE_BUFFER_PUBLIC Result {
				Status status;		///< Outcome of the call.
				std::size_t count;	///< Bytes placed in the destination FIFO this call.
			};
		}
	}
}
