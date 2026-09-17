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

#include <StormByte/buffer/hopper.hxx>
#include <StormByte/type_traits.hxx>

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>

namespace StormByte::Buffer {

	/**
	 * @class Sink
	 * @brief Set of Hopper buckets keyed by an integer.
	 *
	 * Sink manages a collection of Hopper queues keyed by an integer (representing
	 * channels, tracks, sessions, etc.). Sink does not interpret the integer key.
	 *
	 * Key characteristics:
	 * - Bind: Shares Hopper instances between producer and consumer sinks under specific keys.
	 *   Bind(key) when this Sink already has that hopper attaches the other Sink as a
	 *   co-writer: Eof on this Sink then only closes the hopper when the last writer closes.
	 * - Drain: Terminal producer flag. Push to an un-bound key discards the item without waiting for Bind.
	 * - Pop: Retrieves items across buckets using Round-Robin or custom Select chooser.
	 * - EoF: Closes the Sink and CloseWriter on hoppers this Sink writes.
	 *
	 * @tparam T Item type stored in the hoppers (must be MoveConstructible).
	 */
	template<Type::MoveConstructible T>
	class Sink {
		public:
			/**
			 * @brief Index chooser for Pop selection across multiple buckets.
			 *
			 * Given the count of active buckets, returns an index in range [0, count).
			 */
			using Select = std::function<std::size_t(std::size_t)>;

			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Constructs an empty Sink with zero buckets.
			 */
			Sink() noexcept;

			/**
			 * @brief Copy constructor is deleted (Sink is non-copyable).
			 */
			Sink(const Sink&) = delete;

			/**
			 * @brief Move constructor is deleted (Sink is non-movable).
			 */
			Sink(Sink&&) noexcept = delete;

			/**
			 * @brief Destructor. Wakes any threads blocked on wiring.
			 */
			~Sink() noexcept;

			/**
			 * @brief Copy assignment operator is deleted (Sink is non-copyable).
			 */
			Sink& operator=(const Sink&) = delete;

			/**
			 * @brief Move assignment operator is deleted (Sink is non-movable).
			 */
			Sink& operator=(Sink&&) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @name Producer
			 * @{
			 */

			/**
			 * @brief Pushes an item into the bucket specified by key.
			 * @param key Integer bucket identifier.
			 * @param item Item to enqueue. Empty smart pointers are discarded.
			 *
			 * Waits until the bucket exists, the Sink is closed, or Drain is set.
			 * If closed or draining and no bucket exists for key, item is discarded.
			 */
			void Push(int key, T item) noexcept;

			/**
			 * @brief Closes the Sink and releases writer holds on its hoppers.
			 *
			 * Hoppers this Sink created (or attached to as a co-writer) decrement
			 * their writer count. A hopper shared by several producers Eofs when
			 * the last writer closes. Idempotent.
			 *
			 * Wakes Push and Pop waiters.
			 */
			void Eof() noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Wiring & Connection
			 * @{
			 */

			/**
			 * @brief Shares all existing hoppers on this Sink with the consumer Sink.
			 * @param consumer Target consumer Sink.
			 * @deprecated Will be replaced by operator>> / operator<<.
			 */
			void Bind(Sink& consumer);

			/**
			 * @brief Creates or retrieves the hopper for key and shares it with consumer.
			 * @param key Bucket key identifier.
			 * @param consumer Target consumer Sink.
			 * @deprecated Will be replaced by operator>> / operator<<.
			 *
			 * This Sink has no hopper yet: creates it (this is the writer,
			 * @p consumer is the reader). This Sink already has the hopper:
			 * @p consumer is attached as a co-writer (extra producer).
			 */
			void Bind(int key, Sink& consumer);

			/**
			 * @brief Marks Sink as a terminal producer (un-bound Push calls drop instead of waiting).
			 */
			void Drain() noexcept;

			/**
			 * @brief Checks whether Drain was called.
			 * @return true if Drain was set.
			 */
			bool Draining() const noexcept;

			/**
			 * @brief Registers consumer condition variable to notify on all current and future hoppers.
			 * @param consumer Consumer condition variable reference. Not owned.
			 */
			void Notify(std::condition_variable& consumer) noexcept;

			/**
			 * @brief Clears @ref Notify on this Sink and on every hopper.
			 *
			 * Call after Eof when the consumer condition variable is about
			 * to die. Later Eof from a co-writer will not signal it.
			 */
			void Unnotify() noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Bucket Capacity & Query
			 * @{
			 */

			/**
			 * @brief Gets capacity ceiling of bucket key.
			 * @param key Bucket key identifier.
			 * @return Hopper capacity, or 0 if key bucket does not exist.
			 */
			std::size_t Capacity(int key) const noexcept;

			/**
			 * @brief Sets capacity ceiling of bucket key.
			 * @param key Bucket key identifier.
			 * @param capacity Maximum items allowed (0 = unbounded).
			 */
			void Capacity(int key, std::size_t capacity) noexcept;

			/**
			 * @brief Gets pending item count in bucket key.
			 * @param key Bucket key identifier.
			 * @return Item count, or 0 if bucket does not exist.
			 */
			std::size_t Size(int key) const noexcept;

			/**
			 * @brief Checks if bucket key is full.
			 * @param key Bucket key identifier.
			 * @return true if full, or false if key does not exist.
			 */
			bool Full(int key) const noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Consumer
			 * @{
			 */

			/**
			 * @brief Pops one item from the Sink using default round-robin order.
			 * @return Next item, or default T if empty/closed.
			 *
			 * Blocks while zero buckets exist and Sink is not closed.
			 */
			T Pop() noexcept;

			/**
			 * @brief Pops one item from the Sink using a custom selection function.
			 * @param select Bucket index selection callback.
			 * @return Next item, or default T if empty/closed.
			 *
			 * Blocks while zero buckets exist and Sink is not closed.
			 */
			T Pop(const Select& select) noexcept;

			/**
			 * @brief Checks if the Sink is finished.
			 * @return true if no items remain and no new items can arrive in the current hoppers.
			 *
			 * Contract details for EoF():
			 * - With zero hoppers: EoF() returns true only if this Sink is closed (via Eof() or destruction).
			 * - With hoppers: EoF() returns true when all hoppers are empty and Hopper::EoF() is true,
			 *   even if this Sink itself did not call Eof() (because Bind shares the Hopper and the producer
			 *   closed it from the other Sink).
			 * - Performing a Bind of a new key after EoF() returned true may cause EoF() to return false again
			 *   if new work becomes available.
			 * - EoF() does not mean "this object called Eof()", but "no items remain and none can enter current buckets".
			 */
			bool EoF() const noexcept;

			/**
			 * @brief Checks whether a Pop call can return immediately (item available or EoF).
			 * @return true if any hopper has an item or if Sink is EoF.
			 */
			bool Ready() const noexcept;

			/**
			 * @}
			 */

		private:
			/**
			 * @class Implementation
			 * @brief Private implementation class for Sink.
			 */
			class Implementation;

			std::unique_ptr<Implementation> m_impl;	///< Pointer to private implementation.
	};

}

#include <StormByte/buffer/sink.txx>
