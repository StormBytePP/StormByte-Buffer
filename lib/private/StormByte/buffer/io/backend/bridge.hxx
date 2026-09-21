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

#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_reader.hxx>
#include <StormByte/buffer/io/buffered_writer.hxx>
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
		 * @brief Buffered binary sources and sinks.
		 */
		namespace IO {
			/**
			 * @namespace StormByte::Buffer::IO::Backend
			 * @brief PIMPL coordinators for the public IO types.
			 */
			namespace Backend {
				/**
				 * @class Bridge
				 * @brief Private pump for @ref StormByte::Buffer::Bridge.
				 *
				 * Holds non-owning pointers to one source and one sink.
				 * @c Passthrough is blocking and transactional. No local cache.
				 * External sources are Extracted.
				 */
				class STORMBYTE_BUFFER_PRIVATE Bridge {
					public:
						/**
						 * @name Lifecycle
						 * @{
						 */

						/**
						 * @brief Buffer → buffer.
						 * @param in Source.
						 * @param out Sink.
						 */
						Bridge(ExternalReader& in, ExternalWriter& out) noexcept;

						/**
						 * @brief IO → IO.
						 * @param in Source.
						 * @param out Sink.
						 */
						Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out) noexcept;

						/**
						 * @brief Buffer → IO.
						 * @param in Source.
						 * @param out Sink.
						 */
						Bridge(ExternalReader& in, IO::BufferedWriter& out) noexcept;

						/**
						 * @brief IO → buffer.
						 * @param in Source.
						 * @param out Sink.
						 */
						Bridge(const IO::BufferedReader& in, ExternalWriter& out) noexcept;

						/**
						 * @brief Copy constructor is deleted.
						 */
						Bridge(const Bridge&) = delete;

						/**
						 * @brief Move constructor is deleted. The public Bridge moves the unique_ptr.
						 */
						Bridge(Bridge&&) = delete;

						/**
						 * @brief Destructor. Does not Flush; the public Bridge does.
						 */
						~Bridge() = default;

						/**
						 * @brief Copy assignment is deleted.
						 * @return *this.
						 */
						Bridge& operator=(const Bridge&) = delete;

						/**
						 * @brief Move assignment is deleted.
						 * @return *this.
						 */
						Bridge& operator=(Bridge&&) = delete;

						/**
						 * @}
						 */

						/**
						 * @brief Whether the source reports end-of-stream.
						 * @return @c true on EoF.
						 */
						bool EoF() const noexcept;

						/**
						 * @brief Whether the source can be read.
						 * @return @c false on error or closed.
						 */
						bool IsReadable() const noexcept;

						/**
						 * @brief Whether the sink accepts writes.
						 * @return @c false if closed or failed.
						 */
						bool IsWritable() const noexcept;

						/**
						 * @brief External sink: no-op. IO sink: BufferedWriter::Flush.
						 * @return @c true on success or if there was nothing to flush.
						 */
						bool Flush() noexcept;

						/**
						 * @brief Flush then Close an External writer. IO: Flush only.
						 * @return @c true if Flush succeeded.
						 */
						bool FlushAndClose() noexcept;

						/**
						 * @brief SetError on an External writer. No-op on IO.
						 */
						void SetError() noexcept;

						/**
						 * @brief Move up to @p bytes from source to sink. Blocks.
						 * @param bytes 0 = available now.
						 * @return @c true on success.
						 */
						bool Passthrough(std::size_t bytes) noexcept;

					private:
						/**
						 * @brief Bytes available on the source now.
						 * @return 0 if unknown.
						 */
						std::size_t AvailableNow() const noexcept;

						/**
						 * @brief Pull @p n bytes into @p dest without committing the sink.
						 * @param n Byte count.
						 * @param dest Work FIFO.
						 * @return @c false on hard read failure.
						 */
						bool Pull(std::size_t n, FIFO& dest) noexcept;

						/**
						 * @brief Push @p src to the sink. Source FIFO consumed only on success.
						 * @param src Work FIFO.
						 * @return @c false on write failure.
						 */
						bool Push(FIFO& src) noexcept;

						ExternalReader* m_ext_in {nullptr};				///< External source.
						ExternalWriter* m_ext_out {nullptr};			///< External sink.
						const IO::BufferedReader* m_io_in {nullptr};	///< IO source.
						IO::BufferedWriter* m_io_out {nullptr};			///< IO sink.
				};
			}
		}
	}
}
