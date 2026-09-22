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

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_reader.hxx>
#include <StormByte/buffer/io/typedefs.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/buffer/visibility.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

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
				 * @class BufferedReader
				 * @brief Private implementation of @ref StormByte::Buffer::IO::BufferedReader.
				 *
				 * Owns session flags, @ref State, the cache map, the logical
				 * cursor and the prefetch worker. Invokes @c Origin* hooks on
				 * @c m_owner.
				 *
				 * A seekable origin keeps a map of owned spans keyed by stream
				 * offset. Seek always realigns the device with @c OriginSeek.
				 * A cache hit avoids a re-pull only. Overlapping or adjacent
				 * spans merge. @ref MaxMemory evicts the spans farthest from
				 * @ref Tell. A non-seekable origin keeps a single forward
				 * span; @c Seek fails without calling the hook.
				 */
				class STORMBYTE_BUFFER_PRIVATE BufferedReader {
					public:
						/**
						 * @name Lifecycle
						 * @{
						 */

						/**
						 * @brief Bind to the public leaf and store policy knobs.
						 * @param owner Public instance (the most-derived object).
						 * @param read_ahead Initial @ref ReadAhead in bytes.
						 * @param max_memory Initial @ref MaxMemory in bytes.
						 * @param max_wait Initial @ref MaxWait. @c 0ms = unlimited.
						 *
						 * Starts the worker thread. State is @ref State::Unavailable.
						 */
						BufferedReader(IO::BufferedReader& owner, std::size_t read_ahead,
							std::size_t max_memory, std::chrono::milliseconds max_wait);

						/**
						 * @brief Copy constructor is deleted.
						 */
						BufferedReader(const BufferedReader&) = delete;

						/**
						 * @brief Move constructor is deleted.
						 */
						BufferedReader(BufferedReader&&) = delete;

						/**
						 * @brief Stop the worker. Does not call OriginClose.
						 */
						~BufferedReader();

						/**
						 * @brief Copy assignment is deleted.
						 */
						BufferedReader& operator=(const BufferedReader&) = delete;

						/**
						 * @brief Move assignment is deleted.
						 */
						BufferedReader& operator=(BufferedReader&&) = delete;

						/**
						 * @}
						 */

						/**
						 * @brief Point hooks at a new public instance after a move.
						 * @param owner Destination public object.
						 */
						void Rebind(IO::BufferedReader& owner) noexcept;

						/**
						 * @brief Whether the source is prepared to read.
						 * @return @ref IsReadable.
						 */
						explicit operator bool() const noexcept;

						/**
						 * @brief Session state.
						 * @return Current @ref State.
						 */
						enum State State() const noexcept;

						/**
						 * @brief Publish session state from a leaf hook.
						 * @param state New @ref State.
						 */
						void SetState(enum State state) noexcept;

						/**
						 * @name Session
						 * @{
						 */

						/**
						 * @brief Arm the origin.
						 * @return @c true if @ref State is @ref State::Idle afterwards.
						 */
						bool Open();

						/**
						 * @brief Flush prefetch, drop caches, close the origin.
						 * @return @ref Status::Ok. Idempotent. State → Unavailable.
						 */
						Result Close();

						/**
						 * @brief Join the worker and drop caches. Does not call Origin*.
						 */
						void Shutdown();

						/**
						 * @brief @ref Close then @ref Open when currently armed.
						 * @return @c true if Idle afterwards.
						 */
						bool Rewind();

						/**
						 * @brief Whether the session is armed (not Unavailable-from-ctor/close).
						 * @return @c true after a successful Open until Close.
						 */
						bool IsOpen() const noexcept;

						/**
						 * @brief Whether a @c Read may still produce bytes.
						 * @return Idle and not @ref EoF.
						 */
						bool IsReadable() const noexcept;

						/**
						 * @brief Whether no further bytes can be produced.
						 * @return @c true when no cached byte remains at @ref Tell
						 *         and the origin is exhausted, or after @ref Close.
						 */
						bool EoF() const noexcept;

						/**
						 * @}
						 */

						/**
						 * @name Read
						 * @{
						 */

						/**
						 * @brief Consume @p n bytes into @p dest.
						 * @param n Requested count. Zero serves the current span.
						 * @param dest Caller FIFO.
						 * @return Status and bytes written to @p dest.
						 */
						Result Read(std::size_t n, FIFO& dest) const;

						/**
						 * @brief Copy @p n bytes into @p dest without consuming.
						 * @param n Requested count. Zero copies the current span.
						 * @param dest Caller FIFO.
						 * @return Status and bytes written to @p dest.
						 */
						Result Peek(std::size_t n, FIFO& dest) const;

						/**
						 * @}
						 */

						/**
						 * @name Position
						 * @{
						 */

						/**
						 * @brief Move the logical cursor and realign a seekable origin.
						 * @param offset Byte offset.
						 * @param mode Absolute or relative.
						 * @return @ref Status::Ok or @ref Status::Failed.
						 *
						 * Seekable: @c OriginSeek to the target even on a cache hit.
						 * Not seekable or not armed: Failed, hook not called.
						 */
						Result Seek(std::ptrdiff_t offset, Position mode) const;

						/**
						 * @brief Logical read offset in the stream.
						 * @return Bytes from the origin start.
						 */
						std::size_t Tell() const noexcept;

						/**
						 * @brief Whether the leaf origin can seek.
						 * @return @c OriginCanSeek.
						 */
						bool IsSeekable() const noexcept;

						/**
						 * @}
						 */

						/**
						 * @name Size
						 * @{
						 */

						/**
						 * @brief Whether the leaf origin reports a length.
						 * @return @c OriginHasSize.
						 */
						bool IsSized() const noexcept;

						/**
						 * @brief Origin length when known.
						 * @return Length, or empty.
						 */
						std::optional<std::size_t> Size() const noexcept;

						/**
						 * @}
						 */

						/**
						 * @name Policy
						 * @{
						 */

						/**
						 * @brief Configured prefetch length.
						 * @return Current ReadAhead.
						 */
						std::size_t ReadAhead() const noexcept;

						/**
						 * @brief Set prefetch length.
						 * @param bytes New target. 0 disables prefetch.
						 */
						void ReadAhead(std::size_t bytes);

						/**
						 * @brief Configured cache cap.
						 * @return Current MaxMemory.
						 */
						std::size_t MaxMemory() const noexcept;

						/**
						 * @brief Set cache cap.
						 * @param bytes Approximate resident cap. 0 drops all spans.
						 */
						void MaxMemory(std::size_t bytes);

						/**
						 * @brief Configured read wait limit.
						 * @return @c 0ms waits forever.
						 */
						std::chrono::milliseconds MaxWait() const noexcept;

						/**
						 * @brief Set read wait limit.
						 * @param wait @c 0ms = unlimited. Positive = timeout then TryAgain.
						 */
						void MaxWait(std::chrono::milliseconds wait);

						/**
						 * @}
						 */

					private:
						/**
						 * @brief Start @c m_worker if it is not joinable.
						 */
						void StartWorker();

						/**
						 * @brief Signal stop and join @c m_worker.
						 */
						void StopWorker();

						/**
						 * @brief Ask the worker to fill up to the current ahead target.
						 */
						void RequestPrefetch() const;

						/**
						 * @brief Cancel the in-flight pull and wait until the worker is idle.
						 */
						void FlushPrefetch() const;

						/**
						 * @brief Worker loop: wait for a target, pull, park.
						 */
						void Worker();

						/**
						 * @brief Drop every cached span.
						 */
						void DropCache() const;

						/**
						 * @brief Bytes stored across all spans.
						 * @return Sum of each span's available length.
						 */
						std::size_t CachedBytes() const noexcept;

						/**
						 * @brief Contiguous cached bytes starting at @p pos.
						 * @param pos Stream offset.
						 * @return Length of the hit span from @p pos, or 0.
						 */
						std::size_t CoverageFrom(std::size_t pos) const noexcept;

						/**
						 * @brief Span that contains @p pos, or @c m_spans.end().
						 * @param pos Stream offset.
						 * @return Map iterator.
						 */
						std::map<std::size_t, FIFO>::iterator FindSpan(std::size_t pos) const noexcept;

						/**
						 * @brief Copy @p n bytes at @p pos from a hit span into @p dest.
						 * @param pos Stream offset of the first byte.
						 * @param n Byte count. Must fit in the hit.
						 * @param dest Destination FIFO (appends).
						 * @return @c true if the copy succeeded.
						 */
						bool CopyFromCache(std::size_t pos, std::size_t n, FIFO& dest) const;

						/**
						 * @brief Remove @p [from, to) from the map, splitting spans.
						 * @param from Inclusive stream offset.
						 * @param to Exclusive stream offset.
						 */
						void EraseRange(std::size_t from, std::size_t to) const;

						/**
						 * @brief Insert @p piece at @p start and merge overlap / abutment.
						 * @param start Stream offset of @p piece[0].
						 * @param piece Owned bytes. Ignored when @ref MaxMemory is 0.
						 */
						void CommitSpan(std::size_t start, FIFO&& piece) const;

						/**
						 * @brief Evict spans farthest from @ref Tell until @ref MaxMemory.
						 *
						 * @c 0 drops the whole map. A span that contains @ref Tell
						 * is trimmed last, keeping bytes nearest the cursor.
						 */
						void CollectGarbage() const;

						/**
						 * @brief Place the device cursor at @p pos if needed.
						 * @param pos Desired origin offset.
						 * @return @ref Status::Ok or @ref Status::Failed.
						 *
						 * Must not run under @c m_mutex. Not seekable only
						 * succeeds when the origin is already at @p pos.
						 */
						Result EnsureOrigin(std::size_t pos) const;

						/**
						 * @brief @c OriginPull at @p at into @p dest and optionally cache.
						 * @param at Stream offset to read from.
						 * @param n Maximum bytes.
						 * @param dest Receives the pulled bytes (not the user FIFO).
						 * @return Origin status and byte count.
						 *
						 * Must not run under @c m_mutex.
						 */
						Result PullAt(std::size_t at, std::size_t n, FIFO& dest) const;

						/**
						 * @brief Shared @c Read / @c Peek implementation.
						 * @param n Requested count.
						 * @param dest Caller FIFO.
						 * @param consume @c true for Read, @c false for Peek.
						 * @return Status and bytes written to @p dest.
						 */
						Result Serve(std::size_t n, FIFO& dest, bool consume) const;

						IO::BufferedReader* m_owner;					///< Public leaf (hooks).

						mutable std::mutex m_mutex;						///< Session + map.
						mutable std::condition_variable m_cv;			///< Worker / flush waits.

						std::size_t m_read_ahead {0};					///< Prefetch target length.
						std::size_t m_max_memory {0};					///< Approximate cache cap.
						std::chrono::milliseconds m_max_wait {0};		///< Read wait cap. 0 = forever.

						enum State m_state { State::Unavailable };		///< Session state.
						bool m_open {false};							///< Session armed (Open until Close).
						mutable bool m_failed {false};					///< Permanent failure.
						mutable bool m_origin_exhausted {false};		///< Device EOF (not public EoF).
						mutable std::size_t m_tell {0};					///< Logical stream cursor.
						mutable std::size_t m_origin_pos {0};			///< Last known device cursor.
						mutable bool m_origin_valid {false};			///< Whether @c m_origin_pos is known.

						mutable std::map<std::size_t, FIFO> m_spans;	///< [offset, offset+len) owned bytes.

						mutable std::atomic<bool> m_stop {false};		///< Worker teardown.
						mutable std::atomic<bool> m_cancel_prefetch {false}; ///< Flush in-flight pull.
						mutable bool m_prefetch_run {false};			///< Worker has an active target.
						mutable std::size_t m_prefetch_target {0};		///< Desired coverage from Tell.
						std::thread m_worker;							///< Prefetch thread.
				};
			}
		}
	}
}
