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

#include <StormByte/buffer/buffered_read.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/typedefs.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/buffer/visibility.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
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
		 * @brief Coordinated byte I/O: results and private backends.
		 */
		namespace IO {
			/**
			 * @class BufferedRead
			 * @brief Private implementation of @ref StormByte::Buffer::BufferedRead.
			 *
			 * Owns session flags, @ref State, the cache window, the logical
			 * cursor and the prefetch worker. Invokes @c Origin* hooks on
			 * @c m_owner.
			 */
			class STORMBYTE_BUFFER_PRIVATE BufferedRead {
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
					 *
					 * Starts the worker thread. State is @ref State::Unavailable.
					 */
					BufferedRead(Buffer::BufferedRead& owner, std::size_t read_ahead,
						std::size_t max_memory);

					/**
					 * @brief Copy constructor is deleted.
					 */
					BufferedRead(const BufferedRead&) = delete;

					/**
					 * @brief Move constructor is deleted.
					 */
					BufferedRead(BufferedRead&&) = delete;

					/**
					 * @brief Stop the worker. Does not call OriginClose.
					 */
					~BufferedRead();

					/**
					 * @brief Copy assignment is deleted.
					 * @return *this.
					 */
					BufferedRead& operator=(const BufferedRead&) = delete;

					/**
					 * @brief Move assignment is deleted.
					 * @return *this.
					 */
					BufferedRead& operator=(BufferedRead&&) = delete;

					/**
					 * @}
					 */

					/**
					 * @brief Point hooks at a new public instance after a move.
					 * @param owner Destination public object.
					 */
					void Rebind(Buffer::BufferedRead& owner) noexcept;

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
					 * @brief Flush prefetch, drop the window, close the origin.
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
					 * @return @c true when the window is empty and the origin
					 *         is exhausted, or after @ref Close.
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
					 */
					Result Read(std::size_t n, FIFO& dest) const;

					/**
					 * @brief Copy @p n bytes into @p dest without consuming.
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
					 * @brief Move the logical cursor.
					 */
					Result Seek(std::ptrdiff_t offset, Position mode) const;

					/**
					 * @brief Logical read offset in the stream.
					 */
					std::size_t Tell() const noexcept;

					/**
					 * @brief Whether the leaf origin can seek.
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
					 */
					bool IsSized() const noexcept;

					/**
					 * @brief Origin length when known.
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
					 */
					std::size_t ReadAhead() const noexcept;

					/**
					 * @brief Set prefetch length.
					 */
					void ReadAhead(std::size_t bytes);

					/**
					 * @brief Configured cache cap.
					 */
					std::size_t MaxMemory() const noexcept;

					/**
					 * @brief Set cache cap.
					 */
					void MaxMemory(std::size_t bytes);

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
					 * @brief Clear @c m_window and reset @c m_window_origin to @c m_tell.
					 */
					void DropWindow() const;

					/**
					 * @brief Synchronous @c OriginPull into the window until @p n or end.
					 */
					Result PullIntoWindow(std::size_t n) const;

					/**
					 * @brief Shared @c Read / @c Peek implementation.
					 */
					Result Serve(std::size_t n, FIFO& dest, bool consume) const;

					/**
					 * @brief Drop consumed prefix; cap the window by MaxMemory.
					 */
					void TrimWindow() const;

					Buffer::BufferedRead* m_owner;				///< Public leaf (hooks).

					mutable std::mutex m_mutex;					///< Session + window.
					mutable std::condition_variable m_cv;		///< Worker / flush waits.

					std::size_t m_read_ahead {0};				///< Prefetch target length.
					std::size_t m_max_memory {0};				///< Approximate cache cap.

					enum State m_state { State::Unavailable };	///< Session state.
					bool m_open {false};						///< Session armed (Open until Close).
					mutable bool m_failed {false};				///< Permanent failure.
					mutable bool m_origin_exhausted {false};	///< Device EOF (not public EoF).
					mutable std::size_t m_tell {0};				///< Logical stream cursor.

					mutable FIFO m_window;						///< Single cache span.
					mutable std::size_t m_window_origin {0};	///< Stream offset of window[0].

					mutable std::atomic<bool> m_stop {false};	///< Worker teardown.
					mutable std::atomic<bool> m_cancel_prefetch {false}; ///< Flush in-flight pull.
					mutable bool m_prefetch_run {false};		///< Worker has an active target.
					mutable std::size_t m_prefetch_target {0};	///< Desired window AvailableBytes.
					std::thread m_worker;						///< Prefetch thread.
			};
		}
	}
}
