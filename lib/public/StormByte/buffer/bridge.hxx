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
#include <StormByte/buffer/io/typedefs.hxx>
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
		 * @brief Move bytes from a source to a sink on an owned worker thread.
		 *
		 * Pairings: ExternalReader/Writer, IO::BufferedReader/Writer, or mixed.
		 * Holds references only. Tips must outlive the Bridge. Does not Open
		 * or Seek. No local cache. @c Passthrough is private.
		 *
		 * The worker starts in the constructor, like std::thread.
		 * @p high_water > 0 → @ref IO::Drainer::Status::Started.
		 * @p high_water == 0 → @ref IO::Drainer::Status::Paused; @ref Toggle
		 * to run. Setters never start or pause.
		 *
		 * @p high_water is backpressure on the sink (@c Occupied / Dirty).
		 * The worker never Extracts more than fits under the cap. A
		 * single-thread FIFO that nobody reads will wait forever.
		 *
		 * @ref Flush waits for the in-flight transaction, writes it, then
		 * flushes the destination. @ref IO::Drainer::Operation::Flush pushes
		 * whatever is already held, even a short chunk, and does not flush
		 * the destination. On a BufferedWriter those bytes may still sit
		 * in Dirty.
		 *
		 * The destructor joins the worker. There is no public Stop.
		 *
		 * @see ExternalReader, ExternalWriter, IO::BufferedReader, IO::BufferedWriter
		 */
		class STORMBYTE_BUFFER_PUBLIC Bridge {
			public:
				/**
				 * @brief Buffer → buffer. Tips not owned.
				 * @param in Source.
				 * @param out Sink.
				 * @param high_water Sink occupancy cap. 0 starts Paused.
				 */
				Bridge(ExternalReader& in, ExternalWriter& out, std::size_t high_water) noexcept;

				/**
				 * @brief IO → IO. Tips must already be armed. Not owned.
				 * @param in Source.
				 * @param out Sink.
				 * @param high_water Sink occupancy cap. 0 starts Paused.
				 */
				Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out, std::size_t high_water) noexcept;

				/**
				 * @brief Buffer → IO. Tips not owned.
				 * @param in Source.
				 * @param out Sink.
				 * @param high_water Sink occupancy cap. 0 starts Paused.
				 */
				Bridge(ExternalReader& in, IO::BufferedWriter& out, std::size_t high_water) noexcept;

				/**
				 * @brief IO → buffer. Tips not owned.
				 * @param in Source.
				 * @param out Sink.
				 * @param high_water Sink occupancy cap. 0 starts Paused.
				 */
				Bridge(const IO::BufferedReader& in, ExternalWriter& out, std::size_t high_water) noexcept;

				Bridge(const Bridge&) = delete;

				/**
				 * @brief Move constructor. Moved-from Drainer is Stopped.
				 * @param other Instance to take from.
				 */
				Bridge(Bridge&& other) noexcept;

				/**
				 * @brief Destructor. Joins the worker.
				 */
				~Bridge() noexcept;

				Bridge& operator=(const Bridge&) = delete;

				/**
				 * @brief Move assignment. Moved-from Drainer is Stopped.
				 * @param other Instance to take from.
				 * @return *this.
				 */
				Bridge& operator=(Bridge&& other) noexcept;

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
				 * @brief Sink occupancy cap.
				 * @return Current high_water. Does not report status.
				 */
				std::size_t HighWater() const noexcept;

				/**
				 * @brief Set the sink occupancy cap. Does not start or pause.
				 * @param high_water New cap. 0 means no room until raised.
				 */
				void HighWater(std::size_t high_water) noexcept;

				/**
				 * @brief Worker status.
				 * @return @c Stopped if moved-from.
				 */
				IO::Drainer::Status Drainer() const noexcept;

				/**
				 * @brief Toggle pause or hurry-push already held bytes.
				 * @param operation @c Toggle or @c Flush.
				 * @return @c false if moved-from or the operation cannot run.
				 */
				bool Drainer(IO::Drainer::Operation operation) noexcept;

				/**
				 * @brief Finish the in-flight transaction, write it, flush the sink.
				 * @return @c false on error.
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

			private:
				std::unique_ptr<IO::Backend::Bridge> m_io;	///< Pump.
		};
	}
}
