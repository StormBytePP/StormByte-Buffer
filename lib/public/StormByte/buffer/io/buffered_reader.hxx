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
#include <StormByte/buffer/io/typedefs.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/buffer/visibility.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
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
			 * @namespace StormByte::Buffer::IO::Backend
			 * @brief PIMPL coordinators for the public IO types.
			 */
			namespace Backend {
				/**
				 * @class BufferedReader
				 * @brief Private implementation of @ref StormByte::Buffer::IO::BufferedReader.
				 */
				class BufferedReader;
			}

			/**
			 * @class BufferedReader
			 * @brief Coordinated binary read source with optional prefetch and cache.
			 *
			 * Public base for byte origins. Callers take
			 * @c const BufferedReader&. Leaves implement only the
			 * @c Origin* hooks. They do not override @c Read, @c Peek,
			 * @c Seek, @c Open, @c Close or @c Rewind.
			 *
			 * @par Binary only
			 * Octets only (@ref DataType / @ref FIFO / @c std::span<std::byte>). No text mode.
			 *
			 * @par Session
			 * Construction leaves @ref State::Unavailable. A successful
			 * @ref Open moves to @ref State::Idle (armed, ready to read
			 * until a pull proves otherwise). @ref Close returns to
			 * @ref Unavailable and is idempotent. @ref Open is not idempotent:
			 * a second @c Open while Idle fails and leaves the state Idle.
			 * @c Close then @c Open is a valid round-trip.
			 *
			 * @c operator bool is true only when the instance is prepared to
			 * read: @ref State is Idle and not @ref EoF.
			 *
			 * @par Read / Peek
			 * Wait for @p n bytes or origin end. @ref MaxWait of @c 0ms waits
			 * without limit and never returns @ref IO::Status::TryAgain.
			 * A positive @ref MaxWait caps the wait; timeout yields
			 * @ref IO::Status::TryAgain, destination untouched, state Idle.
			 * Origin failure during a pull is @ref IO::Status::Error; the
			 * destination is not written; session state becomes
			 * @ref State::Fault or @ref State::Unavailable.
			 *
			 * FIFO overloads: @c n == 0 serves the current cached span from
			 * @ref Tell. Span overloads: @c dest.size() is the request;
			 * an empty span returns @ref IO::Status::Ok and count 0 without
			 * consuming or pulling. There is no @c Read(n, span).
			 *
			 * @par Destination
			 * FIFO: overwritten on @ref IO::Status::Ok or @ref IO::Status::End
			 * with a non-zero count. Span: the first @c count bytes of
			 * @p dest are written; the remainder of the span is left as-is.
			 * Untouched on @ref IO::Status::Failed, @ref IO::Status::Error,
			 * @ref IO::Status::TryAgain, or End with count 0.
			 *
			 * @par Read vs Peek vs cache
			 * @c Read advances @ref Tell and removes served bytes from the cache.
			 * @c Peek does not.
			 *
			 * @par Cache map
			 * A seekable origin stores owned spans keyed by stream offset.
			 * Overlap and abutment merge, even past @ref ReadAhead.
			 * @ref MaxMemory is an approximate cap: overflow evicts the
			 * spans farthest from @ref Tell, not the whole cache.
			 * @ref MaxMemory of 0 stores no cache. A non-seekable origin
			 * keeps a single forward span.
			 *
			 * @par Seek
			 * Moves the logical cursor. When the origin is seekable the
			 * device is realigned with @ref OriginSeek even on a cache hit,
			 * so a later pull is not silent corruption. Cached spans are
			 * not discarded solely because of Seek. A hit only avoids
			 * re-pulling those bytes. A non-seekable origin rejects Seek
			 * without calling the hook.
			 *
			 * Seek is not guaranteed to be O(1) or to return immediately.
			 * The call may wait for an in-flight prefetch to cancel, run
			 * @ref OriginSeek (local file, remote origin, or a leaf that
			 * does CPU work first), and update the cache map. Immediate
			 * return is not part of the contract.
			 *
			 * @par ReadAhead
			 * Applied after the synchronous request. Prefetch uses @ref OriginPull.
			 * Leaves must not buffer inside the hook.
			 *
			 * @par Policy setters
			 * @ref ReadAhead, @ref MaxMemory and @ref MaxWait take effect
			 * immediately. They are not deferred to the next Read. Lowering
			 * ReadAhead or MaxMemory may drop cached bytes that no longer fit.
			 * The setter does not return until prefetch is cancelled and the
			 * cache is trimmed. That wait is blocking even though it is not
			 * an origin pull.
			 *
			 * @par Movable, not copyable
			 * Move transfers @c m_io. Moved-from is Unavailable.
			 *
			 * @see IO::Status, State, Result, FIFO
			 */
			class STORMBYTE_BUFFER_PUBLIC BufferedReader {
				friend class Backend::BufferedReader;

				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Copy constructor is deleted.
					 */
					BufferedReader(const BufferedReader&) = delete;

					/**
					 * @brief Move constructor.
					 * @param other Instance to take from. Left Unavailable.
					 */
					BufferedReader(BufferedReader&& other) noexcept;

					/**
					 * @brief Virtual destructor. Stops the worker. Does not call Origin*.
					 *
					 * Leaves must call @ref Close in their destructor so
					 * @ref OriginClose still runs on a live vtable.
					 */
					virtual ~BufferedReader() noexcept;

					/**
					 * @brief Copy assignment is deleted.
					 */
					BufferedReader& operator=(const BufferedReader&) = delete;

					/**
					 * @brief Move assignment.
					 * @param other Instance to take from. Left Unavailable.
					 * @return *this.
					 */
					BufferedReader& operator=(BufferedReader&& other) noexcept;

					/**
					 * @}
					 */

					/**
					 * @brief Whether the source is prepared to read.
					 * @return @c true if @ref State is @ref State::Idle and not @ref EoF.
					 */
					virtual explicit operator bool() const noexcept final;

					/**
					 * @brief Session state.
					 * @return Current @ref State.
					 */
					virtual enum State State() const noexcept final;

					/**
					 * @name Session
					 * @{
					 */

					/**
					 * @brief Arm the origin.
					 * @return @c true if @ref State is @ref State::Idle afterwards.
					 *
					 * Not idempotent. A second call while Idle returns @c false
					 * and leaves the session Idle.
					 */
					virtual bool Open() final;

					/**
					 * @brief Stop prefetch, drop caches, close the origin.
					 * @return @ref IO::Status::Ok. Always succeeds at this layer.
					 *
					 * Idempotent. Sets session state to @ref State::Unavailable.
					 */
					virtual Result Close() final;

					/**
					 * @brief Re-arm an open source: @ref Close then @ref Open.
					 * @return @c true if session state is Idle afterwards.
					 *         @c false if not currently armed.
					 */
					virtual bool Rewind() final;

					/**
					 * @brief Whether @ref Open succeeded and @ref Close has not.
					 * @return @c true if the session is armed.
					 */
					virtual bool IsOpen() const noexcept final;

					/**
					 * @brief Whether reads may be attempted.
					 * @return Same as @c operator bool.
					 */
					virtual bool IsReadable() const noexcept final;

					/**
					 * @brief Whether no further bytes can be produced.
					 * @return @c true when no cached byte remains at @ref Tell
					 *         and the origin is exhausted, or after @ref Close.
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
					 * @param n Byte count. Zero serves the current span from @ref Tell.
					 * @param dest Caller FIFO. Overwritten on Ok / End with count > 0.
					 * @return Status and byte count written to @p dest.
					 */
					virtual Result Read(std::size_t n, FIFO& dest) const final;

					/**
					 * @brief Read into @p dest, consuming cache / origin.
					 * @param dest Caller span. Request size is @c dest.size().
					 * @return Status and byte count written to the front of @p dest.
					 *
					 * An empty span returns @ref IO::Status::Ok and count 0
					 * without consuming or pulling. The first @c count bytes of
					 * @p dest are written; the tail is left unchanged.
					 */
					virtual Result Read(std::span<std::byte> dest) const final;

					/**
					 * @brief Copy @p n bytes into @p dest without consuming cache.
					 * @param n Byte count. Zero copies the current span from @ref Tell.
					 * @param dest Caller FIFO. Overwritten on Ok / End with count > 0.
					 * @return Status and byte count written to @p dest.
					 */
					virtual Result Peek(std::size_t n, FIFO& dest) const final;

					/**
					 * @brief Copy into @p dest without consuming cache.
					 * @param dest Caller span. Request size is @c dest.size().
					 * @return Status and byte count written to the front of @p dest.
					 *
					 * An empty span returns @ref IO::Status::Ok and count 0
					 * without pulling. The first @c count bytes of @p dest are
					 * written; the tail is left unchanged.
					 */
					virtual Result Peek(std::span<std::byte> dest) const final;

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
					 * @param mode @ref Position::Absolute or @ref Position::Relative.
					 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
					 *
					 * Seekable origins call @ref OriginSeek for the resolved
					 * target whether or not that offset is already cached.
					 * The cache map is kept. Non-seekable origins return
					 * Failed without invoking the hook. Negative absolute
					 * offsets and relative steps before offset 0 fail.
					 * Not currently armed also fails.
					 *
					 * @par Latency
					 * Not O(1). May block on prefetch cancellation,
					 * @ref OriginSeek, and cache bookkeeping. Immediate
					 * return is not part of the contract.
					 */
					virtual Result Seek(std::ptrdiff_t offset, Position mode) const final;

					/**
					 * @brief Logical read offset in the stream.
					 * @return Bytes from the origin start (0 after Open / Rewind).
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
					 * @brief Set prefetch length. Takes effect immediately.
					 * @param bytes Bytes to hold ahead of the cursor after a Read.
					 *
					 * Cancels in-flight prefetch and may trim the cache before
					 * returning. Does not pull from the origin. Still waits
					 * for the worker.
					 */
					virtual void ReadAhead(std::size_t bytes);

					/**
					 * @brief Configured cache memory cap in bytes.
					 * @return Current cap. 0 means no cache and no prefetch.
					 */
					virtual std::size_t MaxMemory() const noexcept;

					/**
					 * @brief Set cache memory cap. Takes effect immediately.
					 * @param bytes Approximate maximum resident cache. 0 drops all spans.
					 *
					 * Cancels prefetch and evicts farthest spans before
					 * returning. Waits for the worker; not an origin pull.
					 */
					virtual void MaxMemory(std::size_t bytes);

					/**
					 * @brief Configured read wait limit.
					 * @return Wait cap. @c 0ms waits forever (never @ref IO::Status::TryAgain).
					 */
					virtual std::chrono::milliseconds MaxWait() const noexcept;

					/**
					 * @brief Set read wait limit. Takes effect on the next Read / Peek.
					 * @param wait @c 0ms = unlimited. Positive = timeout then TryAgain.
					 *
					 * Overridable so a leaf can clamp. Does not cancel an in-flight
					 * prefetch. Does not trim the cache.
					 */
					virtual void MaxWait(std::chrono::milliseconds wait);

					/**
					 * @}
					 */

				protected:
					/**
					 * @brief Construct an unopened coordinator (@ref State::Unavailable).
					 * @param read_ahead Initial @ref ReadAhead in bytes.
					 * @param max_memory Initial @ref MaxMemory in bytes.
					 * @param max_wait Initial @ref MaxWait. @c 0ms = unlimited.
					 */
					explicit BufferedReader(std::size_t read_ahead = 0, std::size_t max_memory = 0,
						std::chrono::milliseconds max_wait = std::chrono::milliseconds{0});

					/**
					 * @brief Publish session state from a leaf hook.
					 * @param state New @ref State.
					 *
					 * Called from @ref OriginOpen, @ref OriginClose and
					 * @ref OriginPull. Not for user code.
					 */
					void SetState(enum State state) noexcept;

					/**
					 * @name Origin hooks
					 * @{
					 */

					/**
					 * @brief Arm the underlying device and @ref SetState.
					 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
					 */
					virtual Result OriginOpen() = 0;

					/**
					 * @brief Release the underlying device and @ref SetState Unavailable.
					 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
					 */
					virtual Result OriginClose() = 0;

					/**
					 * @brief Read up to @p n bytes from the device into @p dest.
					 * @param n Maximum bytes to transfer.
					 * @param dest Implementation FIFO (not the user destination).
					 * @return @ref IO::Status::Ok, @ref IO::Status::End,
					 *         @ref IO::Status::Error or @ref IO::Status::Failed.
					 *
					 * On @ref IO::Status::Error call @ref SetState with
					 * @ref State::Fault or @ref State::Unavailable.
					 */
					virtual Result OriginPull(std::size_t n, FIFO& dest) = 0;

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
					 *
					 * May be slow (remote origin, heavy leaf setup). The public
					 * @ref Seek path assumes this can block.
					 */
					virtual Result OriginSeek(std::ptrdiff_t offset, Position mode) = 0;

					/**
					 * @brief Whether the device reports a length.
					 * @return @c true if @ref OriginSize has a value.
					 */
					virtual bool OriginHasSize() const noexcept = 0;

					/**
					 * @brief Device length in bytes.
					 * @return Length, or empty when unknown.
					 */
					virtual std::optional<std::size_t> OriginSize() const noexcept = 0;

					/**
					 * @}
					 */

				private:
					std::unique_ptr<Backend::BufferedReader> m_io;	///< Private coordinator state.
			};
		}
	}
}
