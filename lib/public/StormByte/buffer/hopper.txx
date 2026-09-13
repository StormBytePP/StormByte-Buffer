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

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>

namespace StormByte::Buffer {

	/**
	 * @class Hopper<T>::Implementation
	 * @brief Internal implementation of Hopper queue details.
	 *
	 * Handles mutex-protected queue operations, atomic capacity settings,
	 * condition variable notifications, and EoF flags.
	 */
	template<Type::MoveConstructible T>
	class Hopper<T>::Implementation {
		public:
			/**
			 * @brief Constructs an unbounded Implementation instance.
			 */
			Implementation() noexcept
			: m_eof(false), m_wake(nullptr), m_cap(0) {}

			/**
			 * @brief Constructs a bounded Implementation instance.
			 * @param capacity Maximum items allowed.
			 */
			explicit Implementation(std::size_t capacity) noexcept
			: m_eof(false), m_wake(nullptr), m_cap(capacity) {}

			/**
			 * @brief Destructor. Marks EoF and wakes waiting producers.
			 */
			~Implementation() noexcept {
				m_eof.store(true, std::memory_order_release);
				m_space.notify_all();
			}

			/**
			 * @brief Gets capacity ceiling.
			 * @return Capacity value.
			 */
			std::size_t Capacity() const noexcept {
				return m_cap.load(std::memory_order_acquire);
			}

			/**
			 * @brief Sets capacity ceiling.
			 * @param capacity New capacity value.
			 */
			void Capacity(std::size_t capacity) noexcept {
				m_cap.store(capacity, std::memory_order_release);
				m_space.notify_all();
			}

			/**
			 * @brief Gets item count in queue.
			 * @return Count of items.
			 */
			std::size_t Size() const noexcept {
				std::lock_guard<std::mutex> lock(m_mutex);
				return m_items.size();
			}

			/**
			 * @brief Checks if bounded bucket is full.
			 * @return true if capacity > 0 and size >= capacity.
			 */
			bool Full() const noexcept {
				const std::size_t cap = m_cap.load(std::memory_order_acquire);
				if (cap == 0)
					return false;
				return Size() >= cap;
			}

			/**
			 * @brief Enqueues an item, waiting if full.
			 * @param item Item to enqueue.
			 */
			void Push(T item) noexcept {
				if constexpr (Type::NullablePointer<T>) {
					if (!item)
						return;
				}
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_space.wait(lock, [this]() {
						const std::size_t cap = m_cap.load(std::memory_order_acquire);
						return cap == 0
							|| m_items.size() < cap
							|| m_eof.load(std::memory_order_acquire);
					});
					if (m_eof.load(std::memory_order_acquire))
						return;
					m_items.push(std::move(item));
				}
				SignalConsumer();
			}

			/**
			 * @brief Signals end of production.
			 */
			void Eof() noexcept {
				m_eof.store(true, std::memory_order_release);
				SignalConsumer();
				m_space.notify_all();
			}

			/**
			 * @brief Pops next item from queue without waiting.
			 * @return Next item, or default T if empty.
			 */
			T Pop() noexcept {
				T item{};
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_items.empty())
						return T{};
					item = std::move(m_items.front());
					m_items.pop();
				}
				m_space.notify_one();
				return item;
			}

			/**
			 * @brief Checks if Eof was signaled.
			 * @return true if Eof set.
			 */
			bool EoF() const noexcept {
				return m_eof.load(std::memory_order_acquire);
			}

			/**
			 * @brief Checks if queue is empty.
			 * @return true if empty.
			 */
			bool Empty() const noexcept {
				std::lock_guard<std::mutex> lock(m_mutex);
				return m_items.empty();
			}

			/**
			 * @brief Registers consumer condition variable.
			 * @param wake Condition variable reference.
			 */
			void Notify(std::condition_variable& wake) noexcept {
				m_wake.store(&wake, std::memory_order_release);
			}

		private:
			/**
			 * @brief Notifies registered consumer condition variable if set.
			 */
			void SignalConsumer() noexcept {
				std::condition_variable* wake = m_wake.load(std::memory_order_acquire);
				if (wake == nullptr)
					return;
				wake->notify_one();
			}

			mutable std::mutex m_mutex;						///< Guards queue access.
			std::condition_variable m_space;				///< Producer wait condition when full.
			std::queue<T> m_items;							///< Queue of stored items.
			std::atomic<bool> m_eof;						///< End of production flag.
			std::atomic<std::condition_variable*> m_wake;	///< Consumer condition variable.
			std::atomic<std::size_t> m_cap;					///< Capacity ceiling (0 = unbounded).
	};

	template<Type::MoveConstructible T>
	Hopper<T>::Hopper() noexcept
	: m_impl(std::make_unique<Implementation>()) {}

	template<Type::MoveConstructible T>
	Hopper<T>::Hopper(std::size_t capacity) noexcept
	: m_impl(std::make_unique<Implementation>(capacity)) {}

	template<Type::MoveConstructible T>
	Hopper<T>::~Hopper() noexcept = default;

	template<Type::MoveConstructible T>
	std::size_t Hopper<T>::Capacity() const noexcept {
		return m_impl->Capacity();
	}

	template<Type::MoveConstructible T>
	void Hopper<T>::Capacity(std::size_t capacity) noexcept {
		m_impl->Capacity(capacity);
	}

	template<Type::MoveConstructible T>
	std::size_t Hopper<T>::Size() const noexcept {
		return m_impl->Size();
	}

	template<Type::MoveConstructible T>
	bool Hopper<T>::Full() const noexcept {
		return m_impl->Full();
	}

	template<Type::MoveConstructible T>
	void Hopper<T>::Push(T item) noexcept {
		m_impl->Push(std::move(item));
	}

	template<Type::MoveConstructible T>
	void Hopper<T>::Eof() noexcept {
		m_impl->Eof();
	}

	template<Type::MoveConstructible T>
	T Hopper<T>::Pop() noexcept {
		return m_impl->Pop();
	}

	template<Type::MoveConstructible T>
	bool Hopper<T>::EoF() const noexcept {
		return m_impl->EoF();
	}

	template<Type::MoveConstructible T>
	bool Hopper<T>::Empty() const noexcept {
		return m_impl->Empty();
	}

	template<Type::MoveConstructible T>
	void Hopper<T>::Notify(std::condition_variable& wake) noexcept {
		m_impl->Notify(wake);
	}

}
