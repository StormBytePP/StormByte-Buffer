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

#include <vector>
#include <span>
#include <string>
#include <sstream>
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
	 * @class FIFO
	 * @brief Byte-oriented FIFO buffer with grow-on-demand and close/error support.
	 *
	 * @par Overview
	 * A contiguous growable buffer implemented atop @ref DataType that tracks a
	 * logical read position. It grows automatically to fit writes and supports
	 * efficient non-destructive reads and destructive extracts.
	 *
	 * @par Thread safety
	 * This class is **not thread-safe**. For concurrent access use @ref SharedFIFO
	 * or @ref Ring.
	 *
	 * @par Buffer behaviour
	 * Supports clear / clean, a movable read position for non-destructive reads,
	 * and a closed / error state to signal end-of-writes or permanent failure.
	 * Once closed, further writes fail; readers can still drain remaining data
	 * until @ref EoF().
	 *
	 * @see SharedFIFO, Ring, ReadWrite, Producer, Consumer
	 */
	class STORMBYTE_BUFFER_PUBLIC FIFO: public ReadWrite {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Default construct an empty FIFO.
			 */
			FIFO() noexcept = default;

			/**
			 * @brief Construct FIFO with initial data (copy).
			 * @param data Initial byte vector.
			 */
			inline FIFO(const DataType& data) noexcept
				: m_buffer(data), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO with initial data (move).
			 * @param data Initial byte vector (moved into the FIFO).
			 */
			inline FIFO(DataType&& data) noexcept
				: m_buffer(std::move(data)), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from an input range (copy / convert).
			 * @tparam R Input range whose elements are convertible to @c std::byte.
			 * @param r  Range to copy from.
			 * @note Disabled when @p R is already @ref DataType to avoid overload ambiguity.
			 */
			template<std::ranges::input_range R>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
				requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
				(!std::same_as<std::remove_cvref_t<R>, DataType>)
			inline FIFO(const R& r) noexcept
				: m_buffer(DataConvert(r)), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from an rvalue range (moves when @ref DataType).
			 * @tparam Rr Input range type.
			 * @param r   Range to consume.
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline FIFO(Rr&& r) noexcept
				: m_buffer(DataConvert(std::forward<Rr>(r))), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from a string view (no trailing NUL).
			 * @param sv Source characters.
			 */
			inline FIFO(std::string_view sv) noexcept
				: m_buffer(DataConvert(sv)), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from a null-terminated C string.
			 * @param s Source string (may be null → empty buffer).
			 */
			inline FIFO(const char* s) noexcept
				: FIFO(s ? std::string_view(s) : std::string_view()) {}

			/**
			 * @brief Copy construct, preserving buffer contents and state.
			 * @param other Source FIFO.
			 */
			FIFO(const FIFO& other) noexcept;

			/**
			 * @brief Move construct.
			 * @param other Source FIFO (left empty / unspecified but valid).
			 */
			FIFO(FIFO&& other) noexcept;

			/**
			 * @brief Virtual destructor.
			 */
			virtual ~FIFO() noexcept = default;

			/**
			 * @brief Copy assign, preserving buffer contents and state.
			 * @param other Source FIFO.
			 * @return Reference to this FIFO.
			 */
			FIFO& operator=(const FIFO& other);

			/**
			 * @brief Move assign.
			 * @param other Source FIFO.
			 * @return Reference to this FIFO.
			 */
			FIFO& operator=(FIFO&& other) noexcept;

			/** @} */

			/**
			 * @name Comparison
			 * @{
			 */

			/**
			 * @brief Equality comparison.
			 * @param other Other FIFO.
			 * @return @c true if buffer contents, read position and closed/error flags match.
			 */
			inline bool operator==(const FIFO& other) const noexcept {
				return m_buffer == other.m_buffer &&
					m_position_offset == other.m_position_offset &&
					m_closed == other.m_closed &&
					m_error == other.m_error;
			}

			/**
			 * @brief Inequality comparison.
			 * @param other Other FIFO.
			 * @return Negation of @ref operator==.
			 */
			inline bool operator!=(const FIFO& other) const noexcept {
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
			 * @see Size(), Read(), Extract(), Seek()
			 */
			inline virtual std::size_t AvailableBytes() const noexcept {
				const std::size_t current_size = m_buffer.size();
				return (m_position_offset <= current_size) ? (current_size - m_position_offset) : 0;
			}

			/**
			 * @brief Access the internal storage.
			 * @return Constant reference to the owned @ref DataType.
			 */
			inline virtual const DataType& Data() const noexcept override {
				return m_buffer;
			}

			/**
			 * @brief Whether the underlying storage is empty.
			 * @return @c true if @c m_buffer is empty.
			 * @note With a non-zero read position, @ref Empty() may be @c false while
			 *       @ref AvailableBytes() is zero.
			 * @see Size(), AvailableBytes()
			 */
			inline virtual bool Empty() const noexcept override {
				return m_buffer.empty();
			}

			/**
			 * @brief End-of-stream condition.
			 * @return @c true if in error, or closed with no remaining unread bytes.
			 */
			inline virtual bool EoF() const noexcept override {
				const std::size_t avail =
					(m_position_offset <= m_buffer.size())
						? (m_buffer.size() - m_position_offset)
						: 0;
				return m_error || (m_closed && avail == 0);
			}

			/**
			 * @brief Whether the buffer can be read.
			 * @return @c false in permanent error state.
			 */
			inline virtual bool IsReadable() const noexcept override {
				return !m_error;
			}

			/**
			 * @brief Whether the buffer accepts writes.
			 * @return @c false if closed or in error.
			 */
			inline virtual bool IsWritable() const noexcept override {
				return !m_closed && !m_error;
			}

			/**
			 * @brief Total number of bytes stored (including already-read prefix).
			 * @return Size of the internal buffer.
			 * @see Empty(), AvailableBytes()
			 */
			inline virtual std::size_t Size() const noexcept override {
				return m_buffer.size();
			}

			/** @} */

			/**
			 * @name Maintenance / lifecycle
			 * @{
			 */

			/**
			 * @brief Discard data from the start up to the current read position.
			 */
			virtual void Clean() noexcept override;

			/**
			 * @brief Clear all buffer contents and reset the read position.
			 * @details Does **not** clear the closed / error flags.
			 * @see Size(), Empty()
			 */
			inline virtual void Clear() noexcept override {
				m_buffer.clear();
				m_position_offset = 0;
			}

			/**
			 * @brief Mark the buffer closed for further writes.
			 * @details Subsequent @c Write() calls fail. Readers may still drain
			 *          remaining data until @ref AvailableBytes() is zero.
			 */
			inline void Close() noexcept override {
				m_closed = true;
			}

			/**
			 * @brief Discard @p count unread bytes and advance the read position.
			 * @param count Number of bytes to drop.
			 * @return @c true on success, @c false if fewer bytes were available.
			 * @see Read(), Seek()
			 */
			virtual bool Drop(const std::size_t& count) noexcept override;

			/**
			 * @brief Move the logical read position for non-destructive reads.
			 * @param offset Offset value.
			 * @param mode   @ref Position::Absolute or @ref Position::Relative.
			 * @details Position is clamped to @c [0, Size()]. Does not modify stored data.
			 */
			virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Enter permanent error state (unreadable and unwritable).
			 */
			inline void SetError() noexcept override {
				m_error = true;
			}

			/** @} */

			/**
			 * @name Extract (destructive)
			 * @{
			 */

			/**
			 * @brief Extract bytes into a @ref DataType.
			 * @param count     Bytes to extract; 0 = all available.
			 * @param outBuffer Destination (filled / resized as needed).
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Extract(const std::size_t& count, DataType& outBuffer) noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Extract);
			}

			/**
			 * @brief Extract bytes into a @ref WriteOnly.
			 * @param count     Bytes to extract; 0 = all available.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Extract);
			}

			/** @brief Bring @ref ReadOnly convenience Extract overloads into scope. */
			using ReadOnly::Extract;

			/**
			 * @brief Extract all remaining bytes until EoF into a @ref DataType.
			 * @param outBuffer Destination buffer.
			 */
			inline void ExtractUntilEoF(DataType& outBuffer) noexcept override {
				ReadUntilEoFInternal(outBuffer, Operation::Extract);
			}

			/**
			 * @brief Extract all remaining bytes until EoF into a @ref WriteOnly.
			 * @param outBuffer Destination writer.
			 */
			inline void ExtractUntilEoF(WriteOnly& outBuffer) noexcept override {
				ReadUntilEoFInternal(outBuffer, Operation::Extract);
			}

			/** @} */

			/**
			 * @name Read (non-destructive)
			 * @{
			 */

			/**
			 * @brief Non-destructive read into a @ref DataType (advances position).
			 * @param count     Bytes to read; 0 = all available.
			 * @param outBuffer Destination buffer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Read(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Read);
			}

			/**
			 * @brief Non-destructive read into a @ref WriteOnly (advances position).
			 * @param count     Bytes to read; 0 = all available.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Read);
			}

			/** @brief Bring @ref ReadOnly convenience Read overloads into scope. */
			using ReadOnly::Read;

			/**
			 * @brief Read all remaining bytes until EoF into a @ref DataType.
			 * @param outBuffer Destination buffer.
			 */
			inline void ReadUntilEoF(DataType& outBuffer) const noexcept override {
				const_cast<FIFO*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
			}

			/**
			 * @brief Read all remaining bytes until EoF into a @ref WriteOnly.
			 * @param outBuffer Destination writer.
			 */
			inline void ReadUntilEoF(WriteOnly& outBuffer) const noexcept override {
				const_cast<FIFO*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
			}

			/** @} */

			/**
			 * @name Peek (non-destructive, does not advance)
			 * @{
			 */

			/**
			 * @brief Peek into a @ref DataType without advancing the read position.
			 * @param count     Bytes to peek; 0 = all available.
			 * @param outBuffer Destination buffer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Peek(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
			}

			/**
			 * @brief Peek into a @ref WriteOnly without advancing the read position.
			 * @param count     Bytes to peek; 0 = all available.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
			}

			/** @} */

			/**
			 * @name Diagnostics
			 * @{
			 */

			/**
			 * @brief Hexdump of unread contents from the current read position.
			 * @param collumns   Bytes per line (0 → default 16). Parameter name kept for ABI.
			 * @param byte_limit Max bytes to include (0 → no limit).
			 * @return Formatted string: size / position / status, then hex/ASCII lines
			 *         (no trailing newline).
			 */
			virtual std::string HexDump(const std::size_t& columns = 16,
										const std::size_t& byte_limit = 0) const noexcept;

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
			inline bool Write(const std::size_t& count, const DataType& data) noexcept override {
				return WriteInternal(count, data);
			}

			/**
			 * @brief Append bytes from a @ref DataType (move).
			 * @param count Number of bytes to write.
			 * @param data  Source vector.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const std::size_t& count, DataType&& data) noexcept override {
				return WriteInternal(count, std::move(data));
			}

			/**
			 * @brief Append bytes from a @ref ReadOnly source (copy).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return WriteInternal(count, data);
			}

			/**
			 * @brief Append bytes from a @ref ReadOnly source (move / extract path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return WriteInternal(count, std::move(data));
			}

			/** @brief Bring @ref WriteOnly convenience Write overloads into scope. */
			using WriteOnly::Write;

			/** @} */

		protected:
			/**
			 * @brief Owned contiguous byte storage.
			 */
			DataType m_buffer;

			/**
			 * @brief Logical read offset from the start of @c m_buffer.
			 */
			mutable std::size_t m_position_offset {0};

			/**
			 * @brief Closed flag — once @c true, further writes fail.
			 */
			bool m_closed {false};

			/**
			 * @brief Permanent error flag — unreadable and unwritable.
			 */
			bool m_error {false};

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
			 * @param collumns     Bytes per line.
			 * @return Formatted lines (no header).
			 */
			static std::string FormatHexLines(std::span<const std::byte>& data,
											std::size_t start_offset,
											std::size_t columns) noexcept;

			/**
			 * @brief Build the hexdump header (size / position / status).
			 * @return Stream containing the header lines.
			 */
			virtual std::ostringstream HexDumpHeader() const noexcept;

			/**
			 * @name Internal read / write helpers
			 * @{
			 */

			/**
			 * @brief Shared implementation for Extract / Read / Peek into @ref DataType.
			 * @param count     Requested byte count (0 = all available).
			 * @param outBuffer Destination.
			 * @param flag      Operation kind.
			 * @return @c true on success.
			 */
			virtual bool ReadInternal(const std::size_t& count, DataType& outBuffer,
									const Operation& flag) noexcept;

			/**
			 * @brief Shared implementation for Extract / Read / Peek into @ref WriteOnly.
			 * @param count     Requested byte count (0 = all available).
			 * @param outBuffer Destination writer.
			 * @param flag      Operation kind.
			 * @return @c true on success.
			 */
			virtual bool ReadInternal(const std::size_t& count, WriteOnly& outBuffer,
									const Operation& flag) noexcept;

			/**
			 * @brief Drain until EoF into @ref DataType using @p flag semantics.
			 * @param outBuffer Destination.
			 * @param flag      Extract or Read.
			 */
			virtual void ReadUntilEoFInternal(DataType& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Drain until EoF into @ref WriteOnly using @p flag semantics.
			 * @param outBuffer Destination writer.
			 * @param flag      Extract or Read.
			 */
			virtual void ReadUntilEoFInternal(WriteOnly& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Append from @ref DataType (copy).
			 * @param count Number of bytes.
			 * @param src   Source.
			 * @return @c true on success, @c false if closed / error.
			 */
			virtual bool WriteInternal(const std::size_t& count, const DataType& src) noexcept;

			/**
			 * @brief Append from @ref DataType (move).
			 * @param count Number of bytes.
			 * @param src   Source.
			 * @return @c true on success, @c false if closed / error.
			 */
			virtual bool WriteInternal(const std::size_t& count, DataType&& src) noexcept;

			/**
			 * @brief Append from @ref ReadOnly (copy path).
			 * @param count Number of bytes.
			 * @param src   Source.
			 * @return @c true on success, @c false if closed / error.
			 */
			virtual bool WriteInternal(const std::size_t& count, const ReadOnly& src) noexcept;

			/**
			 * @brief Append from @ref ReadOnly (move / extract path).
			 * @param count Number of bytes.
			 * @param src   Source.
			 * @return @c true on success, @c false if closed / error.
			 */
			virtual bool WriteInternal(const std::size_t& count, ReadOnly&& src) noexcept;

			/** @} */
	};
}
