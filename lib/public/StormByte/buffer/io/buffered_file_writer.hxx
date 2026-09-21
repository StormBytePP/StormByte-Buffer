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

#include <StormByte/buffer/io/buffered_writer.hxx>
#include <StormByte/buffer/visibility.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>

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
			 * @class BufferedFileWriter
			 * @brief @ref BufferedWriter leaf over a filesystem file.
			 *
			 * Binary `ofstream` only. Open is append. Overwrite is @ref Truncate.
			 * Does not open in the constructor. Does not create parent directories.
			 *
			 * @see BufferedWriter, State
			 */
			class STORMBYTE_BUFFER_PUBLIC BufferedFileWriter: public BufferedWriter {
				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Store the path and policy knobs. Does not open.
					 * @param path Filesystem path.
					 * @param write_chunk Initial @ref WriteChunk in bytes.
					 * @param back_pressure Initial @ref BackPressure in chunks.
					 * @param max_wait Initial @ref MaxWait.
					 */
					explicit BufferedFileWriter(std::filesystem::path path,
						std::size_t write_chunk = 0, std::size_t back_pressure = 0,
						std::chrono::milliseconds max_wait = std::chrono::milliseconds{0});

					/**
					 * @brief Copy constructor is deleted.
					 */
					BufferedFileWriter(const BufferedFileWriter&) = delete;

					/**
					 * @brief Move constructor.
					 * @param other Instance to take from.
					 */
					BufferedFileWriter(BufferedFileWriter&& other) noexcept;

					/**
					 * @brief Destructor. Calls @ref Close while the leaf vtable is live.
					 */
					~BufferedFileWriter() noexcept override;

					/**
					 * @brief Copy assignment is deleted.
					 * @return *this.
					 */
					BufferedFileWriter& operator=(const BufferedFileWriter&) = delete;

					/**
					 * @brief Move assignment.
					 * @param other Instance to take from.
					 * @return *this.
					 */
					BufferedFileWriter& operator=(BufferedFileWriter&& other) noexcept;

					/**
					 * @}
					 */

					/**
					 * @brief Path passed to the constructor.
					 * @return Stored path (not resolved).
					 */
					const std::filesystem::path& Path() const noexcept;

				protected:
					/**
					 * @brief Open @c m_path as a binary append file. Creates the file
					 *        when the parent directory exists.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					Result OriginOpen() override;

					/**
					 * @brief Close the file stream.
					 * @return @ref Status::Ok.
					 */
					Result OriginClose() override;

					/**
					 * @brief Write @p data to the file.
					 * @param data Contiguous octets.
					 * @return Ok with bytes written, Error or Failed.
					 */
					Result OriginPush(std::span<const std::byte> data) override;

					/**
					 * @brief Flush the output stream.
					 * @return @ref Status::Ok, Error or Failed.
					 */
					Result OriginFlush() override;

					/**
					 * @brief Resize the file to zero bytes.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					Result OriginTruncate() override;

					/**
					 * @brief Ring cap and indicative free space on the volume.
					 * @param n Prospective Write size.
					 * @return @c false if the base rejects @p n or free space is
					 *         below @p n. Query failure is @c false.
					 *
					 * Indicative. Another process, quotas or a network filesystem
					 * can still reject the later Write.
					 */
					bool WillWrite(std::size_t n) const override;

				private:
					std::filesystem::path m_path;			///< Path given at construction.
					std::ofstream m_file;					///< Binary output stream.
					mutable std::mutex m_file_mutex;		///< Serialises ofstream access.
			};
		}
	}
}
