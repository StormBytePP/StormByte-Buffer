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

#include <StormByte/buffer/io/typedefs.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/buffer/visibility.h>

#include <cstddef>
#include <memory>
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
		 * @brief Coordinated byte I/O: results and private backends.
		 */
		namespace IO {
			/**
			 * @class BufferedRead
			 * @brief Private implementation of @ref StormByte::Buffer::BufferedRead.
			 *
			 * Not installed as a documented public API. Owns the cache map,
			 * cursor, prefetch thread and origin-exhausted flag. Friend of
			 * the public class so it can call @c Origin* hooks on the leaf.
			 */
			class BufferedRead;
		}

		/**
		 * @class BufferedRead
		 * @brief Coordinated binary read source with optional prefetch and cache.
		 *
		 * BufferedRead is the public base for byte origins that Multimedia
		 * (and later Network or a third party) pass as @c const BufferedRead&.
		 * Leaves inherit this class and implement only the @c Origin* hooks.
		 * They do not override @c Read, @c Peek, @c Seek, @c Open, @c Close
		 * or @c Rewind.
		 *
		 * The class is a coordinator, not a raw device and not an item queue
		 * (@ref Hopper / @ref Sink). Internally it owns a map of @ref FIFO
		 * cache spans (one span when the origin is not seekable). Users never
		 * see that map. There is no Flush API; cache is released on successful
		 * operations, on @ref Close / @ref Rewind, and when @ref MaxMemory
		 * requires it.
		 *
		 * @par Binary only
		 * Octets only (@ref DataType / @ref FIFO). No text mode, no locale,
		 * no newline translation. Decode text above this type.
		 *
		 * @par Synchronous Read / Peek
		 * @c Read(n) and @c Peek(n) run on the caller thread and block until
		 * @p n bytes are available or the origin is exhausted. They never
		 * return a would-block status. Prefetch, when @ref ReadAhead is
		 * greater than zero, is separate and asynchronous; it does not
		 * replace the requested @p n. If prefetch already holds the bytes,
		 * @c Read / @c Peek are O(1) with respect to the origin (no
		 * @ref OriginPull).
		 *
		 * @par Destination FIFO
		 * On @ref IO::Status::Ok or @ref IO::Status::End the destination FIFO
		 * is overwritten with this call's bytes. On @ref IO::Status::Failed
		 * the destination is left untouched.
		 *
		 * @par Read vs Peek vs cache
		 * @c Read advances @ref Tell and removes served bytes from the cache.
		 * @c Peek does not advance the cursor and does not destroy cache.
		 *
		 * @par ReadAhead
		 * Applied after the synchronous request. Extra bytes ahead of the
		 * cursor, not “request + ahead” counted twice on @c Read:
		 * - @c Read(5) with @c ReadAhead(25) → five consumed, cache target 25
		 *   from the new cursor (not 30).
		 * - @c Peek(5) with @c ReadAhead(25) → five remain, cache target 30
		 *   from the cursor.
		 *
		 * A later @c Read flushes an in-flight prefetch (“stop and publish
		 * what you have”) instead of waiting for the full ahead. Example:
		 * ahead 25, prefetch has 12, @c Read(5) takes 5 from those 12, 7
		 * remain, next prefetch asks for 18.
		 *
		 * Prefetch uses the same @ref OriginPull as a user @c Read. Leaves
		 * do not implement a second pull hook and must not buffer inside
		 * the hook.
		 *
		 * @par End of origin vs EoF()
		 * Prefetch may exhaust the device and record that privately without
		 * setting @ref EoF(). Cached bytes remain readable. Further prefetch
		 * is then a no-op. @ref EoF() becomes true when no cache remains and
		 * the origin is exhausted (or after @ref Close).
		 *
		 * @par Open / Close / Rewind
		 * @c Open and @c Close are idempotent. @c Rewind is @c Close() then
		 * @c Open() only when the instance is currently open. Rewind on a
		 * never-opened or already-closed instance returns
		 * @ref IO::Status::Failed and is not a substitute for @c Open.
		 * @c Close empties caches, stops prefetch, calls @ref OriginClose,
		 * and leaves a valid object; further @c Read / @c Peek fail until a
		 * successful @c Open.
		 *
		 * @par Seek and memory
		 * @ref IsSeekable / @ref IsSized are runtime and come from the leaf.
		 * @c Seek does not drop previous cache spans while total cache stays
		 * within @ref MaxMemory. Overlapping spans merge even if the union
		 * exceeds @ref ReadAhead. @c MaxMemory(0) disables cache and prefetch.
		 * The cap is approximate. Cleanup runs on the caller thread at the
		 * end of a successful @c Read / @c Peek / @c Seek.
		 *
		 * @par Concurrency
		 * One consumer thread for @c Read / @c Peek / @c Seek. Prefetch is
		 * the sole extra thread. The cache-map mutex is held only to look
		 * up, publish or drop spans.
		 *
		 * @par Movable, not copyable
		 * Move transfers @c m_io. Moved-from is a closed valid shell. Move
		 * may wait for an in-flight @ref OriginPull.
		 *
		 * @see IO::Status, IO::Result, FIFO, IO::BufferedRead
		 */
		class STORMBYTE_BUFFER_PUBLIC BufferedRead {
			friend class IO::BufferedRead;

			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Copy constructor is deleted.
				 */
				BufferedRead(const BufferedRead&) = delete;

				/**
				 * @brief Move constructor.
				 * @param other Instance to take from. Left closed and valid.
				 */
				BufferedRead(BufferedRead&& other) noexcept;

				/**
				 * @brief Virtual destructor. Closes if still open.
				 */
				virtual ~BufferedRead() noexcept;

				/**
				 * @brief Copy assignment is deleted.
				 * @return *this.
				 */
				BufferedRead& operator=(const BufferedRead&) = delete;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from. Left closed and valid.
				 * @return *this.
				 */
				BufferedRead& operator=(BufferedRead&& other) noexcept;

				/**
				 * @}
				 */

				/**
				 * @brief Whether the source is armed and ready to read.
				 * @return @c true if open, not failed, and not @ref EoF().
				 *
				 * End of stream yields @c false even though the object
				 * remains constructed.
				 */
				virtual explicit operator bool() const noexcept final;

				/**
				 * @name Session
				 * @{
				 */

				/**
				 * @brief Arm the origin if it is not already open.
				 * @return @ref IO::Status::Ok if open (already or newly).
				 *         @ref IO::Status::Failed if @ref OriginOpen failed.
				 *
				 * Idempotent. Does not call @ref OriginOpen when already open.
				 */
				virtual IO::Result Open() final;

				/**
				 * @brief Stop prefetch, drop caches, close the origin.
				 * @return @ref IO::Status::Ok. Always succeeds at this layer.
				 *
				 * Idempotent. Does not call @ref OriginClose when already
				 * closed or never opened. The object stays valid.
				 * @c Read / @c Peek return @ref IO::Status::Failed until a
				 * later successful @c Open.
				 */
				virtual IO::Result Close() final;

				/**
				 * @brief Re-arm an open source: @ref Close then @ref Open.
				 * @return @ref IO::Status::Ok on success.
				 *         @ref IO::Status::Failed if not currently open.
				 *
				 * Not a substitute for the first @c Open. Safe to call
				 * repeatedly while open (each call is a full close/open).
				 */
				virtual IO::Result Rewind() final;

				/**
				 * @brief Whether @ref Open succeeded and @ref Close has not.
				 * @return @c true if the session is open.
				 */
				virtual bool IsOpen() const noexcept final;

				/**
				 * @brief Whether reads may be attempted.
				 * @return @c false if closed, failed, or @ref EoF().
				 */
				virtual bool IsReadable() const noexcept final;

				/**
				 * @brief Whether no further bytes can be produced.
				 * @return @c true when caches are empty and the origin is
				 *         exhausted, or after @ref Close.
				 *
				 * Prefetch hitting device EOF does not by itself set this
				 * flag while cached bytes remain.
				 */
				virtual bool EoF() const noexcept final;

				/**
				 * @}
				 */

				/**
				 * @name Read
				 * @{
				 */

				/**
				 * @brief Read @p n bytes into @p dest, consuming cache / origin.
				 * @param n Byte count. Zero overwrites @p dest with what is
				 *          already in cache at the cursor (no synchronous pull
				 *          for the request) and then starts prefetch if configured.
				 * @param dest Caller FIFO. Overwritten on Ok / End. Untouched on Failed.
				 * @return Status and byte count written to @p dest.
				 *
				 * Blocks until @p n bytes are available or the origin is
				 * exhausted. Short count with @ref IO::Status::End is the
				 * tail before EOF (including count 0).
				 *
				 * After a successful transfer, an in-flight prefetch is
				 * flushed and a new prefetch targets @ref ReadAhead bytes
				 * ahead of the new cursor.
				 */
				virtual IO::Result Read(std::size_t n, FIFO& dest) const final;

				/**
				 * @brief Copy @p n bytes into @p dest without consuming cache.
				 * @param n Byte count. Zero copies the current cache window.
				 * @param dest Caller FIFO. Overwritten on Ok / End. Untouched on Failed.
				 * @return Status and byte count written to @p dest.
				 *
				 * Same wait rule as @ref Read. Cursor and cache spans stay.
				 * Prefetch target after Peek(n) is n (still cached) + ReadAhead.
				 */
				virtual IO::Result Peek(std::size_t n, FIFO& dest) const final;

				/**
				 * @}
				 */

				/**
				 * @name Position
				 * @{
				 */

				/**
				 * @brief Move the logical read cursor.
				 * @param offset Byte offset.
				 * @param mode @ref Position::Absolute from stream start, or
				 *             @ref Position::Relative from @ref Tell.
				 * @return @ref IO::Status::Ok on success.
				 *         @ref IO::Status::Failed if not open, not seekable,
				 *         or @ref OriginSeek failed.
				 *
				 * Does not drop existing cache spans while they fit in
				 * @ref MaxMemory. Overlap with a new window merges spans.
				 */
				virtual IO::Result Seek(std::ptrdiff_t offset, Position mode) const final;

				/**
				 * @brief Logical read offset in the stream.
				 * @return Bytes from the origin start (0 after Open / Rewind).
				 *
				 * Not the read position inside @p dest. Not @ref Size.
				 */
				virtual std::size_t Tell() const noexcept final;

				/**
				 * @brief Whether this instance can reposition the origin.
				 * @return @ref OriginCanSeek.
				 */
				virtual bool IsSeekable() const noexcept final;

				/**
				 * @}
				 */

				/**
				 * @name Size
				 * @{
				 */

				/**
				 * @brief Whether the origin length is known.
				 * @return @ref OriginHasSize.
				 */
				virtual bool IsSized() const noexcept final;

				/**
				 * @brief Origin length in bytes when known.
				 * @return Length, or empty if @ref IsSized is false.
				 */
				virtual std::optional<std::size_t> Size() const noexcept final;

				/**
				 * @}
				 */

				/**
				 * @name Policy
				 * @{
				 */

				/**
				 * @brief Configured prefetch length in bytes.
				 * @return Current ReadAhead. 0 disables prefetch.
				 */
				virtual std::size_t ReadAhead() const noexcept;

				/**
				 * @brief Set prefetch length in bytes.
				 * @param bytes Bytes to hold ahead of the cursor after a Read.
				 *              0 disables prefetch.
				 *
				 * Overridable so a leaf can clamp or ignore. Default stores
				 * @p bytes in the implementation. Does not drop cache already
				 * filled. Applies to the next prefetch cycle.
				 */
				virtual void ReadAhead(std::size_t bytes);

				/**
				 * @brief Configured cache memory cap in bytes.
				 * @return Current cap. 0 means no cache and no prefetch.
				 */
				virtual std::size_t MaxMemory() const noexcept;

				/**
				 * @brief Set cache memory cap in bytes.
				 * @param bytes Approximate maximum resident cache. 0 disables
				 *              cache. The cap need not be exact.
				 *
				 * Overridable so a leaf can clamp or ignore. Lowering the cap
				 * is applied on the next successful Read / Peek / Seek cleanup.
				 */
				virtual void MaxMemory(std::size_t bytes);

				/**
				 * @}
				 */

			protected:
				/**
				 * @brief Construct an unopened coordinator.
				 * @param read_ahead Initial @ref ReadAhead in bytes.
				 * @param max_memory Initial @ref MaxMemory in bytes.
				 *
				 * Does not call @ref OriginOpen. The leaf must @ref Open after
				 * its own members are ready (not from this constructor).
				 */
				explicit BufferedRead(std::size_t read_ahead = 0, std::size_t max_memory = 0);

				/**
				 * @name Origin hooks
				 * @brief Device operations. No extra caching, no prefetch
				 *        thread, no ReadAhead policy. Serve at most the
				 *        requested count.
				 * @{
				 */

				/**
				 * @brief Arm the underlying device.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginOpen() = 0;

				/**
				 * @brief Release the underlying device.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 *
				 * Must be safe to implement as idempotent. The public
				 * @ref Close will not call this twice without an
				 * @ref OriginOpen in between.
				 */
				virtual IO::Result OriginClose() = 0;

				/**
				 * @brief Read up to @p n bytes from the device into @p dest.
				 * @param n Maximum bytes to transfer.
				 * @param dest Implementation FIFO (not the user destination).
				 * @return @ref IO::Status::Ok with count == n,
				 *         @ref IO::Status::End with count <= n if the device
				 *         is exhausted, or @ref IO::Status::Failed.
				 *
				 * Used both by synchronous @c Read and by the base prefetch
				 * thread. Do not set public @ref EoF() here; report device
				 * end via @ref IO::Status::End. Do not retain extra bytes
				 * beyond this call.
				 */
				virtual IO::Result OriginPull(std::size_t n, FIFO& dest) = 0;

				/**
				 * @brief Whether the device can seek.
				 * @return @c true if @ref OriginSeek is usable.
				 */
				virtual bool OriginCanSeek() const noexcept = 0;

				/**
				 * @brief Seek the device.
				 * @param offset Byte offset.
				 * @param mode Absolute or relative to the device cursor.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginSeek(std::ptrdiff_t offset, Position mode) = 0;

				/**
				 * @brief Whether the device reports a length.
				 * @return @c true if @ref OriginSize has a value.
				 */
				virtual bool OriginHasSize() const noexcept = 0;

				/**
				 * @brief Device length in bytes.
				 * @return Length, or empty when unknown (socket, live stream).
				 */
				virtual std::optional<std::size_t> OriginSize() const noexcept = 0;

				/**
				 * @}
				 */

			private:
				std::unique_ptr<IO::BufferedRead> m_io;	///< Private coordinator state.
		};
	}
}
