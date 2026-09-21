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
		 * @brief Coordinated byte I/O: results and private backends.
		 */
		namespace IO {
			/**
			 * @class BufferedRead
			 * @brief Private implementation of @ref StormByte::Buffer::BufferedRead.
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
		 * @par Binary only
		 * Octets only (@ref DataType / @ref FIFO). No text mode.
		 *
		 * @par Session
		 * Construction leaves @ref IO::State::Unavailable. A successful
		 * @ref Open moves to @ref IO::State::Idle (armed, ready to read
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
		 * destination FIFO is not written; session state becomes
		 * @ref IO::State::Fault or @ref IO::State::Unavailable.
		 *
		 * @par Destination FIFO
		 * Overwritten on @ref IO::Status::Ok or @ref IO::Status::End with
		 * a non-zero count. Untouched on @ref IO::Status::Failed,
		 * @ref IO::Status::Error, @ref IO::Status::TryAgain, or End with count 0.
		 *
		 * @par Read vs Peek vs cache
		 * @c Read advances @ref Tell and removes served bytes from the cache.
		 * @c Peek does not.
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
		 * window is trimmed. That wait is blocking even though it is not
		 * an origin pull.
		 *
		 * @par Movable, not copyable
		 * Move transfers @c m_io. Moved-from is Unavailable.
		 *
		 * @see IO::Status, IO::State, IO::Result, FIFO, IO::BufferedRead
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
				 * @param other Instance to take from. Left Unavailable.
				 */
				BufferedRead(BufferedRead&& other) noexcept;

				/**
				 * @brief Virtual destructor. Stops the worker. Does not call Origin*.
				 *
				 * Leaves must call @ref Close in their destructor so
				 * @ref OriginClose still runs on a live vtable.
				 */
				virtual ~BufferedRead() noexcept;

				/**
				 * @brief Copy assignment is deleted.
				 * @return *this.
				 */
				BufferedRead& operator=(const BufferedRead&) = delete;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from. Left Unavailable.
				 * @return *this.
				 */
				BufferedRead& operator=(BufferedRead&& other) noexcept;

				/**
				 * @}
				 */

				/**
				 * @brief Whether the source is prepared to read.
				 * @return @c true if @ref State is @ref IO::State::Idle and not @ref EoF.
				 */
				virtual explicit operator bool() const noexcept final;

				/**
				 * @brief Session state.
				 * @return Current @ref IO::State.
				 */
				virtual enum IO::State State() const noexcept final;

				/**
				 * @name Session
				 * @{
				 */

				/**
				 * @brief Arm the origin.
				 * @return @c true if @ref State is @ref IO::State::Idle afterwards.
				 *
				 * Not idempotent. A second call while Idle returns @c false
				 * and leaves the session Idle.
				 */
				virtual bool Open() final;

				/**
				 * @brief Stop prefetch, drop caches, close the origin.
				 * @return @ref IO::Status::Ok. Always succeeds at this layer.
				 *
				 * Idempotent. Sets session state to @ref IO::State::Unavailable.
				 */
				virtual IO::Result Close() final;

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
				 * @return @c true when caches are empty and the origin is
				 *         exhausted, or after @ref Close.
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
				 * @param n Byte count. Zero serves the current window only.
				 * @param dest Caller FIFO. Overwritten on Ok / End with count > 0.
				 * @return Status and byte count written to @p dest.
				 */
				virtual IO::Result Read(std::size_t n, FIFO& dest) const final;

				/**
				 * @brief Copy @p n bytes into @p dest without consuming cache.
				 * @param n Byte count. Zero copies the current cache window.
				 * @param dest Caller FIFO. Overwritten on Ok / End with count > 0.
				 * @return Status and byte count written to @p dest.
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
				 * @param mode @ref Position::Absolute or @ref Position::Relative.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result Seek(std::ptrdiff_t offset, Position mode) const final;

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
				 * Cancels in-flight prefetch and trims the window before
				 * returning. Cached bytes past the new target may be discarded.
				 * Does not pull from the origin. Still waits for the worker.
				 */
				virtual void ReadAhead(std::size_t bytes);

				/**
				 * @brief Configured cache memory cap in bytes.
				 * @return Current cap. 0 means no cache and no prefetch.
				 */
				virtual std::size_t MaxMemory() const noexcept;

				/**
				 * @brief Set cache memory cap. Takes effect immediately.
				 * @param bytes Approximate maximum resident cache. 0 drops the window.
				 *
				 * Cancels prefetch and trims or drops the window before
				 * returning. Bytes that no longer fit are discarded.
				 * Waits for the worker; not an origin pull.
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
				 * prefetch. Does not trim the window.
				 */
				virtual void MaxWait(std::chrono::milliseconds wait);

				/**
				 * @}
				 */

			protected:
				/**
				 * @brief Construct an unopened coordinator (@ref IO::State::Unavailable).
				 * @param read_ahead Initial @ref ReadAhead in bytes.
				 * @param max_memory Initial @ref MaxMemory in bytes.
				 * @param max_wait Initial @ref MaxWait. @c 0ms = unlimited.
				 */
				explicit BufferedRead(std::size_t read_ahead = 0, std::size_t max_memory = 0,
					std::chrono::milliseconds max_wait = std::chrono::milliseconds{0});

				/**
				 * @brief Publish session state from a leaf hook.
				 * @param state New @ref IO::State.
				 *
				 * Called from @ref OriginOpen, @ref OriginClose and
				 * @ref OriginPull. Not for user code.
				 */
				void SetState(enum IO::State state) noexcept;

				/**
				 * @name Origin hooks
				 * @{
				 */

				/**
				 * @brief Arm the underlying device and @ref SetState.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginOpen() = 0;

				/**
				 * @brief Release the underlying device and @ref SetState Unavailable.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginClose() = 0;

				/**
				 * @brief Read up to @p n bytes from the device into @p dest.
				 * @param n Maximum bytes to transfer.
				 * @param dest Implementation FIFO (not the user destination).
				 * @return @ref IO::Status::Ok, @ref IO::Status::End,
				 *         @ref IO::Status::Error or @ref IO::Status::Failed.
				 *
				 * On @ref IO::Status::Error call @ref SetState with
				 * @ref IO::State::Fault or @ref IO::State::Unavailable.
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
				 * @return Length, or empty when unknown.
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
