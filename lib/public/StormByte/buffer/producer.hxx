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
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class Producer
	 * @brief Write-only handle over a shared @ref Ring.
	 *
	 * Multiple Producer instances may share the same underlying Ring,
	 * allowing concurrent writes in a fully thread-safe manner.
	 *
	 * All operations delegate to the shared Ring. Use @ref Consumer() to
	 * obtain a matching read-oriented handle on the same buffer.
	 *
	 * @see Consumer, Ring, WriteOnly
	 */
	class STORMBYTE_BUFFER_PUBLIC Producer final : public WriteOnly {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Construct a Producer with a fresh shared @ref Ring.
			 */
			inline Producer() noexcept : m_buffer(std::make_shared<Ring>()) {}

			/**
			 * @brief Construct a Producer that shares an existing Ring.
			 * @param buffer Shared ring instance (must not be null).
			 */
			inline explicit Producer(std::shared_ptr<Ring> buffer) noexcept
				: m_buffer(std::move(buffer)) {}

			/**
			 * @brief Construct a Producer that shares the Ring of a @ref Consumer.
			 * @param consumer Consumer whose underlying Ring will be shared.
			 */
			inline Producer(const Consumer& consumer) noexcept
				: m_buffer(consumer.m_buffer) {}

			/**
			 * @brief Copy construct (shares the same Ring).
			 * @param other Source Producer.
			 */
			inline Producer(const Producer& other) noexcept : m_buffer(other.m_buffer) {}

			/**
			 * @brief Move construct.
			 * @param other Source Producer.
			 */
			inline Producer(Producer&& other) noexcept : m_buffer(std::move(other.m_buffer)) {}

			/** @brief Destructor. */
			~Producer() noexcept = default;

			/**
			 * @brief Copy assignment (shares the same Ring afterwards).
			 * @param other Source Producer.
			 * @return Reference to this Producer.
			 */
			inline Producer& operator=(const Producer& other) noexcept {
				if (this != &other) m_buffer = other.m_buffer;
				return *this;
			}

			/**
			 * @brief Move assignment.
			 * @param other Source Producer.
			 * @return Reference to this Producer.
			 */
			inline Producer& operator=(Producer&& other) noexcept {
				if (this != &other) m_buffer = std::move(other.m_buffer);
				return *this;
			}

			/** @} */

			/**
			 * @name Comparison
			 * @{
			 */

			/**
			 * @brief Equality comparison.
			 * @param other Other Producer.
			 * @return @c true if both refer to the same underlying Ring instance.
			 */
			inline bool operator==(const Producer& other) const noexcept {
				return m_buffer.get() == other.m_buffer.get();
			}

			/**
			 * @brief Inequality comparison.
			 * @param other Other Producer.
			 * @return @c true if the underlying Ring instances differ.
			 */
			inline bool operator!=(const Producer& other) const noexcept {
				return !(*this == other);
			}

			/** @} */

			/**
			 * @name Lifecycle / queries
			 * @{
			 */

			/**
			 * @brief Close the shared Ring for further writes.
			 * @details Subsequent writes fail; readers may still drain data.
			 *          Waiters on the Ring are notified.
			 */
			inline void Close() noexcept override {
				m_buffer->Close();
			}

			/**
			 * @brief Put the shared Ring into a permanent error state.
			 * @details Makes the buffer unreadable and unwritable; notifies waiters.
			 */
			inline void SetError() noexcept override {
				m_buffer->SetError();
			}

			/**
			 * @brief Whether the shared Ring still accepts writes.
			 * @return @c false if closed or in error.
			 */
			inline bool IsWritable() const noexcept override {
				return m_buffer->IsWritable();
			}

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
				return m_buffer->Write(count, data);
			}

			/**
			 * @brief Append an entire @ref DataType (copy).
			 * @param data Source vector.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const DataType& data) noexcept {
				return Write(data.size(), data);
			}

			/**
			 * @brief Append bytes from a @ref DataType (move path).
			 * @param count Number of bytes to write.
			 * @param data  Source vector.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const std::size_t& count, DataType&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}

			/**
			 * @brief Append an entire @ref DataType (move path).
			 * @param data Source vector.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(DataType&& data) noexcept {
				return Write(data.size(), std::move(data));
			}

			/**
			 * @brief Append bytes from a @ref ReadOnly (copy path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return m_buffer->Write(count, data);
			}

			/**
			 * @brief Append bytes from a @ref ReadOnly (move / extract path).
			 * @param count Number of bytes to write.
			 * @param data  Source buffer.
			 * @return @c true on success, @c false if closed / error.
			 */
			inline bool Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}

			/** @brief Bring @ref WriteOnly convenience Write overloads into scope. */
			using WriteOnly::Write;

			/** @} */

			/**
			 * @name Consumer factory
			 * @{
			 */

			/**
			 * @brief Create a @ref Consumer that shares this Producer’s Ring.
			 * @return Consumer bound to the same underlying buffer.
			 */
			inline class Consumer Consumer() {
				return StormByte::Buffer::Consumer{ m_buffer };
			}

			/** @} */

		protected:
			std::shared_ptr<Ring> m_buffer; ///< Shared ring storage.
	};
}
