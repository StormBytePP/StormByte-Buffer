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

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/string.hxx>

#include <condition_variable>
#include <deque>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <utility>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class Ring
	 * @brief High-performance, highly-concurrent thread-safe ring buffer.
	 *
	 * @par Overview
	 * Backed by @c std::deque&lt;std::byte&gt; with a logical read position.
	 * Uses @c std::shared_mutex so multiple readers can proceed concurrently
	 * while writers and destructive operations remain exclusive.
	 * Critical sections are kept as short as possible; blocked readers wait
	 * on a @c condition_variable_any.
	 *
	 * @par Thread safety
	 * Fully thread-safe for concurrent producers and consumers (many-to-many).
	 * Prefer @ref LockFreeRing inside @ref Pipeline intermediates for SPSC
	 * lock-free performance when applicable.
	 *
	 * @par Lifecycle
	 * @ref Close() stops further writes; remaining data can still be drained.
	 * @ref SetError() makes the buffer permanently unreadable and unwritable
	 * and wakes waiters.
	 *
	 * @see Producer, Consumer, LockFreeRing, ReadWrite, SharedFIFO
	 */
	class STORMBYTE_BUFFER_PUBLIC Ring: public ReadWrite {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/** @brief Default construct an empty Ring. */
			Ring() noexcept = default;

			/**
			 * @brief Construct with initial data (copy).
			 * @param data Source bytes.
			 */
			inline explicit Ring(const DataType& data) noexcept
				: m_buffer(data.begin(), data.end()) {}

			/**
			 * @brief Construct with initial data (move elements from vector).
			 * @param data Source bytes (moved element-wise into the deque).
			 */
			inline explicit Ring(DataType&& data) noexcept
				: m_buffer(std::make_move_iterator(data.begin()),
						std::make_move_iterator(data.end())) {}

			/**
			 * @brief Construct from an input range (copy / convert).
			 * @tparam R Range whose value_type is convertible to @c std::byte.
			 * @param r  Source range (disabled when already @ref DataType).
			 */
			template<std::ranges::input_range R>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
					requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
					(!std::same_as<std::remove_cvref_t<R>, DataType>)
			inline explicit Ring(const R& r) noexcept {
				auto converted = DataConvert(r);
				m_buffer.assign(converted.begin(), converted.end());
			}

			/**
			 * @brief Construct from an rvalue range.
			 * @tparam Rr Range type.
			 * @param r  Source range.
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
					requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline explicit Ring(Rr&& r) noexcept {
				auto converted = DataConvert(std::forward<Rr>(r));
				m_buffer.assign(std::make_move_iterator(converted.begin()),
								std::make_move_iterator(converted.end()));
			}

			/**
			 * @brief Construct from a string view (no trailing NUL).
			 * @param sv Source characters.
			 */
			inline explicit Ring(std::string_view sv) noexcept {
				auto converted = DataConvert(sv);
				m_buffer.assign(converted.begin(), converted.end());
			}

			/**
			 * @brief Construct from a null-terminated C string.
			 * @param s Source string (may be null → empty).
			 */
			inline explicit Ring(const char* s) noexcept
				: Ring(s ? std::string_view(s) : std::string_view{}) {}

			/** @brief Copy constructor (deleted – mutexes are not copyable). */
			Ring(const Ring&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source Ring.
			 */
			Ring(Ring&& other) noexcept;

			/** @brief Virtual destructor. */
			virtual ~Ring() noexcept = default;

			/** @brief Copy assignment (deleted). */
			Ring& operator=(const Ring&) = delete;

			/**
			 * @brief Move assignment.
			 * @param other Source Ring.
			 * @return Reference to this Ring.
			 */
			Ring& operator=(Ring&& other) noexcept;

			/** @} */

			/**
			 * @name Comparison
			 * @{
			 */

			/**
			 * @brief Equality comparison (contents and lifecycle state).
			 * @param other Other Ring.
			 * @return @c true if equal under the comparison rules.
			 */
			bool operator==(const Ring& other) const noexcept;

			/**
			 * @brief Inequality comparison.
			 * @param other Other Ring.
			 * @return Negation of @ref operator==.
			 */
			inline bool operator!=(const Ring& other) const noexcept {
				return !(*this == other);
			}

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
			 * @brief Snapshot of stored data (may rebuild an internal cache).
			 * @return Constant reference to a @ref DataType view.
			 */
			const DataType& Data() const noexcept override;

			/**
			 * @brief Whether the underlying storage is empty.
			 * @return @c true if empty.
			 */
			bool Empty() const noexcept override;

			/**
			 * @brief End-of-stream condition.
			 * @return @c true if closed (or in error) and no unread bytes remain.
			 */
			bool EoF() const noexcept override;

			/**
			 * @brief Whether @ref SetError() has been called.
			 * @return @c true in permanent error state.
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

			/** @} */

			/**
			 * @name Maintenance / lifecycle
			 * @{
			 */

			/**
			 * @brief Discard data from the start up to the current read position.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Clear all contents and reset the read position.
			 * @details Does not clear closed / error flags.
			 */
			void Clear() noexcept override;

			/**
			 * @brief Close the buffer for further writes.
			 * @details Subsequent writes fail; readers may still drain data.
			 *          Waiters are notified.
			 */
			void Close() noexcept override;

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

			/**
			 * @brief Enter permanent error state (unreadable and unwritable).
			 * @details Waiters are notified.
			 */
			void SetError() noexcept override;

			/** @} */

			/**
			 * @name Diagnostics
			 * @{
			 */

			/**
			 * @brief Hexdump of unread contents from the current read position.
			 * @param columns    Bytes per line (0 → default 16).
			 * @param byte_limit Max bytes to include (0 → no limit).
			 * @return Formatted diagnostic string.
			 */
			std::string HexDump(const std::size_t& columns = 16,
								const std::size_t& byte_limit = 0) const noexcept;

			/** @} */

			/**
			 * @name Peek (non-destructive, does not advance)
			 * @{
			 */
			bool Peek(const std::size_t& count, DataType& outBuffer) const noexcept override;
			bool Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override;
			/** @} */

			/**
			 * @name Read (non-destructive, advances position)
			 * @{
			 */
			bool Read(const std::size_t& count, DataType& outBuffer) const noexcept override;
			bool Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override;
			void ReadUntilEoF(DataType& outBuffer) const noexcept override;
			void ReadUntilEoF(WriteOnly& outBuffer) const noexcept override;
			/** @} */

			/**
			 * @name Extract (destructive)
			 * @{
			 */
			bool Extract(const std::size_t& count, DataType& outBuffer) noexcept override;
			bool Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept override;
			void ExtractUntilEoF(DataType& outBuffer) noexcept override;
			void ExtractUntilEoF(WriteOnly& outBuffer) noexcept override;
			/** @} */

			/**
			 * @name Write
			 * @{
			 */
			bool Write(const std::size_t& count, const DataType& data) noexcept override;
			bool Write(const std::size_t& count, DataType&& data) noexcept override;
			bool Write(const std::size_t& count, const ReadOnly& data) noexcept override;
			bool Write(const std::size_t& count, ReadOnly&& data) noexcept override;

			/** @brief Bring @ref WriteOnly convenience Write overloads into scope. */
			using WriteOnly::Write;
			/** @} */

		protected:
			/**
			 * @brief Kind of internal read operation.
			 */
			enum class Operation {
				Extract,	///< Destructive read
				Read,		///< Non-destructive read (advances position)
				Peek		///< Non-destructive peek (no advance)
			};

			/**
			 * @brief Format hex/ASCII lines for a data span.
			 * @param data         Bytes to dump.
			 * @param start_offset Display offset of the first byte.
			 * @param columns      Bytes per line.
			 * @return Formatted lines (no header).
			 */
			static std::string FormatHexLines(std::span<const std::byte> data,
											std::size_t start_offset,
											std::size_t columns) noexcept;

			/**
			 * @brief Build the hexdump header (size / position / status).
			 * @return Stream containing the header lines.
			 */
			virtual std::ostringstream HexDumpHeader() const noexcept;

			/**
			 * @name Internal helpers
			 * @{
			 */
			virtual bool ReadInternal(const std::size_t& count, DataType& outBuffer, Operation flag) noexcept;
			virtual bool ReadInternal(const std::size_t& count, WriteOnly& outBuffer, Operation flag) noexcept;
			virtual void ReadUntilEoFInternal(DataType& outBuffer, Operation flag) noexcept;
			virtual void ReadUntilEoFInternal(WriteOnly& outBuffer, Operation flag) noexcept;

			virtual bool WriteInternal(const std::size_t& count, const DataType& src) noexcept;
			virtual bool WriteInternal(const std::size_t& count, DataType&& src) noexcept;
			/** @} */

		private:
			std::deque<std::byte>               m_buffer;			///< Contiguous-ish byte storage.
			mutable std::size_t                 m_position_offset{0};	///< Logical read offset.
			bool                                m_closed{false};		///< Closed-for-writes flag.
			bool                                m_error{false};			///< Permanent error flag.
			std::string                         m_error_message;		///< Optional error detail.

			mutable DataType                    m_data_cache;			///< Cache for @ref Data().
			mutable std::shared_mutex           m_mutex;				///< Shared for readers, exclusive for writers.
			mutable std::condition_variable_any m_cv;					///< Signalled on data / close / error.

			/**
			 * @brief Block until at least @p n bytes are available, or closed/error.
			 * @param n    Requested byte count.
			 * @param lock Unique lock already held on @c m_mutex (released while waiting).
			 */
			void Wait(const std::size_t& n, std::unique_lock<std::shared_mutex>& lock) const;
	};
}
