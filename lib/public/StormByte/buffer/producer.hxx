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

#include <StormByte/buffer/consumer.hxx>

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
		 * @class Producer
		 * @brief Write-only handle over a shared @ref Ring.
		 *
		 * Several Producer instances may share the same Ring. Writes are
		 * thread-safe. Occupancy is @ref Generic::Size of that Ring.
		 * @ref Consumer() returns a matching reader on the same store.
		 *
		 * @see Consumer, Ring, WriteOnly
		 */
		class STORMBYTE_BUFFER_PUBLIC Producer final : public WriteOnly {
			public:
				/**
				 * @name Lifecycle
				 * @{
				 */

				/**
				 * @brief Construct a Producer with a new shared @ref Ring.
				 */
				inline Producer() noexcept : m_buffer(std::make_shared<Ring>()) {}

				/**
				 * @brief Share an existing Ring.
				 * @param buffer Shared ring. Must not be null.
				 */
				inline explicit Producer(std::shared_ptr<Ring> buffer) noexcept
					: m_buffer(std::move(buffer)) {}

				/**
				 * @brief Share the Ring of a @ref Consumer.
				 * @param consumer Consumer whose Ring is shared.
				 */
				inline Producer(const Consumer& consumer) noexcept
					: m_buffer(consumer.m_buffer) {}

				/**
				 * @brief Copy constructor. Shares the same Ring.
				 * @param other Source Producer.
				 */
				inline Producer(const Producer& other) noexcept : m_buffer(other.m_buffer) {}

				/**
				 * @brief Move constructor.
				 * @param other Source Producer.
				 */
				inline Producer(Producer&& other) noexcept : m_buffer(std::move(other.m_buffer)) {}

				/**
				 * @brief Destructor.
				 */
				~Producer() noexcept override;

				/**
				 * @brief Copy assignment. Shares the same Ring afterwards.
				 * @param other Source Producer.
				 * @return *this.
				 */
				inline Producer& operator=(const Producer& other) noexcept {
					if (this != &other)
						m_buffer = other.m_buffer;
					return *this;
				}

				/**
				 * @brief Move assignment.
				 * @param other Source Producer.
				 * @return *this.
				 */
				inline Producer& operator=(Producer&& other) noexcept {
					if (this != &other)
						m_buffer = std::move(other.m_buffer);
					return *this;
				}

				/**
				 * @}
				 */

				/**
				 * @name Comparison
				 * @{
				 */

				/**
				 * @brief Equality. Same underlying Ring instance.
				 * @param other Other Producer.
				 * @return @c true if both refer to the same Ring.
				 */
				inline bool operator==(const Producer& other) const noexcept {
					return m_buffer.get() == other.m_buffer.get();
				}

				/**
				 * @brief Inequality.
				 * @param other Other Producer.
				 * @return @c true if the Rings differ.
				 */
				inline bool operator!=(const Producer& other) const noexcept {
					return !(*this == other);
				}

				/**
				 * @}
				 */

				/**
				 * @name Lifecycle / queries
				 * @{
				 */

				/**
				 * @brief Close the shared Ring for further writes.
				 * @details Later writes fail. Readers may still drain. Waiters are notified.
				 */
				void Close() noexcept override;

				/**
				 * @brief Permanent error on the shared Ring. Notifies waiters.
				 */
				void SetError() noexcept override;

				/**
				 * @brief Whether the shared Ring still accepts writes.
				 * @return @c false if closed or in error.
				 */
				bool IsWritable() const noexcept override;

				/**
				 * @brief Bytes stored in the shared Ring right now.
				 * @return Size in bytes. 0 if empty.
				 */
				std::size_t Size() const noexcept override;

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
				 */
				bool Write(const std::size_t& count, const DataType& data) noexcept override;

				/**
				 * @brief Append an entire @ref DataType (copy).
				 * @param data Source vector.
				 * @return @c false if closed or in error.
				 */
				inline bool Write(const DataType& data) noexcept {
					return Write(data.size(), data);
				}

				/**
				 * @brief Append bytes from a @ref DataType (move).
				 * @param count Bytes to write.
				 * @param data Source vector.
				 * @return @c false if closed or in error.
				 */
				bool Write(const std::size_t& count, DataType&& data) noexcept override;

				/**
				 * @brief Append an entire @ref DataType (move).
				 * @param data Source vector.
				 * @return @c false if closed or in error.
				 */
				inline bool Write(DataType&& data) noexcept {
					return Write(data.size(), std::move(data));
				}

				/**
				 * @brief Append bytes from a @ref ReadOnly (copy).
				 * @param count Bytes to write.
				 * @param data Source buffer.
				 * @return @c false if closed or in error.
				 */
				bool Write(const std::size_t& count, const ReadOnly& data) noexcept override;

				/**
				 * @brief Append bytes from a @ref ReadOnly (extract).
				 * @param count Bytes to write.
				 * @param data Source buffer.
				 * @return @c false if closed or in error.
				 */
				bool Write(const std::size_t& count, ReadOnly&& data) noexcept override;

				/**
				 * @brief Bring @ref WriteOnly convenience Write overloads into scope.
				 */
				using WriteOnly::Write;

				/**
				 * @}
				 */

				/**
				 * @brief Consumer that shares this Producer’s Ring.
				 * @return Consumer bound to the same store.
				 */
				inline class Consumer Consumer() {
					return StormByte::Buffer::Consumer{ m_buffer };
				}

			protected:
				std::shared_ptr<Ring> m_buffer;	///< Shared ring. Not null after construction.
		};
	}
}
