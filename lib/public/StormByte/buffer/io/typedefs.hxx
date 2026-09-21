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
			 * @brief Outcome of an I/O operation on a buffered byte source.
			 *
			 * There is no would-block / EAGAIN value.
			 *
			 * @see Result, StormByte::Buffer::BufferedRead
			 */
			enum class STORMBYTE_BUFFER_PUBLIC Status {
				Ok,	///< The call completed as requested (see @ref Result::count).
				End,	///< Origin exhausted; @ref Result::count may be short.
				Failed	///< Permanent failure or invalid state for the call.
			};

			/**
			 * @struct Result
			 * @brief Status plus how many bytes were transferred into the destination.
			 *
			 * @c count is 0 when nothing was written to the caller FIFO
			 * (including @ref Status::Failed and @ref Status::End with no
			 * remaining bytes).
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
