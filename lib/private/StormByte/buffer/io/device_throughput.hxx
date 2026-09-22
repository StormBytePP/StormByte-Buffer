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
#include <filesystem>

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
		 * @brief Buffered binary sources and sinks.
		 */
		namespace IO {
			/**
			 * @struct DeviceThroughput
			 * @brief Indicative sequential throughput of the origin behind a path.
			 *
			 * Not a benchmark. Network FS uses 80 % of the default-route NIC.
			 * Local media uses rotational / NVMe generation / USB class.
			 * Probe failure uses the HDD preset.
			 */
			struct STORMBYTE_BUFFER_PRIVATE DeviceThroughput {
				std::size_t read_bps;	///< Nominal sequential read, bytes per second.
				std::size_t write_bps;	///< Nominal sequential write, bytes per second.
			};

			/**
			 * @brief Classify the volume of @p path and return nominal bytes/s.
			 * @param path Filesystem path. Need not exist; the parent is probed.
			 * @return Preset or NIC-derived rates.
			 */
			STORMBYTE_BUFFER_PRIVATE DeviceThroughput ProbeDeviceThroughput(
				const std::filesystem::path& path) noexcept;
		}
	}
}
