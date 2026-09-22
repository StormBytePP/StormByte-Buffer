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

#include <StormByte/buffer/io/buffered_reader.hxx>
#include <StormByte/buffer/visibility.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>

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
		class FIFO;

		/**
		 * @namespace StormByte::Buffer::IO
		 * @brief Buffered binary sources and sinks.
		 */
		namespace IO {
			/**
			 * @class BufferedFileReader
			 * @brief @ref BufferedReader leaf over a filesystem file.
			 *
			 * Binary `ifstream` only. Hooks call @ref SetState.
			 * Does not open in the constructor. Not sealed.
			 *
			 * A derived class may override any @c Origin* hook. Keep
			 * @ref OriginCanSeek if the origin stays seekable. When the
			 * transport is still this file, call the File implementation
			 * and then add behaviour. When the transport is not this file,
			 * override every hook that touches the stream and do not call
			 * these File implementations. Prefetch and Seek stay in
			 * @ref BufferedReader.
			 *
			 * The derived destructor must call @ref Close first so the
			 * derived vtable is live. File @ref Close is idempotent.
			 *
			 * @see BufferedReader, State
			 */
			class STORMBYTE_BUFFER_PUBLIC BufferedFileReader: public BufferedReader {
				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Store the path and policy knobs. Does not open.
					 * @param path Filesystem path.
					 * @param read_ahead Initial @ref ReadAhead in bytes (0 = off).
					 * @param max_memory Initial @ref MaxMemory in bytes (0 = no cache).
					 */
					explicit BufferedFileReader(std::filesystem::path path,
						std::size_t read_ahead = 0, std::size_t max_memory = 0);

					/**
					 * @brief Copy constructor is deleted.
					 */
					BufferedFileReader(const BufferedFileReader&) = delete;

					/**
					 * @brief Move constructor.
					 * @param other Instance to take from.
					 */
					BufferedFileReader(BufferedFileReader&& other) noexcept;

					/**
					 * @brief Destructor. Calls @ref Close while the leaf vtable is live.
					 */
					~BufferedFileReader() noexcept override;

					/**
					 * @brief Copy assignment is deleted.
					 * @return *this.
					 */
					BufferedFileReader& operator=(const BufferedFileReader&) = delete;

					/**
					 * @brief Move assignment.
					 * @param other Instance to take from.
					 * @return *this.
					 */
					BufferedFileReader& operator=(BufferedFileReader&& other) noexcept;

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
					 * @brief Open @c m_path as a binary input file and cache its size.
					 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
					 */
					Result OriginOpen() override;

					/**
					 * @brief Close the file stream.
					 * @return @ref IO::Status::Ok.
					 */
					Result OriginClose() override;

					/**
					 * @brief Read up to @p n bytes from the file into @p dest.
					 * @param n Maximum bytes.
					 * @param dest Base-owned FIFO.
					 * @return Ok, End, Error or Failed.
					 */
					Result OriginPull(std::size_t n, FIFO& dest) override;

					/**
					 * @brief Files are seekable.
					 * @return @c true.
					 */
					bool OriginCanSeek() const noexcept override;

					/**
					 * @brief Seek the file stream.
					 * @param offset Byte offset.
					 * @param mode Absolute from start or relative to the file cursor.
					 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
					 */
					Result OriginSeek(std::ptrdiff_t offset, Position mode) override;

					/**
					 * @brief Length is known after a successful @ref OriginOpen.
					 * @return @c true when @c m_size is set.
					 */
					bool OriginHasSize() const noexcept override;

					/**
					 * @brief Cached file size.
					 * @return Size in bytes, or empty if not open.
					 */
					std::optional<std::size_t> OriginSize() const noexcept override;

				private:
					std::filesystem::path m_path;			///< Path given at construction.
					std::ifstream m_file;					///< Binary input stream.
					std::optional<std::size_t> m_size;		///< Size after OriginOpen.
					mutable std::mutex m_file_mutex;		///< Serialises ifstream access.
			};
		}
	}
}
