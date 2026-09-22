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
				 * @class BufferedWriter
				 * @brief Private implementation of @ref StormByte::Buffer::IO::BufferedWriter.
				 */
				class BufferedWriter;

				/**
				 * @class Bridge
				 * @brief Private pump for @ref StormByte::Buffer::Bridge.
				 */
				class Bridge;
			}

			/**
			 * @class BufferedWriter
			 * @brief Coordinated binary write sink with optional chunked write-behind.
			 *
			 * Public base for byte destinations. Leaves implement the
			 * @c Origin* hooks and may override @ref Setup, @ref Seek,
			 * @ref Size and @ref WillWrite. They do not override
			 * @c Write, @c Flush, @c Open, @c Close, @c Rewind or
			 * @c Truncate.
			 *
			 * @par Binary only
			 * Octets only. No text mode.
			 *
			 * @par Session
			 * Construction is @ref State::Unavailable. A successful
			 * @ref Open moves to @ref State::Idle. @ref Close is
			 * idempotent, always @ref Flush then @ref OriginClose, and
			 * returns to @ref Unavailable on success or @ref Fault if
			 * Flush failed. @ref Open is not idempotent. @c Close then
			 * @c Open is a valid round-trip. Destructor of a leaf must
			 * call @ref Close while the leaf vtable is live.
			 *
			 * @c operator bool is true when @ref State is Idle.
			 *
			 * @par Write
			 * @c Write consumes the whole visible source or nothing
			 * (atomic). @c FIFO is read from the current read position
			 * (@c FIFO::Read is const; the cursor is mutable). The FIFO
			 * / span is left untouched on @ref Status::TryAgain,
			 * @ref Status::Failed and @ref Status::Error.
			 *
			 * @par WriteChunk / BackPressure
			 * Either knob @c 0 disables the ring: @c Write calls
			 * @ref OriginPush on the caller thread and blocks until the
			 * accepted bytes are pushed. Both knobs @c > 0 enable an
			 * internal SPSC ring. Capacity is
			 * @c BackPressure * WriteChunk bytes. A @c Write that would
			 * exceed that cap returns @ref Status::TryAgain.
			 * The worker pushes full @c WriteChunk spans when possible;
			 * short @ref OriginPush results are retried until complete
			 * or @ref Status::Error.
			 *
			 * Setters take effect immediately. They are not deferred to
			 * the next @c Write. Turning the ring off or lowering the cap
			 * below @ref Dirty flushes dirty bytes first and may block
			 * on @ref OriginPush / @ref OriginFlush.
			 *
			 * @par Flush / Truncate
			 * @ref Flush blocks, drains the ring including a short tail,
			 * calls @ref OriginFlush, and never returns @ref Status::TryAgain.
			 * @ref Truncate drops the ring without pushing and calls
			 * @ref OriginTruncate. @ref Tell becomes 0.
			 *
			 * @par Seek / Size
			 * @ref Seek and @ref Size are virtual so a remote File-family
			 * writer can override them. Default @ref Size is @ref Tell
			 * (includes @ref Dirty). Default @ref Seek fails. A file
			 * leaf flushes dirty bytes, seeks the origin and updates
			 * @ref Tell.
			 *
			 * @par MaxWait
			 * Applies to the next @ref OriginPush (direct @c Write or worker).
			 * @c 0ms waits without limit. The ring itself is not timed.
			 * Setting @c MaxWait does not abort an in-flight push.
			 *
			 * @par WillWrite
			 * Protected probe used by @ref Backend::Bridge. Default asks
			 * the ring cap. Leaves may tighten it (disk space, socket).
			 * The answer is indicative: another process, quotas or a
			 * network filesystem can still make the later @c Write fail.
			 *
			 * @par Movable, not copyable
			 * Move transfers @c m_io. The worker is not stopped. Moved-from
			 * is Unavailable.
			 *
			 * @see Status, State, Result, FIFO, Backend::BufferedWriter
			 */
			class STORMBYTE_BUFFER_PUBLIC BufferedWriter {
				friend class Backend::BufferedWriter;
				friend class Backend::Bridge;

				public:
					/**
					 * @name Lifecycle
					 * @{
					 */

					/**
					 * @brief Copy constructor is deleted.
					 */
					BufferedWriter(const BufferedWriter&) = delete;

					/**
					 * @brief Move constructor.
					 * @param other Instance to take from. Left Unavailable.
					 */
					BufferedWriter(BufferedWriter&& other) noexcept;

					/**
					 * @brief Virtual destructor. Stops the worker. Does not call Origin*.
					 *
					 * Leaves must call @ref Close in their destructor.
					 */
					virtual ~BufferedWriter() noexcept;

					/**
					 * @brief Copy assignment is deleted.
					 * @return *this.
					 */
					BufferedWriter& operator=(const BufferedWriter&) = delete;

					/**
					 * @brief Move assignment.
					 * @param other Instance to take from. Left Unavailable.
					 * @return *this.
					 */
					BufferedWriter& operator=(BufferedWriter&& other) noexcept;

					/**
					 * @}
					 */

					/**
					 * @brief Whether the sink is prepared to write.
					 * @return @c true if @ref State is @ref State::Idle.
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
					 * @return @c true if @ref State is Idle afterwards.
					 *
					 * Calls @ref Setup then the backend @c Open.
					 * Not idempotent.
					 */
					virtual bool Open() final;

					/**
					 * @brief Flush then close the origin.
					 * @return @c true if @ref State is Unavailable afterwards.
					 *
					 * Idempotent on an already closed instance. Blocking.
					 * Flush failure leaves @ref State::Fault and returns false.
					 */
					virtual bool Close() final;

					/**
					 * @brief Re-arm: @ref Close then @ref Open when currently armed.
					 * @return @c true if Idle afterwards.
					 */
					virtual bool Rewind() final;

					/**
					 * @brief Whether @ref Open succeeded and @ref Close has not.
					 * @return @c true if the session is armed.
					 */
					virtual bool IsOpen() const noexcept final;

					/**
					 * @brief Push every dirty byte to the origin and @ref OriginFlush.
					 * @return @ref Status::Ok, @ref Status::Error or
					 *         @ref Status::Failed. Never @ref Status::TryAgain.
					 *
					 * Blocking. @ref Tell is unchanged.
					 */
					virtual Result Flush() final;

					/**
					 * @brief Drop dirty bytes and truncate the origin.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 *
					 * Does not push the ring. Sets @ref Tell to 0.
					 */
					virtual Result Truncate() final;

					/**
					 * @}
					 */

					/**
					 * @name Write
					 * @{
					 */

					/**
					 * @brief Write every unread byte of @p src.
					 * @param src Source FIFO. Read from the current position.
					 * @return Status and bytes accepted. Source untouched unless Ok.
					 */
					virtual Result Write(const FIFO& src) final;

					/**
					 * @brief Write every unread byte of @p src.
					 * @param src Source FIFO. Read from the current position.
					 * @return Status and bytes accepted. Source untouched unless Ok.
					 */
					virtual Result Write(FIFO& src) final;

					/**
					 * @brief Write the whole span.
					 * @param src Octets to copy.
					 * @return Status and bytes accepted. Source untouched unless Ok.
					 */
					virtual Result Write(std::span<const std::byte> src) final;

					/**
					 * @}
					 */

					/**
					 * @name Position
					 * @{
					 */

					/**
					 * @brief Bytes accepted since Open or Truncate.
					 * @return Logical write offset.
					 */
					virtual std::size_t Tell() const noexcept;

					/**
					 * @brief Bytes in the ring not yet pushed.
					 * @return 0 in direct mode or after a successful Flush.
					 */
					virtual std::size_t Dirty() const noexcept;

					/**
					 * @brief Logical sink length in bytes.
					 * @return Length. Default is @ref Tell (includes @ref Dirty).
					 *
					 * Not optional. A file leaf returns
					 * max(filesystem size, Tell). A remote leaf overrides
					 * this.
					 */
					virtual std::size_t Size() const noexcept;

					/**
					 * @brief Move the write cursor.
					 * @param offset Byte offset.
					 * @param mode @ref Position::Absolute or @ref Position::Relative.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 *
					 * Default fails. A file or remote leaf overrides this,
					 * flushes dirty bytes, seeks the origin and updates
					 * @ref Tell.
					 */
					virtual Result Seek(std::ptrdiff_t offset, Position mode);

					/**
					 * @}
					 */

					/**
					 * @name Policy
					 * @{
					 */

					/**
					 * @brief Configured origin push unit.
					 * @return Bytes. 0 disables the ring (with BackPressure 0).
					 */
					virtual std::size_t WriteChunk() const noexcept;

					/**
					 * @brief Set origin push unit. Takes effect immediately.
					 * @param bytes Chunk size. 0 disables the ring.
					 *
					 * May block. A zero value or a smaller unit that now meets
					 * dirty bytes drains the ring through @ref Flush / the worker
					 * before the setter returns. Not deferred to the next Write.
					 */
					virtual void WriteChunk(std::size_t bytes);

					/**
					 * @brief Configured dirty cap in WriteChunk units.
					 * @return Chunk count. 0 disables the ring.
					 */
					virtual std::size_t BackPressure() const noexcept;

					/**
					 * @brief Set dirty cap in WriteChunk units. Takes effect immediately.
					 * @param chunks 0 disables the ring. Otherwise cap is chunks * WriteChunk.
					 *
					 * May block. Zero, or a cap below @ref Dirty, flushes dirty
					 * bytes before the setter returns. Not deferred to the next Write.
					 */
					virtual void BackPressure(std::size_t chunks);

					/**
					 * @brief Wait cap for OriginPush.
					 * @return @c 0ms waits without limit.
					 */
					virtual std::chrono::milliseconds MaxWait() const noexcept;

					/**
					 * @brief Set wait cap for OriginPush. Takes effect on the next push.
					 * @param wait @c 0ms = unlimited.
					 *
					 * Does not cancel an in-flight @ref OriginPush. Does not Flush.
					 */
					virtual void MaxWait(std::chrono::milliseconds wait);

					/**
					 * @}
					 */

				protected:
					/**
					 * @brief Construct an unopened coordinator (@ref State::Unavailable).
					 * @param write_chunk Initial @ref WriteChunk.
					 * @param back_pressure Initial @ref BackPressure.
					 * @param max_wait Initial @ref MaxWait.
					 */
					explicit BufferedWriter(std::size_t write_chunk = 0, std::size_t back_pressure = 0,
						std::chrono::milliseconds max_wait = std::chrono::milliseconds{0});

					/**
					 * @brief Publish session state from a leaf hook.
					 * @param state New @ref State.
					 */
					void SetState(enum State state) noexcept;

					/**
					 * @brief Publish the logical write offset from a leaf @ref Seek.
					 * @param offset New @ref Tell.
					 */
					void SetTell(std::size_t offset) noexcept;

					/**
					 * @brief Leaf policy hook. Called from @ref Open before the origin.
					 *
					 * Default does nothing. File uses it for the path-only ctor.
					 * The most-derived vtable is live.
					 */
					virtual void Setup();

					/**
					 * @brief Whether @p n more bytes can be accepted now.
					 * @param n Byte count to probe.
					 * @return @c true if the ring (or direct mode) can take @p n.
					 *
					 * Indicative. Another writer, quotas or the filesystem can
					 * still reject the later @c Write. Override to tighten
					 * (disk space, socket window). Used by @ref Backend::Bridge.
					 */
					virtual bool WillWrite(std::size_t n) const;

					/**
					 * @name Origin hooks
					 * @{
					 */

					/**
					 * @brief Arm the device and @ref SetState.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					virtual Result OriginOpen() = 0;

					/**
					 * @brief Release the device and @ref SetState Unavailable.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					virtual Result OriginClose() = 0;

					/**
					 * @brief Write @p data to the device. Do not buffer in the hook.
					 * @param data Contiguous octets. May be a full WriteChunk or a tail.
					 * @return Ok with count == data.size(), Ok with a short count
					 *         (backend retries), Error or Failed.
					 */
					virtual Result OriginPush(std::span<const std::byte> data) = 0;

					/**
					 * @brief Make accepted bytes visible on the device.
					 * @return @ref Status::Ok, Error or Failed.
					 *
					 * The base calls this after a completed direct Write and after
					 * Flush has drained the ring. Do not buffer here.
					 */
					virtual Result OriginFlush() = 0;

					/**
					 * @brief Discard origin contents. Network may no-op Ok.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 */
					virtual Result OriginTruncate() = 0;

					/**
					 * @brief Seek the origin to @p absolute.
					 * @param absolute Byte offset from the start.
					 * @return @ref Status::Ok or @ref Status::Failed.
					 *
					 * Default fails. File and remote leaves override this.
					 */
					virtual Result OriginSeek(std::size_t absolute);

					/**
					 * @}
					 */

				private:
					std::unique_ptr<Backend::BufferedWriter> m_io;	///< Private coordinator state.
			};
		}
	}
}
