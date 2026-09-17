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

#include <StormByte/type_traits.hxx>

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <utility>

namespace StormByte::Buffer {
	template<Type::MoveConstructible T> class Sink;

	/**
	 * @class Hopper
	 * @brief Single-producer single-consumer (SPSC) typed item queue.
	 *
	 * Hopper is a typed bucket queue for passing items between a single producer
	 * and a single consumer. Unlike byte-oriented FIFOs, Hopper operates on discrete
	 * typed elements.
	 *
	 * Key characteristics:
	 * - Capacity ceiling: Optional capacity ceiling (0 = unbounded). Push waits when full.
	 * - EoF handling: Marking Eof signals end of production; queued items can still be drained via Pop.
	 * - Consumer notification: Points to a consumer condition variable via Notify to signal when
	 *   items or EoF are available.
	 * - Item flow: @c hopper << item and @c item >> hopper enqueue; @c hopper >> item dequeues.
	 * - Non-copyable, non-movable: Shared via std::shared_ptr.
	 *
	 * @tparam T Item type stored in the queue (must be MoveConstructible).
	 */
	template<Type::MoveConstructible T>
	class Hopper {
		public:
			/**
			 * @name Lifecycle
			 * @{
			 */

			/**
			 * @brief Constructs an empty unbounded Hopper.
			 */
			Hopper() noexcept;

			/**
			 * @brief Constructs an empty Hopper with a capacity ceiling.
			 * @param capacity Maximum number of items allowed (0 = unbounded).
			 */
			explicit Hopper(std::size_t capacity) noexcept;

			/**
			 * @brief Copy constructor is deleted (Hopper is non-copyable).
			 */
			Hopper(const Hopper&) = delete;

			/**
			 * @brief Move constructor is deleted (Hopper is non-movable).
			 */
			Hopper(Hopper&&) noexcept = delete;

			/**
			 * @brief Destructor. Wakes any Push blocked on a full bucket.
			 */
			~Hopper() noexcept;

			/**
			 * @brief Copy assignment operator is deleted (Hopper is non-copyable).
			 */
			Hopper& operator=(const Hopper&) = delete;

			/**
			 * @brief Move assignment operator is deleted (Hopper is non-movable).
			 */
			Hopper& operator=(Hopper&&) noexcept = delete;

			/**
			 * @}
			 */

			/**
			 * @name Capacity
			 * @{
			 */

			/**
			 * @brief Gets the current capacity ceiling.
			 * @return Maximum items, or 0 if unbounded.
			 */
			std::size_t Capacity() const noexcept;

			/**
			 * @brief Sets a new capacity ceiling.
			 * @param capacity Maximum items allowed (0 = unbounded).
			 *
			 * Lowering capacity does not drop queued items; subsequent Push calls wait
			 * until Size falls below the new ceiling.
			 */
			void Capacity(std::size_t capacity) noexcept;

			/**
			 * @brief Gets the number of items currently waiting in the bucket.
			 * @return Item count.
			 */
			std::size_t Size() const noexcept;

			/**
			 * @brief Checks whether a bounded bucket cannot accept another Push without waiting.
			 * @return true if Capacity > 0 and Size >= Capacity.
			 */
			bool Full() const noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Producer
			 * @{
			 */

			/**
			 * @brief Enqueues one item and signals the consumer if Notify was configured.
			 * @param item Item to push. Empty smart pointers are discarded.
			 *
			 * If Capacity > 0 and the bucket is full, waits until space becomes available,
			 * Eof is called, or the Hopper is destroyed. After Eof, Push does not enqueue.
			 */
			void Push(T item) noexcept;

			/**
			 * @brief Write: @p item flows into this Hopper.
			 * @param item Unit. Moved. Empty smart pointers are discarded.
			 * @return *this.
			 */
			Hopper& operator<<(T item) noexcept;

			/**
			 * @brief Marks end of production and wakes waiters.
			 *
			 * Does not discard already queued items. After Eof, Push does not enqueue.
			 */
			void Eof() noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Consumer
			 * @{
			 */

			/**
			 * @brief Pops one item from the bucket, or returns default-constructed T if dry.
			 * @return Next item, or default T if empty.
			 *
			 * Does not block. If empty and EoF is false, the consumer waits on its condition variable.
			 * Wakes one producer blocked in Push if space becomes available.
			 */
			T Pop() noexcept;

			/**
			 * @brief Pop one unit from this Hopper into @p item.
			 * @param item Destination. Becomes default T if the bucket is dry.
			 * @return *this.
			 */
			Hopper& operator>>(T& item) noexcept;

			/**
			 * @brief Checks whether Eof was called by a producer.
			 * @return true if Eof was called. Note that queued items may still remain.
			 */
			bool EoF() const noexcept;

			/**
			 * @brief Checks whether the queue has no pending items.
			 * @return true if empty.
			 */
			bool Empty() const noexcept;

			/**
			 * @}
			 */

			/**
			 * @name Notification
			 * @{
			 */

			/**
			 * @brief Registers the consumer condition variable to notify on Push or Eof.
			 * @param wake Consumer condition variable reference. Not owned.
			 *
			 * The referent must outlive this Hopper, or the owner must call
			 * @ref Unnotify before destroying @p wake. Sink wiring shares the Hopper:
			 * a producer Eof after the consumer died is the usual case.
			 */
			void Notify(std::condition_variable& wake) noexcept;

			/**
			 * @brief Drops the pointer set by @ref Notify.
			 *
			 * Safe to call more than once or when nothing was registered.
			 * After this, Push and Eof do not signal a consumer CV; producers
			 * blocked on a full bucket still wake on @c m_space.
			 */
			void Unnotify() noexcept;

			/**
			 * @}
			 */

			/**
			 * @brief Write: lvalue @p item flows into @p hopper.
			 * @param item Unit. Moved. Empty smart pointers are discarded.
			 * @param hopper Destination hopper.
			 * @return @p hopper.
			 *
			 * Namespace declaration required by GCC next to the friend
			 * (GCC will not define a friend-only operator out of line).
			 */
			friend Hopper& operator>>(T& item, Hopper& hopper) noexcept {
				hopper << std::move(item);
				return hopper;
			}

			/**
			 * @brief Write: rvalue @p item flows into @p hopper.
			 * @param item Unit. Empty smart pointers are discarded.
			 * @param hopper Destination hopper.
			 * @return @p hopper.
			 */
			friend Hopper& operator>>(T&& item, Hopper& hopper) noexcept {
				hopper << std::move(item);
				return hopper;
			}

		private:
			friend class Sink<T>;

			void AddWriter() noexcept;
			void CloseWriter() noexcept;

			/**
			 * @class Implementation
			 * @brief Private implementation details of Hopper.
			 */
			class Implementation;

			std::unique_ptr<Implementation> m_impl;	///< Pointer to private implementation.
	};
}

#include <StormByte/buffer/hopper.txx>
