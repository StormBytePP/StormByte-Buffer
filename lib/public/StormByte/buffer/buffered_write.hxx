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
		 * @brief Coordinated byte I/O: results and private backends.
		 */
		namespace IO {
			/**
			 * @class BufferedWrite
			 * @brief Private implementation of @ref StormByte::Buffer::BufferedWrite.
			 */
			class BufferedWrite;
		}

		/**
		 * @class BufferedWrite
		 * @brief Coordinated binary write sink with optional chunked write-behind.
		 *
		 * Public base for byte destinations. Leaves implement only the
		 * @c Origin* hooks. They do not override @c Write, @c Flush,
		 * @c Open, @c Close, @c Rewind or @c Truncate.
		 *
		 * @par Binary only
		 * Octets only. No text mode.
		 *
		 * @par Session
		 * Construction is @ref IO::State::Unavailable. A successful
		 * @ref Open moves to @ref IO::State::Idle. @ref Close is
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
		 * / span is left untouched on @ref IO::Status::TryAgain,
		 * @ref IO::Status::Failed and @ref IO::Status::Error.
		 *
		 * @par WriteChunk / BackPressure
		 * Either knob @c 0 disables the ring: @c Write calls
		 * @ref OriginPush on the caller thread and blocks until the
		 * accepted bytes are pushed. Both knobs @c > 0 enable an
		 * internal SPSC ring. Capacity is
		 * @c BackPressure * WriteChunk bytes. A @c Write that would
		 * exceed that cap returns @ref IO::Status::TryAgain.
		 * The worker pushes full @c WriteChunk spans when possible;
		 * short @ref OriginPush results are retried until complete
		 * or @ref IO::Status::Error.
		 *
		 * @par Flush / Truncate
		 * @ref Flush blocks, drains the ring including a short tail,
		 * and never returns @ref IO::Status::TryAgain.
		 * @ref Truncate drops the ring without pushing and calls
		 * @ref OriginTruncate. @ref Tell becomes 0.
		 *
		 * @par MaxWait
		 * Applies to @ref OriginPush (direct @c Write or worker).
		 * @c 0ms waits without limit. The ring itself is not timed.
		 *
		 * @par Movable, not copyable
		 * Move transfers @c m_io. The worker is not stopped. Moved-from
		 * is Unavailable.
		 *
		 * @see IO::Status, IO::State, IO::Result, FIFO, IO::BufferedWrite
		 */
		class STORMBYTE_BUFFER_PUBLIC BufferedWrite {
			friend class IO::BufferedWrite;

			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Copy constructor is deleted.
				 */
				BufferedWrite(const BufferedWrite&) = delete;

				/**
				 * @brief Move constructor.
				 * @param other Instance to take from. Left Unavailable.
				 */
				BufferedWrite(BufferedWrite&& other) noexcept;

				/**
				 * @brief Virtual destructor. Stops the worker. Does not call Origin*.
				 *
				 * Leaves must call @ref Close in their destructor.
				 */
				virtual ~BufferedWrite() noexcept;

				/**
				 * @brief Copy assignment is deleted.
				 * @return *this.
				 */
				BufferedWrite& operator=(const BufferedWrite&) = delete;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from. Left Unavailable.
				 * @return *this.
				 */
				BufferedWrite& operator=(BufferedWrite&& other) noexcept;

				/**
				 * @}
				 */

				/**
				 * @brief Whether the sink is prepared to write.
				 * @return @c true if @ref State is @ref IO::State::Idle.
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
				 * @return @c true if @ref State is Idle afterwards.
				 *
				 * Not idempotent.
				 */
				virtual bool Open() final;

				/**
				 * @brief Flush then close the origin.
				 * @return @c true if @ref State is Unavailable afterwards.
				 *
				 * Idempotent on an already closed instance. Blocking.
				 * Flush failure leaves @ref IO::State::Fault and returns false.
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
				 * @brief Push every dirty byte to the origin.
				 * @return @ref IO::Status::Ok, @ref IO::Status::Error or
				 *         @ref IO::Status::Failed. Never @ref IO::Status::TryAgain.
				 *
				 * Blocking. @ref Tell is unchanged.
				 */
				virtual IO::Result Flush() final;

				/**
				 * @brief Drop dirty bytes and truncate the origin.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 *
				 * Does not push the ring. Sets @ref Tell to 0.
				 */
				virtual IO::Result Truncate() final;

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
				virtual IO::Result Write(const FIFO& src) final;

				/**
				 * @brief Write every unread byte of @p src.
				 * @param src Source FIFO. Read from the current position.
				 * @return Status and bytes accepted. Source untouched unless Ok.
				 */
				virtual IO::Result Write(FIFO& src) final;

				/**
				 * @brief Write the whole span.
				 * @param src Octets to copy.
				 * @return Status and bytes accepted. Source untouched unless Ok.
				 */
				virtual IO::Result Write(std::span<const std::byte> src) final;

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
				 * @brief Set origin push unit.
				 * @param bytes Chunk size. 0 disables the ring.
				 */
				virtual void WriteChunk(std::size_t bytes);

				/**
				 * @brief Configured dirty cap in WriteChunk units.
				 * @return Chunk count. 0 disables the ring.
				 */
				virtual std::size_t BackPressure() const noexcept;

				/**
				 * @brief Set dirty cap in WriteChunk units.
				 * @param chunks 0 disables the ring. Otherwise cap is chunks * WriteChunk.
				 */
				virtual void BackPressure(std::size_t chunks);

				/**
				 * @brief Wait cap for OriginPush.
				 * @return @c 0ms waits without limit.
				 */
				virtual std::chrono::milliseconds MaxWait() const noexcept;

				/**
				 * @brief Set wait cap for OriginPush.
				 * @param wait @c 0ms = unlimited.
				 */
				virtual void MaxWait(std::chrono::milliseconds wait);

				/**
				 * @}
				 */

			protected:
				/**
				 * @brief Construct an unopened coordinator (@ref IO::State::Unavailable).
				 * @param write_chunk Initial @ref WriteChunk.
				 * @param back_pressure Initial @ref BackPressure.
				 * @param max_wait Initial @ref MaxWait.
				 */
				explicit BufferedWrite(std::size_t write_chunk = 0, std::size_t back_pressure = 0,
					std::chrono::milliseconds max_wait = std::chrono::milliseconds{0});

				/**
				 * @brief Publish session state from a leaf hook.
				 * @param state New @ref IO::State.
				 */
				void SetState(enum IO::State state) noexcept;

				/**
				 * @name Origin hooks
				 * @{
				 */

				/**
				 * @brief Arm the device and @ref SetState.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginOpen() = 0;

				/**
				 * @brief Release the device and @ref SetState Unavailable.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginClose() = 0;

				/**
				 * @brief Write @p data to the device. Do not buffer in the hook.
				 * @param data Contiguous octets. May be a full WriteChunk or a tail.
				 * @return Ok with count == data.size(), Ok with a short count
				 *         (backend retries), Error or Failed.
				 */
				virtual IO::Result OriginPush(std::span<const std::byte> data) = 0;

				/**
				 * @brief Discard origin contents. Network may no-op Ok.
				 * @return @ref IO::Status::Ok or @ref IO::Status::Failed.
				 */
				virtual IO::Result OriginTruncate() = 0;

				/**
				 * @}
				 */

			private:
				std::unique_ptr<IO::BufferedWrite> m_io;	///< Private coordinator state.
		};
	}
}
