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

#include <condition_variable>
#include <mutex>

/**
 * @namespace StormByte::Buffer
 * @brief Buffer module of the StormByte suite.
 */
namespace StormByte::Buffer {
	/**
	 * @class SharedFIFO
	 * @brief Thread-safe FIFO built on top of @ref FIFO.
	 *
	 * @par Overview
	 * SharedFIFO wraps the non-thread-safe @ref FIFO with a mutex and a
	 * condition variable to provide safe concurrent access from multiple
	 * producer / consumer threads. It preserves the byte-oriented FIFO
	 * semantics of @ref FIFO while adding blocking behaviour for reads and
	 * extracts.
	 *
	 * @par Blocking semantics
	 * - @c Read / @c Extract with @c count &gt; 0 block until that many bytes are
	 *   available from the current position, or until the FIFO is closed /
	 *   set to error via @ref Close() / @ref SetError().
	 * - If @c count == 0, the call returns immediately with all bytes currently
	 *   available (Extract may clear them).
	 *
	 * @par Close behaviour
	 * @ref Close() marks the FIFO as closed (base class flag) and notifies all
	 * waiting threads. Subsequent @c Write() calls fail. Waiters wake and
	 * complete using whatever data is presently available (which may be none).
	 *
	 * @par Error behaviour
	 * @ref SetError() puts the buffer into a permanent error state (unreadable
	 * and unwritable) and wakes all waiters.
	 *
	 * @par Seek behaviour
	 * @ref Seek() updates the non-destructive read position and notifies
	 * waiters so blocked readers can re-evaluate their predicates.
	 *
	 * @par Thread safety
	 * All public member functions are thread-safe. Mutators and accessors take
	 * the internal mutex as needed for a consistent view.
	 *
	 * @note Closed / error flags are owned by the base @ref FIFO. SharedFIFO
	 *       only protects access and notifies waiters.
	 *
	 * @see FIFO, Ring, Producer, Consumer
	 */
	class STORMBYTE_BUFFER_PUBLIC SharedFIFO : public FIFO {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Default construct an empty SharedFIFO.
			 * @details Behaves like @ref FIFO and may grow as needed.
			 */
			SharedFIFO() noexcept = default;

			/**
			 * @brief Construct with initial data (copy).
			 * @param data Initial byte vector.
			 */
			inline SharedFIFO(const DataType& data) : FIFO(data) {}

			/**
			 * @brief Construct with initial data (move).
			 * @param data Initial byte vector (moved into the base FIFO).
			 */
			inline SharedFIFO(DataType&& data) noexcept : FIFO(std::move(data)) {}

			/**
			 * @brief Construct from an input range (copy / convert).
			 * @tparam R Range whose value_type is convertible to @c std::byte.
			 * @param r Source range (disabled when already @ref DataType).
			 */
			template<std::ranges::input_range R>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
				requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
				(!std::same_as<std::remove_cvref_t<R>, DataType>)
			inline SharedFIFO(const R& r) noexcept : FIFO(r) {}

			/**
			 * @brief Construct from an rvalue range.
			 * @tparam Rr Range type.
			 * @param r Source range (moved when @ref DataType).
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline SharedFIFO(Rr&& r) noexcept : FIFO(std::forward<Rr>(r)) {}

			/**
			 * @brief Construct from a string view (no trailing NUL).
			 * @param sv Source characters.
			 */
			inline SharedFIFO(std::string_view sv) noexcept : FIFO(sv) {}

			/**
			 * @brief Construct from a null-terminated C string.
			 * @param s Source string (may be null → empty).
			 */
			inline SharedFIFO(const char* s) noexcept : FIFO(s) {}

			/**
			 * @brief Construct by copying a plain @ref FIFO.
			 * @param other Source FIFO.
			 */
			inline SharedFIFO(const FIFO& other) : FIFO(other) {}

			/**
			 * @brief Construct by moving a plain @ref FIFO.
			 * @param other Source FIFO (left empty after move).
			 */
			inline SharedFIFO(FIFO&& other) noexcept : FIFO(std::move(other)) {}

			/**
			 * @brief Copy constructor (deleted – synchronization primitives are not copyable).
			 */
			SharedFIFO(const SharedFIFO&) = delete;

			/**
			 * @brief Move constructor (deleted – mutex / condition_variable are not movable).
			 */
			SharedFIFO(SharedFIFO&&) = delete;

			/** @brief Virtual destructor. */
			virtual ~SharedFIFO() noexcept = default;

			/**
			 * @brief Copy-assign from a plain @ref FIFO.
			 * @param other Source FIFO.
			 * @return Reference to this SharedFIFO.
			 */
			SharedFIFO& operator=(const FIFO& other);

			/**
			 * @brief Move-assign from a plain @ref FIFO.
			 * @param other Source FIFO.
			 * @return Reference to this SharedFIFO.
			 */
			SharedFIFO& operator=(FIFO&& other) noexcept;

			/**
			 * @brief Copy assignment (deleted).
			 */
			SharedFIFO& operator=(const SharedFIFO&) = delete;

			/**
			 * @brief Move assignment (deleted – mutex / condition_variable are not movable).
			 */
			SharedFIFO& operator=(SharedFIFO&&) = delete;

			/** @} */

			/**
			 * @name Comparison
			 * @{
			 */

			/**
			 * @brief Equality comparison (thread-safe).
			 * @param other Other SharedFIFO.
			 * @return @c true if contents and state match (both mutexes held).
			 * @details Delegates to @ref FIFO::operator== under dual locks.
			 */
			bool operator==(const SharedFIFO& other) const noexcept;

			/**
			 * @brief Inequality comparison.
			 * @param other Other SharedFIFO.
			 * @return Negation of @ref operator==.
			 */
			inline bool operator!=(const SharedFIFO& other) const noexcept {
				return !(*this == other);
			}

			/** @} */

			/**
			 * @name Queries
			 * @{
			 */

			/**
			 * @brief Bytes available from the current read position (thread-safe).
			 * @return Unread byte count.
			 */
			virtual std::size_t AvailableBytes() const noexcept override;

			/**
			 * @brief Access the internal storage.
			 * @warning Not safe under concurrent mutation without external exclusion.
			 * @return Constant reference to the base @ref DataType.
			 */
			inline virtual const DataType& Data() const noexcept override {
				return m_buffer;
			}

			/**
			 * @brief Whether the underlying storage is empty (thread-safe).
			 * @return @c true if empty.
			 * @note With a non-zero read position, @ref Empty() may be @c false while
			 *       @ref AvailableBytes() is zero.
			 * @see Size(), AvailableBytes()
			 */
			virtual bool Empty() const noexcept override;

			/**
			 * @brief End-of-stream condition (thread-safe).
			 * @return @c true if closed (or in error) and no unread bytes remain.
			 */
			virtual bool EoF() const noexcept override;

			/**
			 * @brief Whether the buffer is in permanent error state (thread-safe).
			 * @return @c true after @ref SetError().
			 */
			bool HasError() const noexcept;

			/**
			 * @brief Whether the buffer can still be read (thread-safe).
			 * @return @c false in permanent error state.
			 * @see SetError(), IsWritable(), AvailableBytes(), EoF()
			 */
			inline virtual bool IsReadable() const noexcept override {
				std::scoped_lock lock(m_mutex);
				return FIFO::IsReadable();
			}

			/**
			 * @brief Whether the buffer accepts writes (thread-safe).
			 * @return @c false if closed or in error.
			 * @see Close(), SetError(), IsReadable()
			 */
			inline virtual bool IsWritable() const noexcept override {
				std::scoped_lock lock(m_mutex);
				return FIFO::IsWritable();
			}

			/**
			 * @brief Total number of bytes stored (thread-safe).
			 * @return Size in bytes.
			 * @see Empty(), AvailableBytes()
			 */
			virtual std::size_t Size() const noexcept override;

			/** @} */

			/**
			 * @name Maintenance / lifecycle
			 * @{
			 */

			/**
			 * @brief Discard data from the start up to the read position (thread-safe).
			 * @see FIFO::Clean()
			 */
			void Clean() noexcept override;

			/**
			 * @brief Clear all contents (thread-safe).
			 * @details Does **not** clear closed / error flags.
			 * @see FIFO::Clear()
			 */
			virtual void Clear() noexcept override;

			/**
			 * @brief Close for further writes (thread-safe).
			 * @details Marks closed via the base class and notifies all waiters.
			 *          Subsequent writes fail; remaining data stays readable.
			 * @see FIFO::Close(), IsWritable()
			 */
			virtual void Close() noexcept override;

			/**
			 * @brief Discard @p count unread bytes (thread-safe).
			 * @param count Number of bytes to drop.
			 * @return @c true on success, @c false otherwise.
			 * @details Notifies waiting readers after dropping.
			 * @see FIFO::Drop()
			 */
			virtual bool Drop(const std::size_t& count) noexcept override;

			/**
			 * @brief Move the logical read position (thread-safe).
			 * @param offset Offset value.
			 * @param mode @ref Position::Absolute or @ref Position::Relative.
			 * @details Position is clamped to @c [0, Size()]. Notifies waiters.
			 * @see Read(), Position
			 */
			virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Enter permanent error state (thread-safe).
			 * @details Marks error via the base class and notifies all waiters.
			 * @see FIFO::SetError(), IsReadable(), IsWritable()
			 */
			virtual void SetError() noexcept override;

			/** @} */

			/**
			 * @name Diagnostics
			 * @{
			 */

			/**
			 * @brief Thread-safe hexdump of unread contents.
			 * @param columns Bytes per line (0 → default 16).
			 * @param byte_limit Max bytes to include (0 → no limit).
			 * @return Formatted dump (size / position / status + hex/ASCII; no trailing newline).
			 * @details Acquires the mutex for a consistent snapshot.
			 */
			virtual std::string HexDump(const std::size_t& columns = 0,
										const std::size_t& byte_limit = 0) const noexcept override;

			/** @} */

		protected:
			// Closed/error state is owned by the base FIFO class.
			// SharedFIFO only protects access and notifies waiters.

		private:
			mutable std::mutex m_mutex;					///< Mutex protecting internal state
			mutable std::condition_variable_any m_cv;	///< Condition variable for blocking reads

			/**
			 * @brief Hexdump header (size / position / status).
			 * @return Stream with header lines (delegates to base semantics).
			 */
			std::ostringstream HexDumpHeader() const noexcept override;

			/**
			 * @brief Blocking Extract / Read / Peek into @ref DataType.
			 * @param count Requested bytes.
			 * @param outBuffer Destination.
			 * @param flag Operation kind.
			 * @return @c true on success, @c false on error or insufficient data after close.
			 */
			virtual bool ReadInternal(const std::size_t& count, DataType& outBuffer,
									const Operation& flag) noexcept override;

			/**
			 * @brief Blocking Extract / Read / Peek into @ref WriteOnly.
			 * @param count Requested bytes.
			 * @param outBuffer Destination writer.
			 * @param flag Operation kind.
			 * @return @c true on success, @c false on error or insufficient data after close.
			 */
			virtual bool ReadInternal(const std::size_t& count, WriteOnly& outBuffer,
									const Operation& flag) noexcept override;

			/**
			 * @brief Wait until at least @p n bytes are available, or closed/error.
			 * @param n Requested byte count; if 0, returns immediately.
			 * @param lock Caller-held unique_lock on @c m_mutex (still held on return).
			 * @note Returns when @ref Close() or @ref SetError() is called even if
			 *       fewer than @p n bytes are available.
			 * @see Close(), SetError(), IsReadable()
			 */
			void Wait(const std::size_t& n, std::unique_lock<std::mutex>& lock) const;

			/**
			 * @brief Append from @ref DataType (copy), under lock + notify.
			 * @param count Number of bytes.
			 * @param src Source.
			 * @return @c true on success, @c false if closed / error.
			 */
			virtual bool WriteInternal(const std::size_t& count, const DataType& src) noexcept override;

			/**
			 * @brief Append from @ref DataType (move), under lock + notify.
			 * @param count Number of bytes.
			 * @param src Source.
			 * @return @c true on success, @c false if closed / error.
			 */
			virtual bool WriteInternal(const std::size_t& count, DataType&& src) noexcept override;
	};
}
