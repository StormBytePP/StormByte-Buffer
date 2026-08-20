/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte.
 *
 * StormByte is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StormByte is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StormByte. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 */
namespace StormByte::Buffer {
	/**
	 * @class LockFreeRing
	 * @brief High-performance lock-free SPSC ring buffer (private / internal).
	 *
	 * Designed exclusively for intermediate @ref Pipeline stages.
	 * Correct behaviour is guaranteed only under Single-Producer /
	 * Single-Consumer access (the pattern used by the current Async Pipeline).
	 *
	 * - Lock-free data path (atomics + power-of-two circular buffer)
	 * - Grows automatically (doubles capacity) when full
	 * - Minimal overhead, good cache locality
	 * - Same @ref ReadWrite contract as @ref Ring for drop-in use inside Pipeline
	 *
	 * Blocking waits (when data is not yet available) use a mutex + condition
	 * variable; the data path itself stays lock-free.
	 *
	 * @warning Never share a LockFreeRing instance between multiple producers
	 *          or multiple consumers. Doing so is undefined behaviour.
	 *
	 * @note This class is marked @c STORMBYTE_BUFFER_PRIVATE and is not installed
	 *       as a public header.
	 *
	 * @see Pipeline, Ring, ReadWrite
	 */
	class STORMBYTE_BUFFER_PRIVATE LockFreeRing final : public ReadWrite {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Construct with an initial capacity (rounded up to power of two).
			 * @param initial_capacity Suggested starting size (default 1 MiB).
			 */
			explicit LockFreeRing(std::size_t initial_capacity = 1u << 20);

			/** @brief Copy constructor (deleted – not copyable). */
			LockFreeRing(const LockFreeRing&) = delete;

			/** @brief Copy assignment (deleted – not copyable). */
			LockFreeRing& operator=(const LockFreeRing&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source instance.
			 */
			LockFreeRing(LockFreeRing&& other) noexcept;

			/**
			 * @brief Move assignment.
			 * @param other Source instance.
			 * @return Reference to this instance.
			 */
			LockFreeRing& operator=(LockFreeRing&& other) noexcept;

			/** @brief Destructor. */
			~LockFreeRing() noexcept override = default;

			/** @} */

			/**
			 * @name Queries
			 * @{
			 */

			/**
			 * @brief Bytes available from the current read position.
			 * @return Unread byte count.
			 */
			std::size_t AvailableBytes() const noexcept override;

			/**
			 * @brief Whether the ring holds no unread data.
			 * @return @c true if empty.
			 */
			bool Empty() const noexcept override;

			/**
			 * @brief End-of-stream condition.
			 * @return @c true if closed (or in error) and no bytes remain.
			 */
			bool EoF() const noexcept override;

			/**
			 * @brief Whether @ref SetError() has been called.
			 * @return @c true if the buffer is in a permanent error state.
			 */
			bool HasError() const noexcept;

			/**
			 * @brief Whether the buffer can still be read.
			 * @return @c false in permanent error state.
			 */
			bool IsReadable() const noexcept override;

			/**
			 * @brief Whether the buffer accepts writes.
			 * @return @c false if closed or in error.
			 */
			bool IsWritable() const noexcept override;

			/**
			 * @brief Total number of bytes stored.
			 * @return Size in bytes.
			 */
			std::size_t Size() const noexcept override;

			/**
			 * @brief Snapshot of stored data (may rebuild an internal cache).
			 * @return Constant reference to a @ref DataType view.
			 * @warning Intended for diagnostics; prefer Read / Extract on the hot path.
			 */
			const DataType& Data() const noexcept override;

			/** @} */

			/**
			 * @name Maintenance / lifecycle
			 * @{
			 */

			/**
			 * @brief Discard already-consumed data up to the logical read position.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Clear all contents and reset positions.
			 * @details Does not clear closed / error flags.
			 */
			void Clear() noexcept override;

			/**
			 * @brief Close the buffer for further writes (SPSC-safe).
			 * @details Sets the closed flag and wakes any @c WaitFor() waiters.
			 *          Remaining bytes can still be read until @ref EoF().
			 */
			void Close() noexcept override;

			/**
			 * @brief Enter a permanent error state.
			 * @details Makes the buffer unreadable and unwritable and wakes waiters.
			 */
			void SetError() noexcept override;

			/**
			 * @brief Discard @p count unread bytes.
			 * @param count Number of bytes to drop.
			 * @return @c true on success, @c false if fewer bytes were available.
			 */
			bool Drop(const std::size_t& count) noexcept override;

			/**
			 * @brief Move the logical read position for non-destructive reads.
			 * @param offset Offset value.
			 * @param mode   @ref Position::Absolute or @ref Position::Relative.
			 */
			void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/** @} */

			/**
			 * @name Peek (non-destructive, does not advance)
			 * @{
			 */

			/**
			 * @brief Peek into a @ref DataType without advancing the read position.
			 * @param count Bytes to peek; 0 = all available.
			 * @param out   Destination.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			bool Peek(const std::size_t& count, DataType& out) const noexcept override;

			/**
			 * @brief Peek into a @ref WriteOnly without advancing the read position.
			 * @param count Bytes to peek; 0 = all available.
			 * @param out   Destination writer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			bool Peek(const std::size_t& count, WriteOnly& out) const noexcept override;

			/** @} */

			/**
			 * @name Read (non-destructive, advances position)
			 * @{
			 */

			/**
			 * @brief Non-destructive read into a @ref DataType.
			 * @param count Bytes to read; 0 = all available.
			 * @param out   Destination.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			bool Read(const std::size_t& count, DataType& out) const noexcept override;

			/**
			 * @brief Non-destructive read into a @ref WriteOnly.
			 * @param count Bytes to read; 0 = all available.
			 * @param out   Destination writer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			bool Read(const std::size_t& count, WriteOnly& out) const noexcept override;

			/**
			 * @brief Read until EoF into a @ref DataType.
			 * @param out Destination.
			 */
			void ReadUntilEoF(DataType& out) const noexcept override;

			/**
			 * @brief Read until EoF into a @ref WriteOnly.
			 * @param out Destination writer.
			 */
			void ReadUntilEoF(WriteOnly& out) const noexcept override;

			/** @} */

			/**
			 * @name Extract (destructive)
			 * @{
			 */

			/**
			 * @brief Extract bytes into a @ref DataType (consumes data).
			 * @param count Bytes to extract; 0 = all available.
			 * @param out   Destination.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			bool Extract(const std::size_t& count, DataType& out) noexcept override;

			/**
			 * @brief Extract bytes into a @ref WriteOnly (consumes data).
			 * @param count Bytes to extract; 0 = all available.
			 * @param out   Destination writer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			bool Extract(const std::size_t& count, WriteOnly& out) noexcept override;

			/**
			 * @brief Extract until EoF into a @ref DataType.
			 * @param out Destination.
			 */
			void ExtractUntilEoF(DataType& out) noexcept override;

			/**
			 * @brief Extract until EoF into a @ref WriteOnly.
			 * @param out Destination writer.
			 */
			void ExtractUntilEoF(WriteOnly& out) noexcept override;

			/** @} */

			/**
			 * @name Write
			 * @{
			 */

			/**
			 * @brief Append bytes from a @ref DataType (copy).
			 * @param count Number of bytes to write.
			 * @param data  Source vector.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const std::size_t& count, const DataType& data) noexcept override;

			/**
			 * @brief Append bytes from a @ref DataType (move path).
			 * @param count Number of bytes to write.
			 * @param data  Source vector.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const std::size_t& count, DataType&& data) noexcept override;

			/**
			 * @brief Append bytes from a @ref ReadOnly (copy path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const std::size_t& count, const ReadOnly& data) noexcept override;

			/**
			 * @brief Append bytes from a @ref ReadOnly (move / extract path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const std::size_t& count, ReadOnly&& data) noexcept override;

			/** @brief Bring @ref WriteOnly convenience Write overloads into scope. */
			using WriteOnly::Write;

			/** @} */

		private:
			/**
			 * @brief Kind of internal read operation.
			 */
			enum class Operation {
				Extract,	///< Destructive read
				Read,		///< Non-destructive read (advances logical position)
				Peek		///< Non-destructive peek (no advance)
			};

			std::vector<std::byte> m_storage;	///< Power-of-two circular storage.
			std::size_t            m_capacity = 0;	///< Current capacity (power of two).
			std::size_t            m_mask     = 0;	///< @c m_capacity - 1 for fast modulo.

			alignas(64) std::atomic<std::size_t> m_head{0};	///< Consumer index.
			alignas(64) std::atomic<std::size_t> m_tail{0};	///< Producer index.

			mutable std::atomic<std::size_t> m_logical{0};	///< Logical cursor for Read/Peek.

			std::atomic<bool> m_closed{false};	///< Closed-for-writes flag.
			std::atomic<bool> m_error{false};	///< Permanent error flag.

			mutable std::mutex              m_wait_mtx;	///< Mutex for blocking waits only.
			mutable std::condition_variable m_cv;		///< Signalled on data / close / error.

			mutable DataType m_data_cache;	///< Cache used by @ref Data().

			/**
			 * @brief Round @p v up to the next power of two.
			 * @param v Requested size.
			 * @return Power-of-two size (at least 1).
			 */
			static std::size_t RoundUpPow2(std::size_t v) noexcept;

			/**
			 * @brief Double capacity (producer side only; not thread-safe with concurrent grow).
			 */
			void Grow() noexcept;

			/**
			 * @brief Block until at least @p n bytes are available, or closed/error.
			 * @param n Requested byte count.
			 * @return @c false if closed or in error before @p n bytes are ready.
			 */
			bool WaitFor(std::size_t n) const;

			/**
			 * @brief Shared Extract / Read / Peek implementation into @ref DataType.
			 * @param count Requested bytes.
			 * @param out   Destination.
			 * @param op    Operation kind.
			 * @return @c true on success.
			 */
			bool ReadInternal(std::size_t count, DataType& out, Operation op) noexcept;

			/**
			 * @brief Append raw bytes (producer path).
			 * @param count Number of bytes.
			 * @param src   Source pointer.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool WriteInternal(std::size_t count, const std::byte* src) noexcept;
	};
}
