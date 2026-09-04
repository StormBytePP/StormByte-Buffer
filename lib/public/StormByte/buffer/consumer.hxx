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

#include <StormByte/buffer/ring.hxx>

#include <memory>

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
	 * @class Consumer
	 * @brief Read-oriented handle over a shared @ref Ring.
	 *
	 * Multiple Consumer instances may share the same underlying Ring,
	 * allowing concurrent reads in a fully thread-safe manner.
	 * Consumers can only be created through a @ref Producer
	 * (see @ref Producer::Consumer()).
	 *
	 * All operations are thread-safe and delegate to the shared Ring.
	 * Blocking semantics match @ref Ring: Read / Extract / Peek block until
	 * data is available or the buffer is closed / in error.
	 *
	 * @par Lifecycle signalling
	 * Although Consumer is primarily a @ref ReadOnly view, it also exposes:
	 * - @ref Close() — closes the shared Ring for further writes (same effect
	 *   as @ref Producer::Close on the same buffer).
	 * - @ref IsWritable() / @ref HasError() — observe the shared Ring state
	 *   (useful in wait loops, e.g. until a pipeline finishes).
	 *
	 * @see Producer, Ring, ReadOnly
	 */
	class STORMBYTE_BUFFER_PUBLIC Consumer final: public ReadOnly {
		friend class Producer;

		public:
			/**
			 * @name Copy / move / assignment
			 * @{
			 */

			/**
			 * @brief Copy constructor.
			 * @param other Source Consumer; both share the same Ring.
			 */
			inline Consumer(const Consumer& other) noexcept : m_buffer(other.m_buffer) {}

			/**
			 * @brief Move constructor.
			 * @param other Source Consumer (left in a valid but unspecified state).
			 */
			inline Consumer(Consumer&& other) noexcept : m_buffer(std::move(other.m_buffer)) {}

			/**
			 * @brief Destructor.
			 */
			~Consumer() noexcept = default;

			/**
			 * @brief Copy assignment.
			 * @param other Source Consumer; both share the same Ring afterwards.
			 * @return Reference to this Consumer.
			 */
			inline Consumer& operator=(const Consumer& other) noexcept {
				if (this != &other) m_buffer = other.m_buffer;
				return *this;
			}

			/**
			 * @brief Move assignment.
			 * @param other Source Consumer.
			 * @return Reference to this Consumer.
			 */
			inline Consumer& operator=(Consumer&& other) noexcept {
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
			 * @param other Other Consumer.
			 * @return @c true if both refer to the same underlying Ring instance.
			 */
			inline bool operator==(const Consumer& other) const noexcept {
				return m_buffer.get() == other.m_buffer.get();
			}

			/**
			 * @brief Inequality comparison.
			 * @param other Other Consumer.
			 * @return @c true if the underlying Ring instances differ.
			 */
			inline bool operator!=(const Consumer& other) const noexcept {
				return !(*this == other);
			}

			/** @} */

			/**
			 * @name Queries
			 * @{
			 */

			/**
			 * @brief Number of bytes available for reading from the current position.
			 * @return Available byte count.
			 */
			inline std::size_t AvailableBytes() const noexcept override {
				return m_buffer->AvailableBytes();
			}

			/**
			 * @brief Access a snapshot of the underlying data (implementation-defined).
			 * @return Constant reference to the Ring’s data view.
			 * @warning Not intended for concurrent mutation; prefer Read / Extract.
			 */
			inline const DataType& Data() const noexcept override {
				return m_buffer->Data();
			}

			/**
			 * @brief Whether the shared Ring holds no stored bytes.
			 * @return @c true if empty.
			 * @note With a non-zero read position, @ref Empty() may still be @c false
			 *       even when @ref AvailableBytes() is zero.
			 */
			inline bool Empty() const noexcept override {
				return m_buffer->Empty();
			}

			/**
			 * @brief End-of-stream condition.
			 * @return @c true when the Ring is closed (or in error) and no bytes remain.
			 */
			inline bool EoF() const noexcept override {
				return m_buffer->EoF();
			}

			/**
			 * @brief Whether the shared Ring can still be read.
			 * @return @c false if the Ring is in a permanent error state.
			 */
			inline bool IsReadable() const noexcept override {
				return m_buffer->IsReadable();
			}

			/**
			 * @brief Whether the shared Ring still accepts writes.
			 * @return @c false if closed or in error.
			 * @details Observes producer-side lifecycle on the same Ring
			 *          (e.g. wait until a pipeline stage calls Close()).
			 */
			inline bool IsWritable() const noexcept {
				return m_buffer->IsWritable();
			}

			/**
			 * @brief Whether the shared Ring is in a permanent error state.
			 * @return @c true after @ref SetError() on any handle to the same Ring.
			 */
			inline bool HasError() const noexcept {
				return m_buffer->HasError();
			}

			/**
			 * @brief Total number of bytes stored in the shared Ring.
			 * @return Size in bytes.
			 */
			inline std::size_t Size() const noexcept override {
				return m_buffer->Size();
			}

			/** @} */

			/**
			 * @name Maintenance / lifecycle
			 * @{
			 */

			/**
			 * @brief Discard already-consumed data (from start up to the read position).
			 */
			inline void Clean() noexcept override {
				m_buffer->Clean();
			}

			/**
			 * @brief Clear all buffer contents.
			 * @details Does not clear closed / error flags on the shared Ring.
			 */
			inline void Clear() noexcept override {
				m_buffer->Clear();
			}

			/**
			 * @brief Close the shared Ring for further writes.
			 * @details Equivalent to @ref Producer::Close on the same underlying buffer.
			 *          Readers may still drain remaining data until EoF.
			 */
			inline void Close() noexcept {
				m_buffer->Close();
			}

			/**
			 * @brief Discard @p count bytes from the current read position.
			 * @param count Number of bytes to drop.
			 * @return @c true on success, @c false if fewer bytes were available.
			 */
			inline bool Drop(const std::size_t& count) noexcept override {
				return m_buffer->Drop(count);
			}

			/**
			 * @brief Move the logical read position for non-destructive reads.
			 * @param offset Offset value.
			 * @param mode   @ref Position::Absolute or @ref Position::Relative.
			 */
			inline void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override {
				m_buffer->Seek(offset, mode);
			}

			/** @} */

			/**
			 * @name Extract (destructive read)
			 * @{
			 */

			/**
			 * @brief Extract bytes into a @ref DataType (consumes data from the Ring).
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param out    Destination buffer (appended to / filled by the Ring).
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Extract(const std::size_t& count, DataType& out) noexcept override {
				return m_buffer->Extract(count, out);
			}

			/**
			 * @brief Extract bytes into a @ref WriteOnly sink (consumes data from the Ring).
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param out    Destination writer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Extract(const std::size_t& count, WriteOnly& out) noexcept override {
				return m_buffer->Extract(count, out);
			}

			/**
			 * @brief Extract all remaining bytes until EoF into a @ref DataType.
			 * @param out Destination buffer.
			 */
			inline void ExtractUntilEoF(DataType& out) noexcept override {
				m_buffer->ExtractUntilEoF(out);
			}

			/**
			 * @brief Extract all remaining bytes until EoF into a @ref WriteOnly.
			 * @param out Destination writer.
			 */
			inline void ExtractUntilEoF(WriteOnly& out) noexcept override {
				m_buffer->ExtractUntilEoF(out);
			}

			/** @} */

			/**
			 * @name Read (non-destructive)
			 * @{
			 */

			/**
			 * @brief Non-destructive read into a @ref DataType (advances logical position).
			 * @param count Number of bytes to read; 0 reads all available.
			 * @param out    Destination buffer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Read(const std::size_t& count, DataType& out) const noexcept override {
				return m_buffer->Read(count, out);
			}

			/**
			 * @brief Non-destructive read into a @ref WriteOnly (advances logical position).
			 * @param count Number of bytes to read; 0 reads all available.
			 * @param out    Destination writer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Read(const std::size_t& count, WriteOnly& out) const noexcept override {
				return m_buffer->Read(count, out);
			}

			/**
			 * @brief Read all remaining bytes until EoF into a @ref DataType.
			 * @param out Destination buffer.
			 */
			inline void ReadUntilEoF(DataType& out) const noexcept override {
				m_buffer->ReadUntilEoF(out);
			}

			/**
			 * @brief Read all remaining bytes until EoF into a @ref WriteOnly.
			 * @param out Destination writer.
			 */
			inline void ReadUntilEoF(WriteOnly& out) const noexcept override {
				m_buffer->ReadUntilEoF(out);
			}

			/** @} */

			/**
			 * @name Peek (non-destructive, does not advance position)
			 * @{
			 */

			/**
			 * @brief Peek bytes into a @ref DataType without advancing the read position.
			 * @param count Number of bytes to peek; 0 peeks all available.
			 * @param out    Destination buffer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Peek(const std::size_t& count, DataType& out) const noexcept override {
				return m_buffer->Peek(count, out);
			}

			/**
			 * @brief Peek bytes into a @ref WriteOnly without advancing the read position.
			 * @param count Number of bytes to peek; 0 peeks all available.
			 * @param out    Destination writer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Peek(const std::size_t& count, WriteOnly& out) const noexcept override {
				return m_buffer->Peek(count, out);
			}

			/** @} */

		private:
			std::shared_ptr<Ring> m_buffer; ///< Shared ring storage.

			/**
			 * @brief Private constructor used by @ref Producer.
			 * @param buffer Shared ring instance (must not be null).
			 */
			inline explicit Consumer(std::shared_ptr<Ring> buffer) noexcept
				: m_buffer(std::move(buffer)) {}
	};
}
