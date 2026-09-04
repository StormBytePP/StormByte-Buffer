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

#include <StormByte/buffer/typedefs.hxx>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <string_view>
#include <utility>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including Generic buffers, thread-safe shared buffers, producer-consumer
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class Generic
	 * @brief Pure abstract root of the buffer interface hierarchy.
	 *
	 * @details Generic holds **no data members**. Concrete types that need
	 *          storage (e.g. @ref FIFO, @ref Ring) own it themselves.
	 *          Protected @c DataConvert helpers convert ranges / strings into
	 *          @ref DataType for constructors and writes.
	 *
	 * @see ReadOnly, WriteOnly, ReadWrite
	 */
	class STORMBYTE_BUFFER_PUBLIC Generic {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */
			/** @brief Default construct. */
			Generic() noexcept = default;

			/** @brief Copy construct. */
			Generic(const Generic&) noexcept = default;

			/** @brief Move construct. */
			Generic(Generic&&) noexcept = default;

			/**
			 * @brief Pure virtual destructor (keeps the class abstract).
			 */
			virtual ~Generic() noexcept = 0;

			/** @brief Copy assign. */
			Generic& operator=(const Generic& other) = default;

			/** @brief Move assign. */
			Generic& operator=(Generic&&) noexcept = default;
			/** @} */

		protected:
			/**
			 * @name DataConvert
			 * @brief Convert external sources into the library @ref DataType.
			 *
			 * Overload forms:
			 * - @c DataConvert(const Src&) — copy / convert from lvalue ranges
			 * - @c DataConvert(Src&&) — consume rvalue ranges; move when @p Src is @ref DataType
			 * - @c DataConvert(std::string_view) / @c DataConvert(const char*)
			 * @{
			 */

			/**
			 * @brief Convert an lvalue input range to @ref DataType.
			 * @tparam Src Input range whose value_type is convertible to @c std::byte.
			 * @param src Source range.
			 * @return Converted byte vector.
			 */
			template<std::ranges::input_range Src>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Src>>>) &&
				requires(std::ranges::range_value_t<Src> v) { static_cast<std::byte>(v); }
			static DataType DataConvert(const Src& src) noexcept {
				DataType out;
				if constexpr (requires { std::ranges::size(src); }) {
					auto s = std::ranges::size(src);
					if (s > 0) out.reserve(static_cast<typename DataType::size_type>(s));
				}
				std::transform(std::ranges::begin(src), std::ranges::end(src), std::back_inserter(out),
							[] (auto&& e) noexcept { return static_cast<std::byte>(e); });
				return out;
			}

			/**
			 * @brief Convert an rvalue input range to @ref DataType (moves when already @ref DataType).
			 * @tparam Src Input range type.
			 * @param src Source range (may be moved from).
			 * @return Converted or moved byte vector.
			 */
			template<std::ranges::input_range Src>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Src>>>) &&
				requires(std::ranges::range_value_t<Src> v) { static_cast<std::byte>(v); }
			static DataType DataConvert(Src&& src) noexcept {
				using Dec = std::remove_cvref_t<Src>;
				if constexpr (std::same_as<Dec, DataType>) {
					return std::move(src);
				} else {
					DataType out;
					if constexpr (requires { std::ranges::size(src); }) {
						auto s = std::ranges::size(src);
						if (s > 0) out.reserve(static_cast<typename DataType::size_type>(s));
					}
					std::transform(std::ranges::begin(src), std::ranges::end(src), std::back_inserter(out),
							[] (auto&& e) noexcept { return static_cast<std::byte>(e); });
					return out;
				}
			}

			/**
			 * @brief Convert a string view to @ref DataType (no trailing NUL).
			 * @param sv Source characters.
			 * @return Byte vector.
			 */
			static DataType DataConvert(std::string_view sv) noexcept {
				DataType out;
				if (!sv.empty()) out.reserve(static_cast<typename DataType::size_type>(sv.size()));
				std::transform(sv.begin(), sv.end(), std::back_inserter(out),
					[] (char c) noexcept { return static_cast<std::byte>(c); });
				return out;
			}

			/**
			 * @brief Convert a null-terminated C string to @ref DataType.
			 * @param s Source string (may be null → empty vector).
			 * @return Byte vector.
			 */
			static DataType DataConvert(const char* s) noexcept {
				if (!s) return DataType{};
				return DataConvert(std::string_view(s));
			}

			/** @} */
	};

	class WriteOnly; // Forward declaration

	/**
	 * @class ReadOnly
	 * @brief Pure interface for a buffer that can be read but not written.
	 *
	 * @details All read / extract / peek / seek contracts used by concrete
	 *          buffers (@ref FIFO, @ref Ring, @ref Consumer, …) are declared here.
	 *
	 * @see WriteOnly, ReadWrite, Generic
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadOnly: virtual public Generic {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */
			/** @brief Default construct. */
			inline ReadOnly() noexcept: Generic() {}

			/** @brief Copy construct. */
			ReadOnly(const ReadOnly&) noexcept = default;

			/** @brief Move construct. */
			ReadOnly(ReadOnly&&) noexcept = default;

			/** @brief Virtual destructor. */
			virtual ~ReadOnly() noexcept = default;

			/** @brief Copy assign. */
			ReadOnly& operator=(const ReadOnly&) = default;

			/** @brief Move assign. */
			ReadOnly& operator=(ReadOnly&&) noexcept = default;
			/** @} */

			/**
			 * @name Queries
			 * @{
			 */

			/**
			 * @brief Bytes available from the current read position.
			 * @return Unread byte count.
			 */
			virtual std::size_t AvailableBytes() const noexcept = 0;

			/**
			 * @brief Access a view of the internal storage (implementation-defined).
			 * @return Constant reference to a @ref DataType.
			 */
			virtual const DataType& Data() const noexcept = 0;

			/**
			 * @brief Whether the buffer holds no stored bytes.
			 * @return @c true if empty.
			 * @see Size()
			 */
			virtual bool Empty() const noexcept = 0;

			/**
			 * @brief End-of-stream condition.
			 * @return @c true when closed (or in error) and no unread bytes remain.
			 */
			virtual bool EoF() const noexcept = 0;

			/**
			 * @brief Whether the buffer can still be read.
			 * @return @c false in permanent error state.
			 */
			virtual bool IsReadable() const noexcept = 0;

			/**
			 * @brief Total number of bytes stored.
			 * @return Size in bytes.
			 * @see Empty()
			 */
			virtual std::size_t Size() const noexcept = 0;

			/** @} */

			/**
			 * @name Maintenance
			 * @{
			 */

			/**
			 * @brief Discard data from the start up to the current read position.
			 * @see Size(), Empty()
			 */
			virtual void Clean() noexcept = 0;

			/**
			 * @brief Clear all buffer contents and reset logical positions.
			 * @details Closed / error flags are implementation-defined (typically preserved).
			 * @see Size(), Empty()
			 */
			virtual void Clear() noexcept = 0;

			/**
			 * @brief Discard @p count unread bytes.
			 * @param count Number of bytes to drop.
			 * @return @c true on success, @c false if fewer bytes were available.
			 * @see Read()
			 */
			virtual bool Drop(const std::size_t& count) noexcept = 0;

			/**
			 * @brief Move the logical read position for non-destructive reads.
			 * @param offset Offset value.
			 * @param mode   @ref Position::Absolute or @ref Position::Relative.
			 * @details Position is clamped to @c [0, Size()]. Absolute + negative offset is a no-op.
			 * @see Read(), Position
			 */
			virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept = 0;

			/** @} */

			/**
			 * @name Extract (destructive)
			 * @{
			 */

			/**
			 * @brief Extract bytes into a @ref DataType.
			 * @param count     Bytes to extract; 0 = all available.
			 * @param outBuffer Destination.
			 * @return @c true on success, @c false on failure.
			 */
			inline virtual bool Extract(const std::size_t& count, DataType& outBuffer) noexcept = 0;

			/**
			 * @brief Extract all available bytes into a @ref DataType.
			 * @param outBuffer Destination.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Extract(DataType& outBuffer) noexcept {
				return Extract(0, outBuffer);
			}

			/**
			 * @brief Extract bytes into a @ref WriteOnly.
			 * @param count     Bytes to extract; 0 = all available.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			inline virtual bool Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept = 0;

			/**
			 * @brief Extract all available bytes into a @ref WriteOnly.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Extract(WriteOnly& outBuffer) noexcept {
				return Extract(0, outBuffer);
			}

			/**
			 * @brief Extract until EoF into a @ref DataType.
			 * @param outBuffer Destination.
			 */
			virtual void ExtractUntilEoF(DataType& outBuffer) noexcept = 0;

			/**
			 * @brief Extract until EoF into a @ref WriteOnly.
			 * @param outBuffer Destination writer.
			 */
			virtual void ExtractUntilEoF(WriteOnly& outBuffer) noexcept = 0;

			/** @} */

			/**
			 * @name Peek (non-destructive, does not advance)
			 * @{
			 */

			/**
			 * @brief Peek into a @ref DataType without advancing the read position.
			 * @param count     Bytes to peek; 0 = all available.
			 * @param outBuffer Destination.
			 * @return @c true on success, @c false if insufficient data or error.
			 *
			 * @details
			 * - @c count == 0: all available bytes (fails if none).
			 * - @c count > 0: exactly @c count bytes, or fail if fewer are available.
			 *
			 * @see Read(), Seek()
			 */
			virtual bool Peek(const std::size_t& count, DataType& outBuffer) const noexcept = 0;

			/**
			 * @brief Peek into a @ref WriteOnly without advancing the read position.
			 * @param count     Bytes to peek; 0 = all available.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false if insufficient data or error.
			 *
			 * @details Same count semantics as the @ref DataType overload.
			 * @see Read(), Seek()
			 */
			virtual bool Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept = 0;

			/** @} */

			/**
			 * @name Read (non-destructive, advances position)
			 * @{
			 */

			/**
			 * @brief Read into a @ref DataType.
			 * @param count     Bytes to read; 0 = all available.
			 * @param outBuffer Destination.
			 * @return @c true on success, @c false on failure.
			 */
			virtual bool Read(const std::size_t& count, DataType& outBuffer) const noexcept = 0;

			/**
			 * @brief Read all available bytes into a @ref DataType.
			 * @param outBuffer Destination.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Read(DataType& outBuffer) const noexcept {
				return Read(0, outBuffer);
			}

			/**
			 * @brief Read into a @ref WriteOnly.
			 * @param count     Bytes to read; 0 = all available.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			virtual bool Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept = 0;

			/**
			 * @brief Read all available bytes into a @ref WriteOnly.
			 * @param outBuffer Destination writer.
			 * @return @c true on success, @c false on failure.
			 */
			inline bool Read(WriteOnly& outBuffer) const noexcept {
				return Read(0, outBuffer);
			}

			/**
			 * @brief Read until EoF into a @ref DataType.
			 * @param outBuffer Destination.
			 */
			virtual void ReadUntilEoF(DataType& outBuffer) const noexcept = 0;

			/**
			 * @brief Read until EoF into a @ref WriteOnly.
			 * @param outBuffer Destination writer.
			 */
			virtual void ReadUntilEoF(WriteOnly& outBuffer) const noexcept = 0;

			/** @} */
	};

	/**
	 * @class WriteOnly
	 * @brief Pure interface for a buffer that can be written but not read.
	 *
	 * @details Declares lifecycle (@ref Close / @ref SetError), writability, and
	 *          the canonical @c Write overloads. Convenience overloads for strings,
	 *          ranges and iterators are implemented inline in terms of those.
	 *
	 * @see ReadOnly, ReadWrite, Generic
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteOnly: virtual public Generic {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */
			/** @brief Default construct. */
			inline WriteOnly() noexcept: Generic() {}

			/** @brief Copy construct. */
			WriteOnly(const WriteOnly&) = default;

			/** @brief Move construct. */
			WriteOnly(WriteOnly&&) noexcept = default;

			/** @brief Virtual destructor. */
			virtual ~WriteOnly() noexcept = default;

			/** @brief Copy assign. */
			WriteOnly& operator=(const WriteOnly&) = default;

			/** @brief Move assign. */
			WriteOnly& operator=(WriteOnly&&) noexcept = default;
			/** @} */

			/**
			 * @name Lifecycle / queries
			 * @{
			 */

			/**
			 * @brief Whether the buffer accepts writes.
			 * @return @c false if closed or in error state.
			 */
			virtual bool IsWritable() const noexcept = 0;

			/**
			 * @brief Mark the buffer closed for further writes.
			 * @details Subsequent @c Write() calls must fail. Readers may still drain data.
			 *          Thread-safe implementations should wake blocked waiters.
			 */
			virtual void Close() noexcept = 0;

			/**
			 * @brief Enter a permanent error state (unreadable and unwritable).
			 * @details Implementations should wake any blocked waiters.
			 */
			virtual void SetError() noexcept = 0;

			/** @} */

			/**
			 * @name Write (canonical pure virtuals)
			 * @{
			 */

			/**
			 * @brief Append bytes from a @ref DataType (copy).
			 * @param count Number of bytes to write.
			 * @param data  Source vector.
			 * @return @c true on success, @c false if closed / error.
			 * @see IsWritable()
			 */
			virtual bool Write(const std::size_t& count, const DataType& data) noexcept = 0;

			/**
			 * @brief Append bytes from a @ref DataType (move).
			 * @param count Number of bytes to write.
			 * @param data  Source vector.
			 * @return @c true on success, @c false if closed / error.
			 * @see IsWritable()
			 */
			virtual bool Write(const std::size_t& count, DataType&& data) noexcept = 0;

			/**
			 * @brief Append bytes from a @ref ReadOnly (copy path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 * @see IsWritable()
			 */
			virtual bool Write(const std::size_t& count, const ReadOnly& data) noexcept = 0;

			/**
			 * @brief Append bytes from a @ref ReadOnly (move / extract path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 * @see IsWritable()
			 */
			virtual bool Write(const std::size_t& count, ReadOnly&& data) noexcept = 0;

			/** @} */

			/**
			 * @name Write (ReadOnly conveniences)
			 * @{
			 */

			/**
			 * @brief Append all available bytes from a @ref ReadOnly (copy).
			 * @param data Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const ReadOnly& data) noexcept {
				return Write(data.AvailableBytes(), data);
			}

			/**
			 * @brief Append all available bytes from a @ref ReadOnly (move path).
			 * @param data Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(ReadOnly&& data) noexcept {
				return Write(data.AvailableBytes(), std::move(data));
			}

			/** @} */

			/**
			 * @name Write (string conveniences)
			 * @brief Stable overloads so string / C-string calls do not hit range templates.
			 *        String literals exclude the trailing NUL.
			 * @{
			 */

			/**
			 * @brief Write a string view (no trailing NUL).
			 * @param sv Source characters.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(std::string_view sv) noexcept {
				DataType tmp;
				if (!sv.empty()) tmp.reserve(static_cast<typename DataType::size_type>(sv.size()));
				std::transform(sv.begin(), sv.end(), std::back_inserter(tmp),
					[] (char e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
			}

			/**
			 * @brief Write a null-terminated C string.
			 * @param s Source (may be null → empty write).
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const char* s) noexcept {
				if (!s) return Write(DataType{});
				return Write(std::string_view(s));
			}

			/**
			 * @brief Write up to @p count characters from a string view.
			 * @param count Maximum characters (0 = entire view).
			 * @param sv    Source characters.
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const std::size_t& count, std::string_view sv) noexcept {
				size_t to_write = (count == 0) ? static_cast<size_t>(sv.size())
											: std::min(count, static_cast<std::size_t>(sv.size()));
				DataType tmp;
				if (to_write > 0) tmp.reserve(static_cast<typename DataType::size_type>(to_write));
				std::transform(sv.begin(), sv.begin() + to_write, std::back_inserter(tmp),
					[] (char e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(to_write), std::move(tmp));
			}

			/**
			 * @brief Write up to @p count characters from a C string.
			 * @param count Maximum characters (0 = entire string).
			 * @param s     Source (may be null).
			 * @return @c true on success, @c false if closed / error.
			 */
			bool Write(const std::size_t& count, const char* s) noexcept {
				if (!s) return Write(count, DataType{});
				return Write(count, std::string_view(s));
			}

			/**
			 * @brief Write a string literal without the trailing NUL.
			 * @tparam N Array extent (includes NUL for literals).
			 * @param s  Literal / char array.
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::size_t N>
			bool Write(const char (&s)[N]) noexcept {
				if (N == 0) return Write(DataType{});
				return Write(std::string_view(s, (N > 0) ? (N - 1) : 0));
			}

			/** @} */

			/**
			 * @name Write (range / iterator conveniences)
			 * @{
			 */

			/**
			 * @brief Write all elements from an input range.
			 * @tparam R Range whose value_type is convertible to @c std::byte.
			 * @param r  Source range.
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::ranges::input_range R>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
				requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); }
			bool Write(const R& r) noexcept {
				DataType tmp;
				if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
					auto dist = std::ranges::distance(r);
					if (dist > 0) tmp.reserve(static_cast<typename DataType::size_type>(dist));
				}
				std::transform(std::ranges::begin(r), std::ranges::end(r), std::back_inserter(tmp),
						[] (auto&& e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
			}

			/**
			 * @brief Write up to @p count elements from an input range.
			 * @tparam Rw Range type.
			 * @param count Maximum elements (0 = entire range).
			 * @param r     Source range.
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::ranges::input_range Rw>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rw>>>) &&
				requires(std::ranges::range_value_t<Rw> v) { static_cast<std::byte>(v); }
			bool Write(const std::size_t& count, const Rw& r) noexcept {
				if (count == 0) return Write(r);
				DataType tmp;
				if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
					auto dist = std::ranges::distance(r);
					if (dist > 0)
						tmp.reserve(static_cast<typename DataType::size_type>(
							std::min(dist, static_cast<decltype(dist)>(count))));
				}
				auto it = std::ranges::begin(r);
				auto end = std::ranges::end(r);
				std::size_t written = 0;
				for (; it != end && written < count; ++it, ++written) {
					tmp.push_back(static_cast<std::byte>(*it));
				}
				return Write(static_cast<std::size_t>(written), std::move(tmp));
			}

			/**
			 * @brief Write up to @p count elements from an rvalue range.
			 * @tparam Rrw Range type (may be @ref DataType rvalue).
			 * @param count Maximum elements (0 = entire range).
			 * @param r     Source range (moved when @ref DataType).
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::ranges::input_range Rrw>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rrw>>>) &&
				requires(std::ranges::range_value_t<Rrw> v) { static_cast<std::byte>(v); }
			bool Write(const std::size_t& count, Rrw&& r) noexcept {
				using Dec = std::remove_cvref_t<Rrw>;
				if (count == 0) return Write(std::forward<Rrw>(r));
				if constexpr (std::same_as<Dec, DataType>) {
					DataType tmp = std::move(r);
					if (tmp.size() > count) tmp.resize(count);
					return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
				} else {
					DataType tmp;
					if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
						auto dist = std::ranges::distance(r);
						if (dist > 0)
							tmp.reserve(static_cast<typename DataType::size_type>(
								std::min(dist, static_cast<decltype(dist)>(count))));
					}
					auto it = std::ranges::begin(r);
					auto end = std::ranges::end(r);
					std::size_t written = 0;
					for (; it != end && written < count; ++it, ++written) {
						tmp.push_back(static_cast<std::byte>(*it));
					}
					return Write(static_cast<std::size_t>(written), std::move(tmp));
				}
			}

			/**
			 * @brief Write all elements from an rvalue range.
			 * @tparam Rr Range type.
			 * @param r  Source (moved when @ref DataType).
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::ranges::input_range Rr>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			bool Write(Rr&& r) noexcept {
				using Dec = std::remove_cvref_t<Rr>;
				if constexpr (std::same_as<Dec, DataType>) {
					return Write(static_cast<std::size_t>(r.size()), std::move(r));
				} else {
					DataType tmp;
					if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
						auto dist = std::ranges::distance(r);
						if (dist > 0) tmp.reserve(static_cast<typename DataType::size_type>(dist));
					}
					std::transform(std::ranges::begin(r), std::ranges::end(r), std::back_inserter(tmp),
						[] (auto&& e) noexcept { return static_cast<std::byte>(e); });
					return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
				}
			}

			/**
			 * @brief Write all elements from an iterator pair.
			 * @tparam I Input iterator.
			 * @tparam S Sentinel for @p I.
			 * @param first Begin iterator.
			 * @param last  End sentinel.
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::input_iterator I, std::sentinel_for<I> S>
				requires (!std::is_class_v<std::remove_cv_t<std::iter_value_t<I>>>) &&
				requires(std::iter_value_t<I> v) { static_cast<std::byte>(v); }
			bool Write(I first, S last) noexcept {
				DataType tmp;
				std::transform(first, last, std::back_inserter(tmp),
					[] (auto&& e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
			}

			/**
			 * @brief Write up to @p count elements from an iterator pair.
			 * @tparam I2 Input iterator.
			 * @tparam S2 Sentinel for @p I2.
			 * @param count Maximum elements (0 = all).
			 * @param first Begin iterator.
			 * @param last  End sentinel.
			 * @return @c true on success, @c false if closed / error.
			 */
			template<std::input_iterator I2, std::sentinel_for<I2> S2>
				requires (!std::is_class_v<std::remove_cv_t<std::iter_value_t<I2>>>) &&
				requires(std::iter_value_t<I2> v) { static_cast<std::byte>(v); }
			bool Write(const std::size_t& count, I2 first, S2 last) noexcept {
				if (count == 0) return Write(first, last);
				DataType tmp;
				std::size_t written = 0;
				for (; first != last && written < count; ++first, ++written) {
					tmp.push_back(static_cast<std::byte>(*first));
				}
				return Write(static_cast<std::size_t>(written), std::move(tmp));
			}

			/** @} */
	};

	/**
	 * @class ReadWrite
	 * @brief Pure interface combining @ref ReadOnly and @ref WriteOnly.
	 *
	 * @details Concrete bidirectional buffers (@ref FIFO, @ref Ring, …) implement this.
	 *
	 * @see ReadOnly, WriteOnly, Generic
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadWrite: public ReadOnly, public WriteOnly {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */
			/** @brief Default construct. */
			inline ReadWrite() noexcept: Generic() {}

			/** @brief Copy construct. */
			ReadWrite(const ReadWrite& other) noexcept = default;

			/** @brief Move construct. */
			ReadWrite(ReadWrite&& other) noexcept = default;

			/** @brief Virtual destructor. */
			virtual ~ReadWrite() noexcept = default;

			/** @brief Copy assign. */
			ReadWrite& operator=(const ReadWrite& other) = default;

			/** @brief Move assign. */
			ReadWrite& operator=(ReadWrite&& other) noexcept = default;
			/** @} */
	};
}
