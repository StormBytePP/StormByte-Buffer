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

#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_reader.hxx>
#include <StormByte/buffer/io/buffered_writer.hxx>
#include <StormByte/buffer/visibility.h>

#include <cstddef>
#include <memory>

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
				 */
				class Bridge;
			}
		}

		/**
		 * @class Bridge
		 * @brief Move bytes from a source to a sink.
		 *
		 * Pairings: ExternalReader/Writer, IO::BufferedReader/Writer, or mixed.
		 * Holds references only. Tips must outlive the Bridge and every
		 * Passthrough. Does not Open or Seek. No local cache. No configured
		 * chunk: @c Passthrough(n) is the unit.
		 *
		 * External sources are consumed with Extract (Read only if Extract
		 * fails). IO sources use Read, which already consumes. Writers are
		 * never const.
		 *
		 * @c Passthrough(n) blocks and is transactional: the whole request
		 * lands on the sink or neither tip is consumed. @c n == 0 moves
		 * what is available on the source now. A short read at EoF is
		 * success and marks EoF; it is not Failed.
		 *
		 * @ref Flush is a no-op on an External sink and @c Flush on an
		 * IO writer. The destructor Flushes.
		 *
		 * @see ExternalReader, ExternalWriter, IO::BufferedReader, IO::BufferedWriter
		 */
		class STORMBYTE_BUFFER_PUBLIC Bridge {
			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Buffer → buffer. Tips not owned.
				 * @param in Source.
				 * @param out Sink.
				 */
				Bridge(ExternalReader& in, ExternalWriter& out) noexcept;

				/**
				 * @brief IO → IO. Tips must already be armed. Not owned.
				 * @param in Source.
				 * @param out Sink.
				 */
				Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out) noexcept;

				/**
				 * @brief Buffer → IO. Tips not owned.
				 * @param in Source.
				 * @param out Sink.
				 */
				Bridge(ExternalReader& in, IO::BufferedWriter& out) noexcept;

				/**
				 * @brief IO → buffer. Tips not owned.
				 * @param in Source.
				 * @param out Sink.
				 */
				Bridge(const IO::BufferedReader& in, ExternalWriter& out) noexcept;

				/**
				 * @brief Copy constructor is deleted.
				 */
				Bridge(const Bridge&) = delete;

				/**
				 * @brief Move constructor. Moved-from Passthrough is a no-op.
				 * @param other Instance to take from.
				 */
				Bridge(Bridge&& other) noexcept;

				/**
				 * @brief Destructor. Calls @ref Flush.
				 */
				~Bridge() noexcept;

				/**
				 * @brief Copy assignment is deleted.
				 * @return *this.
				 */
				Bridge& operator=(const Bridge&) = delete;

				/**
				 * @brief Move assignment. Moved-from Passthrough is a no-op.
				 * @param other Instance to take from.
				 * @return *this.
				 */
				Bridge& operator=(Bridge&& other) noexcept;

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
				 * @brief External sink: no-op. IO sink: @c BufferedWriter::Flush.
				 * @return @c true on success or if there was nothing to flush.
				 */
				bool Flush() noexcept;

				/**
				 * @brief @ref Flush then Close an External writer. IO: Flush only.
				 * @return @c true if Flush succeeded.
				 */
				bool FlushAndClose() noexcept;

				/**
				 * @brief @c SetError on an External writer. No-op on IO.
				 */
				void SetError() noexcept;

				/**
				 * @brief Move up to @p bytes from source to sink. Blocks.
				 * @param bytes 0 = available now.
				 * @return @c true on success.
				 */
				bool Passthrough(std::size_t bytes) noexcept;

			private:
				std::unique_ptr<IO::Backend::Bridge> m_io;	///< Pump.
		};
	}
}
