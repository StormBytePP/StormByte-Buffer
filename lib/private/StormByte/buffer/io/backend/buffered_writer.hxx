/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Buffer source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-Logger tree and
 * the rest of the StormByte suite it vendors), which remains under its own
 * license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#pragma once

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_writer.hxx>
#include <StormByte/buffer/io/typedefs.hxx>
#include <StormByte/buffer/visibility.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <thread>

namespace StormByte::Buffer {
    class LockFreeRing;
}

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
				 *
				 * Owns session flags, the optional SPSC ring, the logical cursor
				 * and the push worker. Invokes @c Origin* hooks on @c m_owner.
				 * Origin hooks never run while @c m_mutex is held.
				 */
				class STORMBYTE_BUFFER_PRIVATE BufferedWriter {
					public:
						/**
						 * @name Lifecycle
						 * @{
						 */

						/**
						 * @brief Bind to the public leaf and store policy knobs.
						 * @param owner Public instance (the most-derived object).
						 * @param write_chunk Initial WriteChunk in bytes.
						 * @param back_pressure Initial BackPressure in chunks.
						 * @param max_wait Initial MaxWait.
						 *
						 * Starts the worker thread. State is @ref State::Unavailable.
						 */
						BufferedWriter(IO::BufferedWriter& owner, std::size_t write_chunk,
							std::size_t back_pressure, std::chrono::milliseconds max_wait);

						/**
						 * @brief Copy constructor is deleted.
						 */
						BufferedWriter(const BufferedWriter&) = delete;

						/**
						 * @brief Move constructor is deleted.
						 */
						BufferedWriter(BufferedWriter&&) = delete;

						/**
						 * @brief Stop the worker. Does not call OriginClose.
						 */
						~BufferedWriter();

						/**
						 * @brief Copy assignment is deleted.
						 * @return *this.
						 */
						BufferedWriter& operator=(const BufferedWriter&) = delete;

						/**
						 * @brief Move assignment is deleted.
						 * @return *this.
						 */
						BufferedWriter& operator=(BufferedWriter&&) = delete;

						/**
						 * @}
						 */

						/**
						 * @brief Point hooks at a new public instance after a move.
						 * @param owner Destination public object.
						 */
						void Rebind(IO::BufferedWriter& owner) noexcept;

						/**
						 * @brief Whether the sink is prepared to write.
						 * @return @c true if @ref State is @ref State::Idle.
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
						 * @brief Publish the logical write offset from a leaf Seek.
						 * @param offset New Tell.
						 */
						void SetTell(std::size_t offset) noexcept;

						/**
						 * @name Session
						 * @{
						 */

						/**
						 * @brief Arm the origin.
						 * @return @c true if @ref State is Idle afterwards.
						 */
						bool Open();

						/**
						 * @brief Flush then close the origin.
						 * @return @c true if Unavailable afterwards; @c false on Fault.
						 */
						bool Close();

						/**
						 * @brief Join the worker and drop the ring. Does not call Origin*.
						 */
						void Shutdown();

						/**
						 * @brief Close then Open when currently armed.
						 * @return @c true if Idle afterwards.
						 */
						bool Rewind();

						/**
						 * @brief Whether the session is armed.
						 * @return @c true after a successful Open until Close.
						 */
						bool IsOpen() const noexcept;

						/**
						 * @brief Drain the ring to the origin.
						 * @return @ref Status::Ok, @ref Status::Error or @ref Status::Failed.
						 *         Never @ref Status::TryAgain.
						 */
						Result Flush();

						/**
						 * @brief Drop the ring without pushing and truncate the origin.
						 * @return @ref Status::Ok or @ref Status::Failed.
						 */
						Result Truncate();

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
						Result Write(const FIFO& src);

						/**
						 * @brief Write every unread byte of @p src.
						 * @param src Source FIFO. Read from the current position.
						 * @return Status and bytes accepted. Source untouched unless Ok.
						 */
						Result Write(FIFO& src);

						/**
						 * @brief Write the whole span.
						 * @param src Octets to copy.
						 * @return Status and bytes accepted. Source untouched unless Ok.
						 */
						Result Write(std::span<const std::byte> src);

						/**
						 * @}
						 */

						/**
						 * @brief Bytes accepted since Open or Truncate.
						 * @return Logical write offset.
						 */
						std::size_t Tell() const noexcept;

						/**
						 * @brief Bytes in the ring not yet pushed.
						 * @return 0 in direct mode or after a successful Flush.
						 */
						std::size_t Dirty() const noexcept;

						/**
						 * @brief Configured origin push unit.
						 * @return Bytes. 0 disables the ring.
						 */
						std::size_t WriteChunk() const noexcept;

						/**
						 * @brief Set origin push unit.
						 * @param bytes Chunk size. 0 disables the ring.
						 */
						void WriteChunk(std::size_t bytes);

						/**
						 * @brief Configured dirty cap in WriteChunk units.
						 * @return Chunk count. 0 disables the ring.
						 */
						std::size_t BackPressure() const noexcept;

						/**
						 * @brief Set dirty cap in WriteChunk units.
						 * @param chunks 0 disables the ring.
						 */
						void BackPressure(std::size_t chunks);

						/**
						 * @brief Wait cap for OriginPush.
						 * @return @c 0ms waits without limit.
						 */
						std::chrono::milliseconds MaxWait() const noexcept;

						/**
						 * @brief Set wait cap for OriginPush.
						 * @param wait @c 0ms = unlimited.
						 */
						void MaxWait(std::chrono::milliseconds wait);

						/**
						 * @brief Whether @p n bytes fit under the current ring cap.
						 * @param n Prospective Write size.
						 * @return @c true in direct mode, or if Dirty + n <= cap.
						 */
						bool WillWrite(std::size_t n) const noexcept;

					private:
						/**
						 * @brief Whether both knobs enable the ring.
						 * @return @c true if WriteChunk and BackPressure are non-zero.
						 */
						bool BufferedMode() const noexcept;

						/**
						 * @brief Dirty cap in bytes.
						 * @return BackPressure * WriteChunk, or 0 if direct.
						 */
						std::size_t PendingCap() const noexcept;

						/**
						 * @brief Whether @p bytes fit under BackPressure.
						 * @param bytes Payload size of the prospective Write.
						 * @return @c true if the Write may proceed.
						 */
						bool WouldAccept(std::size_t bytes) const noexcept;

						/**
						 * @brief Start @c m_worker if it is not joinable.
						 */
						void StartWorker();

						/**
						 * @brief Signal stop and join @c m_worker.
						 */
						void StopWorker();

						/**
						 * @brief Wake the worker to drain chunks or a flush.
						 */
						void RequestDrain() const;

						/**
						 * @brief Worker loop: wait, OriginPush spans, park.
						 */
						void Worker();

						/**
						 * @brief OriginPush @p data, retry short writes, honor MaxWait.
						 * @param data Contiguous octets.
						 * @return Ok when every byte was pushed; Error or Failed otherwise.
						 */
						Result PushAll(std::span<const std::byte> data) const;

						/**
						 * @brief Shared implementation of the public Write overloads.
						 * @param src Octets to accept.
						 * @return Status and bytes accepted.
						 */
						Result WriteSpan(std::span<const std::byte> src);

						IO::BufferedWriter* m_owner;				///< Public leaf (hooks).

						mutable std::mutex m_mutex;					///< Session + knobs.
						mutable std::condition_variable m_cv;		///< Worker / flush waits.

						std::size_t m_write_chunk {0};				///< Origin push unit.
						std::size_t m_back_pressure {0};			///< Cap in WriteChunk units.
						std::chrono::milliseconds m_max_wait {0};	///< OriginPush wait cap.

						enum State m_state { State::Unavailable };	///< Session state.
						bool m_open {false};						///< Session armed.
						mutable bool m_failed {false};				///< Permanent failure.
						mutable std::size_t m_tell {0};				///< Accepted bytes.

						std::unique_ptr<LockFreeRing> m_ring;		///< SPSC dirty bytes. Null in direct mode.

						mutable std::atomic<bool> m_stop {false};	///< Worker teardown.
						mutable std::atomic<bool> m_flush {false};	///< Drain entire ring.
						mutable bool m_drain_run {false};			///< Worker has work.
						std::thread m_worker;						///< Push thread.
				};
			}
		}
	}
}
