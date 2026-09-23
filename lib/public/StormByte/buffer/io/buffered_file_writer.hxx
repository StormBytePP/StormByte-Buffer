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
			 * Binary file, random-access. Open creates the file when
			 * missing and leaves existing content. Overwrite is
			 * @ref Truncate. Does not open in the constructor. Does
			 * not create parent directories. Not sealed.
			 *
			 * The device does not change after construction: @ref WriteChunk
			 * and @ref BackPressure are chosen once. @ref MaxWait stays
			 * dynamic.
			 *
			 * @par Constructors
			 * @c BufferedFileWriter(path) defers chunk and backpressure to
			 * @ref Setup (device probe). @c BufferedFileWriter(path,
			 * write_chunk, back_pressure, max_wait) stores those values.
			 * A zero chunk or backpressure disables the ring.
			 *
			 * A derived class may override any @c Origin* hook, @ref Seek,
			 * @ref Size, @ref WillWrite and @ref Setup. When the transport
			 * is still this file, call the File implementation and then
			 * add behaviour. When it is not, do not call these File
			 * implementations.
			 *
			 * The derived destructor must call @ref Close first.
			 * File @ref Close is idempotent.
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
					 * @brief Store the path. Chunk and backpressure come from @ref Setup.
					 * @param path Filesystem path.
					 */
					explicit BufferedFileWriter(std::filesystem::path path);

					/**
					 * @brief Store the path and explicit knobs. Does not open.
					 * @param path Filesystem path.
					 * @param write_chunk Initial @ref WriteChunk in bytes.
					 * @param back_pressure Initial @ref BackPressure in chunks.
					 * @param max_wait Initial @ref MaxWait.
					 */
					BufferedFileWriter(std::filesystem::path path,
						StormByte::Size write_chunk, std::size_t back_pressure,
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
					virtual const std::filesystem::path& Path() const noexcept;

					/**
					 * @brief Logical file length in bytes.
					 * @return max(filesystem size, @ref Tell).
					 *
					 * @ref Tell includes @ref Dirty. Does not Flush.
					 */
					virtual StormByte::Size Size() const noexcept override;

					/**
					 * @brief Move the write cursor after flushing dirty bytes.
					 * @param offset Byte offset.
					 * @param mode @ref Position::Absolute or @ref Position::Relative.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					virtual Result Seek(std::ptrdiff_t offset, Position mode) override;

				protected:
					/**
					 * @brief Apply device chunk and backpressure for the path-only ctor.
					 *
					 * No-op when the explicit constructor already set those knobs
					 * (including 0 = ring off).
					 */
					virtual void Setup() override;

					/**
					 * @brief Open @c m_path for binary random-access write.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 *
					 * Creates the file when missing. Does not truncate an
					 * existing file.
					 */
					virtual Result OriginOpen() override;

					/**
					 * @brief Close the file stream.
					 * @return @ref Status::Ok.
					 */
					virtual Result OriginClose() override;

					/**
					 * @brief Write @p data to the file.
					 * @param data Contiguous octets.
					 * @return Ok with bytes written, Error or Failed.
					 */
					virtual Result OriginPush(std::span<const std::byte> data) override;

					/**
					 * @brief Make written bytes visible to later readers of the path.
					 * @return @ref Status::Ok, Error or Failed.
					 *
					 * After Ok the filesystem size is the written length.
					 * On Windows this includes FlushFileBuffers when the
					 * volume allows a shared handle.
					 */
					virtual Result OriginFlush() override;

					/**
					 * @brief Resize the file to zero bytes.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					virtual Result OriginTruncate() override;

					/**
					 * @brief Seek the file to @p absolute.
					 * @param absolute Byte offset from the start.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					virtual Result OriginSeek(StormByte::Size absolute) override;

					/**
					 * @brief Ring cap and indicative free space on the volume.
					 * @param n Prospective Write size.
					 * @return @c false if the base rejects @p n or free space is
					 *         below @p n. Query failure is @c false.
					 *
					 * Indicative. Another process, quotas or a network filesystem
					 * can still reject the later Write. A derived writer that
					 * is not a local volume should override this.
					 */
					virtual bool WillWrite(StormByte::Size n) const override;

				private:
					std::filesystem::path m_path;			///< Path given at construction.
					std::ofstream m_file;					///< Binary output stream.
					mutable std::mutex m_file_mutex;		///< Serialises ofstream access.
					bool m_probe_on_setup;					///< True for the path-only constructor.
			};
		}
	}
}
