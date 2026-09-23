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

#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/size.hxx>
#include <StormByte/type_traits.hxx>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

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
		 * @class Generic
		 * @brief Abstract root of the byte-buffer interfaces.
		 *
		 * No data members. Concrete types own storage. @ref Size is declared
		 * here so every buffer (read, write or both) reports occupancy.
		 * Protected @c DataConvert helpers turn ranges and strings into
		 * @ref DataType.
		 *
		 * @see ReadOnly, WriteOnly, ReadWrite
		 */
		class STORMBYTE_BUFFER_PUBLIC Generic {
			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Default constructor.
				 */
				Generic() noexcept = default;

				/**
				 * @brief Copy constructor.
				 * @param other Instance to copy.
				 */
				Generic(const Generic& other) noexcept = default;

				/**
				 * @brief Move constructor.
				 * @param other Instance to take from.
				 */
				Generic(Generic&& other) noexcept = default;

				/**
				 * @brief Pure virtual destructor. Keeps the class abstract.
				 */
				virtual ~Generic() noexcept = 0;

				/**
				 * @brief Copy assignment.
				 * @param other Instance to copy.
				 * @return *this.
				 */
				Generic& operator=(const Generic& other) = default;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from.
				 * @return *this.
				 */
				Generic& operator=(Generic&& other) noexcept = default;

				/**
				 * @}
				 */

				/**
				 * @name Queries
				 * @{
				 */

				/**
				 * @brief Total number of bytes stored.
				 * @return Size in bytes. 0 if the store is empty.
				 */
				virtual StormByte::Size Size() const noexcept = 0;

				/**
				 * @}
				 */

			protected:
				/**
				 * @name DataConvert
				 * @{
				 */

				/**
				 * @brief Convert an lvalue input range to @ref DataType.
				 * @tparam Src Range whose value_type converts to @c std::byte.
				 * @param src Source range.
				 * @return Converted byte vector.
				 */
				template<Type::ByteInputRange Src>
				static DataType DataConvert(const Src& src) noexcept {
					DataType out;
					if constexpr (requires { std::ranges::size(src); }) {
						auto s = std::ranges::size(src);
						if (s > 0)
							out.reserve(static_cast<typename DataType::size_type>(s));
					}
					std::transform(std::ranges::begin(src), std::ranges::end(src), std::back_inserter(out),
						[](auto&& e) noexcept { return static_cast<std::byte>(e); });
					return out;
				}

				/**
				 * @brief Convert an rvalue input range to @ref DataType.
				 * @tparam Src Range type. Moved when already @ref DataType.
				 * @param src Source range.
				 * @return Converted or moved byte vector.
				 */
				template<Type::ByteInputRange Src>
				static DataType DataConvert(Src&& src) noexcept {
					if constexpr (Type::SameAs<Src, DataType>) {
						return std::move(src);
					} else {
						DataType out;
						if constexpr (requires { std::ranges::size(src); }) {
							auto s = std::ranges::size(src);
							if (s > 0)
								out.reserve(static_cast<typename DataType::size_type>(s));
						}
						std::transform(std::ranges::begin(src), std::ranges::end(src), std::back_inserter(out),
							[](auto&& e) noexcept { return static_cast<std::byte>(e); });
						return out;
					}
				}

				/**
				 * @brief Convert a string view to @ref DataType. No trailing NUL.
				 * @param sv Source characters.
				 * @return Byte vector.
				 */
				static DataType DataConvert(std::string_view sv) noexcept {
					DataType out;
					if (!sv.empty())
						out.reserve(static_cast<typename DataType::size_type>(sv.size()));
					std::transform(sv.begin(), sv.end(), std::back_inserter(out),
						[](char c) noexcept { return static_cast<std::byte>(c); });
					return out;
				}

				/**
				 * @brief Convert a C string to @ref DataType.
				 * @param s Source. Null yields an empty vector.
				 * @return Byte vector.
				 */
				static DataType DataConvert(const char* s) noexcept {
					if (!s)
						return DataType{};
					return DataConvert(std::string_view(s));
				}

				/**
				 * @}
				 */
		};

		class WriteOnly;

		/**
		 * @class ReadOnly
		 * @brief Buffer that can be read and not written.
		 *
		 * Read, extract, peek and seek. Occupancy is @ref Generic::Size.
		 *
		 * @see WriteOnly, ReadWrite, Generic
		 */
		class STORMBYTE_BUFFER_PUBLIC ReadOnly: virtual public Generic {
			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Default constructor.
				 */
				inline ReadOnly() noexcept: Generic() {}

				/**
				 * @brief Copy constructor.
				 * @param other Instance to copy.
				 */
				ReadOnly(const ReadOnly& other) noexcept = default;

				/**
				 * @brief Move constructor.
				 * @param other Instance to take from.
				 */
				ReadOnly(ReadOnly&& other) noexcept = default;

				/**
				 * @brief Destructor.
				 */
				virtual ~ReadOnly() noexcept;

				/**
				 * @brief Copy assignment.
				 * @param other Instance to copy.
				 * @return *this.
				 */
				ReadOnly& operator=(const ReadOnly& other) = default;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from.
				 * @return *this.
				 */
				ReadOnly& operator=(ReadOnly&& other) noexcept = default;

				/**
				 * @}
				 */

				/**
				 * @name Queries
				 * @{
				 */

				/**
				 * @brief Bytes available from the current read position.
				 * @return Unread byte count.
				 */
				virtual StormByte::Size AvailableBytes() const noexcept = 0;

				/**
				 * @brief View of internal storage. Implementation-defined.
				 * @return Constant reference to a @ref DataType.
				 */
				virtual const DataType& Data() const noexcept = 0;

				/**
				 * @brief Whether the store holds no bytes.
				 * @return @c true if empty.
				 * @see Size()
				 */
				virtual bool Empty() const noexcept = 0;

				/**
				 * @brief End of stream.
				 * @return @c true when closed or failed and nothing remains to read.
				 */
				virtual bool EoF() const noexcept = 0;

				/**
				 * @brief Whether the buffer can still be read.
				 * @return @c false in a permanent error state.
				 */
				virtual bool IsReadable() const noexcept = 0;

				/**
				 * @}
				 */

				/**
				 * @name Maintenance
				 * @{
				 */

				/**
				 * @brief Drop bytes from the start up to the current read position.
				 * @see Size(), Empty()
				 */
				virtual void Clean() noexcept = 0;

				/**
				 * @brief Clear stored bytes and reset logical positions.
				 * @details Closed and error flags are implementation-defined.
				 * @see Size(), Empty()
				 */
				virtual void Clear() noexcept = 0;

				/**
				 * @brief Discard @p count unread bytes.
				 * @param count Bytes to drop.
				 * @return @c false if fewer bytes were available.
				 * @see Read()
				 */
				virtual bool Drop(const StormByte::Size& count) noexcept = 0;

				/**
				 * @brief Move the logical read position.
				 * @param offset Offset.
				 * @param mode @ref Position::Absolute or @ref Position::Relative.
				 * @details Clamped to @c [0, Size()]. Absolute + negative offset is a no-op.
				 * @see Read(), Position
				 */
				virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept = 0;

				/**
				 * @}
				 */

				/**
				 * @name Extract
				 * @{
				 */

				/**
				 * @brief Extract bytes into a @ref DataType.
				 * @param count Bytes to extract. 0 = all available.
				 * @param outBuffer Destination.
				 * @return @c false on failure.
				 */
				inline virtual bool Extract(const StormByte::Size& count, DataType& outBuffer) noexcept = 0;

				/**
				 * @brief Extract all available bytes into a @ref DataType.
				 * @param outBuffer Destination.
				 * @return @c false on failure.
				 */
				inline bool Extract(DataType& outBuffer) noexcept {
					return Extract(StormByte::Size{0}, outBuffer);
				}

				/**
				 * @brief Extract bytes into a @ref WriteOnly.
				 * @param count Bytes to extract. 0 = all available.
				 * @param outBuffer Destination writer.
				 * @return @c false on failure.
				 */
				inline virtual bool Extract(const StormByte::Size& count, WriteOnly& outBuffer) noexcept = 0;

				/**
				 * @brief Extract all available bytes into a @ref WriteOnly.
				 * @param outBuffer Destination writer.
				 * @return @c false on failure.
				 */
				inline bool Extract(WriteOnly& outBuffer) noexcept {
					return Extract(StormByte::Size{0}, outBuffer);
				}

				/**
				 * @brief Extract until EoF into a @ref DataType.
				 * @param outBuffer Destination.
				 * @warning Can grow without bound.
				 */
				virtual void ExtractUntilEoF(DataType& outBuffer) noexcept = 0;

				/**
				 * @brief Extract until EoF into a @ref WriteOnly.
				 * @param outBuffer Destination writer.
				 * @warning Can grow without bound.
				 */
				virtual void ExtractUntilEoF(WriteOnly& outBuffer) noexcept = 0;

				/**
				 * @}
				 */

				/**
				 * @name Peek
				 * @{
				 */

				/**
				 * @brief Peek into a @ref DataType. Does not advance the cursor.
				 * @param count Bytes to peek. 0 = all available (fails if none).
				 *        Greater than 0: exactly that many bytes, or fail.
				 * @param outBuffer Destination.
				 * @return @c false if insufficient data or error.
				 * @see Read(), Seek()
				 */
				virtual bool Peek(const StormByte::Size& count, DataType& outBuffer) const noexcept = 0;

				/**
				 * @brief Peek into a @ref WriteOnly. Does not advance the cursor.
				 * @param count Same semantics as the @ref DataType overload.
				 * @param outBuffer Destination writer.
				 * @return @c false if insufficient data or error.
				 * @see Read(), Seek()
				 */
				virtual bool Peek(const StormByte::Size& count, WriteOnly& outBuffer) const noexcept = 0;

				/**
				 * @}
				 */

				/**
				 * @name Read
				 * @{
				 */

				/**
				 * @brief Read into a @ref DataType. Advances the cursor.
				 * @param count Bytes to read. 0 = all available.
				 * @param outBuffer Destination.
				 * @return @c false on failure.
				 */
				virtual bool Read(const StormByte::Size& count, DataType& outBuffer) const noexcept = 0;

				/**
				 * @brief Read all available bytes into a @ref DataType.
				 * @param outBuffer Destination.
				 * @return @c false on failure.
				 */
				inline bool Read(DataType& outBuffer) const noexcept {
					return Read(StormByte::Size{0}, outBuffer);
				}

				/**
				 * @brief Read into a @ref WriteOnly. Advances the cursor.
				 * @param count Bytes to read. 0 = all available.
				 * @param outBuffer Destination writer.
				 * @return @c false on failure.
				 */
				virtual bool Read(const StormByte::Size& count, WriteOnly& outBuffer) const noexcept = 0;

				/**
				 * @brief Read all available bytes into a @ref WriteOnly.
				 * @param outBuffer Destination writer.
				 * @return @c false on failure.
				 */
				inline bool Read(WriteOnly& outBuffer) const noexcept {
					return Read(StormByte::Size{0}, outBuffer);
				}

				/**
				 * @brief Read until EoF into a @ref DataType.
				 * @param outBuffer Destination.
				 * @warning Can grow without bound.
				 */
				virtual void ReadUntilEoF(DataType& outBuffer) const noexcept = 0;

				/**
				 * @brief Read until EoF into a @ref WriteOnly.
				 * @param outBuffer Destination writer.
				 * @warning Can grow without bound.
				 */
				virtual void ReadUntilEoF(WriteOnly& outBuffer) const noexcept = 0;

				/**
				 * @}
				 */
		};

		/**
		 * @class WriteOnly
		 * @brief Buffer that can be written and not read.
		 *
		 * Close, error and Write. Occupancy is @ref Generic::Size.
		 *
		 * @see ReadOnly, ReadWrite, Generic
		 */
		class STORMBYTE_BUFFER_PUBLIC WriteOnly: virtual public Generic {
			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Default constructor.
				 */
				inline WriteOnly() noexcept: Generic() {}

				/**
				 * @brief Copy constructor.
				 * @param other Instance to copy.
				 */
				WriteOnly(const WriteOnly& other) = default;

				/**
				 * @brief Move constructor.
				 * @param other Instance to take from.
				 */
				WriteOnly(WriteOnly&& other) noexcept = default;

				/**
				 * @brief Destructor.
				 */
				virtual ~WriteOnly() noexcept;

				/**
				 * @brief Copy assignment.
				 * @param other Instance to copy.
				 * @return *this.
				 */
				WriteOnly& operator=(const WriteOnly& other) = default;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from.
				 * @return *this.
				 */
				WriteOnly& operator=(WriteOnly&& other) noexcept = default;

				/**
				 * @}
				 */

				/**
				 * @name Lifecycle of the store
				 * @{
				 */

				/**
				 * @brief Whether the buffer accepts writes.
				 * @return @c false if closed or in error.
				 */
				virtual bool IsWritable() const noexcept = 0;

				/**
				 * @brief Stop further writes. Readers may still drain the store.
				 */
				virtual void Close() noexcept = 0;

				/**
				 * @brief Permanent error. Wake waiters.
				 */
				virtual void SetError() noexcept = 0;

				/**
				 * @}
				 */

				/**
				 * @name Write
				 * @{
				 */

				/**
				 * @brief Append bytes from a @ref DataType (copy).
				 * @param count Bytes to write.
				 * @param data Source vector.
				 * @return @c false if closed or in error.
				 * @see IsWritable()
				 */
				virtual bool Write(const StormByte::Size& count, const DataType& data) noexcept = 0;

				/**
				 * @brief Append bytes from a @ref DataType (move).
				 * @param count Bytes to write.
				 * @param data Source vector.
				 * @return @c false if closed or in error.
				 * @see IsWritable()
				 */
				virtual bool Write(const StormByte::Size& count, DataType&& data) noexcept = 0;

				/**
				 * @brief Append bytes from a @ref ReadOnly (copy).
				 * @param count Bytes to write.
				 * @param data Source buffer.
				 * @return @c false if closed or in error.
				 * @see IsWritable()
				 */
				virtual bool Write(const StormByte::Size& count, const ReadOnly& data) noexcept = 0;

				/**
				 * @brief Append bytes from a @ref ReadOnly (extract).
				 * @param count Bytes to write.
				 * @param data Source buffer.
				 * @return @c false if closed or in error.
				 * @see IsWritable()
				 */
				virtual bool Write(const StormByte::Size& count, ReadOnly&& data) noexcept = 0;

				/**
				 * @}
				 */

				/**
				 * @name Write (ReadOnly helpers)
				 * @{
				 */

				/**
				 * @brief Append all available bytes from a @ref ReadOnly (copy).
				 * @param data Source buffer.
				 * @return @c false if closed or in error.
				 */
				inline bool Write(const ReadOnly& data) noexcept {
					return Write(data.AvailableBytes(), data);
				}

				/**
				 * @brief Append all available bytes from a @ref ReadOnly (extract).
				 * @param data Source buffer.
				 * @return @c false if closed or in error.
				 */
				inline bool Write(ReadOnly&& data) noexcept {
					return Write(data.AvailableBytes(), std::move(data));
				}

				/**
				 * @}
				 */

				/**
				 * @name Write (string helpers)
				 * @{
				 */

				/**
				 * @brief Write a string view. No trailing NUL.
				 * @param sv Source.
				 * @return @c false if closed or in error.
				 */
				bool Write(std::string_view sv) noexcept {
					DataType tmp;
					if (!sv.empty())
						tmp.reserve(static_cast<typename DataType::size_type>(sv.size()));
					std::transform(sv.begin(), sv.end(), std::back_inserter(tmp),
						[](char e) noexcept { return static_cast<std::byte>(e); });
					return Write(StormByte::Size{tmp.size()}, std::move(tmp));
				}

				/**
				 * @brief Write a C string.
				 * @param s Source. Null is an empty write.
				 * @return @c false if closed or in error.
				 */
				bool Write(const char* s) noexcept {
					if (!s)
						return Write(DataType{});
					return Write(std::string_view(s));
				}

				/**
				 * @brief Write up to @p count characters of a string view.
				 * @param count Maximum characters. 0 = entire view.
				 * @param sv Source.
				 * @return @c false if closed or in error.
				 */
				bool Write(const StormByte::Size& count, std::string_view sv) noexcept {
					const StormByte::Size to_write = (count == StormByte::Size{0})
						? StormByte::Size{sv.size()}
						: std::min(count, StormByte::Size{sv.size()});
					DataType tmp;
					if (to_write > StormByte::Size{0})
						tmp.reserve(static_cast<typename DataType::size_type>(to_write.Value()));
					std::transform(sv.begin(), sv.begin() + static_cast<std::ptrdiff_t>(to_write.Value()),
						std::back_inserter(tmp),
						[](char e) noexcept { return static_cast<std::byte>(e); });
					return Write(to_write, std::move(tmp));
				}

				/**
				 * @brief Write up to @p count characters of a C string.
				 * @param count Maximum characters. 0 = entire string.
				 * @param s Source. May be null.
				 * @return @c false if closed or in error.
				 */
				bool Write(const StormByte::Size& count, const char* s) noexcept {
					if (!s)
						return Write(count, DataType{});
					return Write(count, std::string_view(s));
				}

				/**
				 * @brief Write a string literal without the trailing NUL.
				 * @tparam N Array extent (includes the NUL of a literal).
				 * @param s Literal.
				 * @return @c false if closed or in error.
				 */
				template<std::size_t N>
				bool Write(const char (&s)[N]) noexcept {
					if (N == 0)
						return Write(DataType{});
					return Write(std::string_view(s, (N > 0) ? (N - 1) : 0));
				}

				/**
				 * @}
				 */

				/**
				 * @name Write (range / iterator helpers)
				 * @{
				 */

				/**
				 * @brief Write all elements from an input range.
				 * @tparam R Range whose value_type converts to @c std::byte.
				 * @param r Source range.
				 * @return @c false if closed or in error.
				 */
				template<Type::ByteInputRange R>
				bool Write(const R& r) noexcept {
					DataType tmp;
					if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
						auto dist = std::ranges::distance(r);
						if (dist > 0)
							tmp.reserve(static_cast<typename DataType::size_type>(dist));
					}
					std::transform(std::ranges::begin(r), std::ranges::end(r), std::back_inserter(tmp),
						[](auto&& e) noexcept { return static_cast<std::byte>(e); });
					return Write(StormByte::Size{tmp.size()}, std::move(tmp));
				}

				/**
				 * @brief Write up to @p count elements from an input range.
				 * @tparam Rw Range type.
				 * @param count Maximum elements. 0 = entire range.
				 * @param r Source range.
				 * @return @c false if closed or in error.
				 */
				template<Type::ByteInputRange Rw>
				bool Write(const StormByte::Size& count, const Rw& r) noexcept {
					if (count == StormByte::Size{0})
						return Write(r);
					DataType tmp;
					if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
						auto dist = std::ranges::distance(r);
						if (dist > 0)
							tmp.reserve(static_cast<typename DataType::size_type>(
								std::min(static_cast<std::uint64_t>(dist), count.Value())));
					}
					auto it = std::ranges::begin(r);
					auto end = std::ranges::end(r);
					StormByte::Size written{0};
					for (; it != end && written < count; ++it, written = written + StormByte::Size{1})
						tmp.push_back(static_cast<std::byte>(*it));
					return Write(written, std::move(tmp));
				}

				/**
				 * @brief Write up to @p count elements from an rvalue range.
				 * @tparam Rrw Range type. Moved when @ref DataType.
				 * @param count Maximum elements. 0 = entire range.
				 * @param r Source range.
				 * @return @c false if closed or in error.
				 */
				template<Type::ByteInputRange Rrw>
				bool Write(const StormByte::Size& count, Rrw&& r) noexcept {
					if (count == StormByte::Size{0})
						return Write(std::forward<Rrw>(r));
					if constexpr (Type::SameAs<Rrw, DataType>) {
						DataType tmp = std::move(r);
						if (StormByte::Size{tmp.size()} > count)
							tmp.resize(static_cast<typename DataType::size_type>(count.Value()));
						return Write(StormByte::Size{tmp.size()}, std::move(tmp));
					} else {
						DataType tmp;
						if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
							auto dist = std::ranges::distance(r);
							if (dist > 0)
								tmp.reserve(static_cast<typename DataType::size_type>(
									std::min(static_cast<std::uint64_t>(dist), count.Value())));
						}
						auto it = std::ranges::begin(r);
						auto end = std::ranges::end(r);
						StormByte::Size written{0};
						for (; it != end && written < count; ++it, written = written + StormByte::Size{1})
							tmp.push_back(static_cast<std::byte>(*it));
						return Write(written, std::move(tmp));
					}
				}

				/**
				 * @brief Write all elements from an rvalue range.
				 * @tparam Rr Range type. Moved when @ref DataType.
				 * @param r Source.
				 * @return @c false if closed or in error.
				 */
				template<Type::ByteInputRange Rr>
				bool Write(Rr&& r) noexcept {
					if constexpr (Type::SameAs<Rr, DataType>) {
						return Write(StormByte::Size{r.size()}, std::move(r));
					} else {
						DataType tmp;
						if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
							auto dist = std::ranges::distance(r);
							if (dist > 0)
								tmp.reserve(static_cast<typename DataType::size_type>(dist));
						}
						std::transform(std::ranges::begin(r), std::ranges::end(r), std::back_inserter(tmp),
							[](auto&& e) noexcept { return static_cast<std::byte>(e); });
						return Write(StormByte::Size{tmp.size()}, std::move(tmp));
					}
				}

				/**
				 * @brief Write all elements from an iterator pair.
				 * @tparam I Input iterator.
				 * @tparam S Sentinel for @p I.
				 * @param first Begin.
				 * @param last End.
				 * @return @c false if closed or in error.
				 */
				template<Type::ByteInputIterator I, typename S>
					requires Type::SentinelFor<S, I>
				bool Write(I first, S last) noexcept {
					DataType tmp;
					std::transform(first, last, std::back_inserter(tmp),
						[](auto&& e) noexcept { return static_cast<std::byte>(e); });
					return Write(StormByte::Size{tmp.size()}, std::move(tmp));
				}

				/**
				 * @brief Write up to @p count elements from an iterator pair.
				 * @tparam I2 Input iterator.
				 * @tparam S2 Sentinel for @p I2.
				 * @param count Maximum elements. 0 = all.
				 * @param first Begin.
				 * @param last End.
				 * @return @c false if closed or in error.
				 */
				template<Type::ByteInputIterator I2, typename S2>
					requires Type::SentinelFor<S2, I2>
				bool Write(const StormByte::Size& count, I2 first, S2 last) noexcept {
					if (count == StormByte::Size{0})
						return Write(first, last);
					DataType tmp;
					StormByte::Size written{0};
					for (; first != last && written < count; ++first, written = written + StormByte::Size{1})
						tmp.push_back(static_cast<std::byte>(*first));
					return Write(written, std::move(tmp));
				}

				/**
				 * @}
				 */
		};

		/**
		 * @class ReadWrite
		 * @brief Read and write interface. @ref FIFO and @ref Ring implement this.
		 *
		 * @see ReadOnly, WriteOnly, Generic
		 */
		class STORMBYTE_BUFFER_PUBLIC ReadWrite: public ReadOnly, public WriteOnly {
			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Default constructor.
				 */
				inline ReadWrite() noexcept: Generic() {}

				/**
				 * @brief Copy constructor.
				 * @param other Instance to copy.
				 */
				ReadWrite(const ReadWrite& other) noexcept = default;

				/**
				 * @brief Move constructor.
				 * @param other Instance to take from.
				 */
				ReadWrite(ReadWrite&& other) noexcept = default;

				/**
				 * @brief Destructor.
				 */
				virtual ~ReadWrite() noexcept;

				/**
				 * @brief Copy assignment.
				 * @param other Instance to copy.
				 * @return *this.
				 */
				ReadWrite& operator=(const ReadWrite& other) = default;

				/**
				 * @brief Move assignment.
				 * @param other Instance to take from.
				 * @return *this.
				 */
				ReadWrite& operator=(ReadWrite&& other) noexcept = default;

				/**
				 * @}
				 */
		};

		/**
		 * @name Explicit instantiations
		 * @brief Closed set emitted by libStormByte-Buffer. Other ranges still instantiate in the caller.
		 * @{
		 */
		extern template DataType STORMBYTE_BUFFER_PUBLIC Generic::DataConvert<DataType>(const DataType&) noexcept;
		extern template DataType STORMBYTE_BUFFER_PUBLIC Generic::DataConvert<DataType>(DataType&&) noexcept;
		extern template DataType STORMBYTE_BUFFER_PUBLIC Generic::DataConvert<std::span<const std::byte>>(const std::span<const std::byte>&) noexcept;
		extern template DataType STORMBYTE_BUFFER_PUBLIC Generic::DataConvert<std::span<std::byte>>(const std::span<std::byte>&) noexcept;
		extern template DataType STORMBYTE_BUFFER_PUBLIC Generic::DataConvert<std::span<std::byte>>(std::span<std::byte>&&) noexcept;

		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType>(const DataType&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType>(DataType&&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType>(const StormByte::Size&, const DataType&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType>(const StormByte::Size&, DataType&&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<std::span<const std::byte>>(const std::span<const std::byte>&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<std::span<const std::byte>>(const StormByte::Size&, const std::span<const std::byte>&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<std::span<std::byte>>(const std::span<std::byte>&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<std::span<std::byte>>(const StormByte::Size&, const std::span<std::byte>&) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType::const_iterator, DataType::const_iterator>(DataType::const_iterator, DataType::const_iterator) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType::iterator, DataType::iterator>(DataType::iterator, DataType::iterator) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType::const_iterator, DataType::const_iterator>(const StormByte::Size&, DataType::const_iterator, DataType::const_iterator) noexcept;
		extern template bool STORMBYTE_BUFFER_PUBLIC WriteOnly::Write<DataType::iterator, DataType::iterator>(const StormByte::Size&, DataType::iterator, DataType::iterator) noexcept;
		/**
		 * @}
		 */
	}
}
