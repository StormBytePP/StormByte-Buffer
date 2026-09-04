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

#include <StormByte/buffer/lockfree_ring.hxx>
#include <algorithm>
#include <cstring>
using namespace StormByte::Buffer;
// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
std::size_t LockFreeRing::RoundUpPow2(std::size_t v) noexcept {
	if (v < 16) return 16;
	--v;
	v |= v >> 1;  v |= v >> 2;  v |= v >> 4;
	v |= v >> 8;  v |= v >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
	v |= v >> 32;
#endif
	return ++v;
}
LockFreeRing::LockFreeRing(std::size_t initial_capacity) {
	m_capacity = RoundUpPow2(initial_capacity);
	m_mask     = m_capacity - 1;
	m_storage.resize(m_capacity);
}
LockFreeRing::LockFreeRing(LockFreeRing&& other) noexcept {
	// Only safe when no concurrent access exists
	m_storage   = std::move(other.m_storage);
	m_capacity  = other.m_capacity;
	m_mask      = other.m_mask;
	m_head.store(other.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
	m_tail.store(other.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
	m_logical.store(other.m_logical.load(std::memory_order_relaxed), std::memory_order_relaxed);
	m_closed.store(other.m_closed.load(std::memory_order_relaxed), std::memory_order_relaxed);
	m_error.store(other.m_error.load(std::memory_order_relaxed), std::memory_order_relaxed);
	other.m_capacity = 0;
	other.m_mask     = 0;
	other.m_head.store(0, std::memory_order_relaxed);
	other.m_tail.store(0, std::memory_order_relaxed);
	other.m_logical.store(0, std::memory_order_relaxed);
}
LockFreeRing& LockFreeRing::operator=(LockFreeRing&& other) noexcept {
	if (this != &other) {
		m_storage   = std::move(other.m_storage);
		m_capacity  = other.m_capacity;
		m_mask      = other.m_mask;
		m_head.store(other.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_tail.store(other.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_logical.store(other.m_logical.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_closed.store(other.m_closed.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_error.store(other.m_error.load(std::memory_order_relaxed), std::memory_order_relaxed);
		other.m_capacity = 0;
		other.m_mask     = 0;
		other.m_head.store(0, std::memory_order_relaxed);
		other.m_tail.store(0, std::memory_order_relaxed);
		other.m_logical.store(0, std::memory_order_relaxed);
	}
	return *this;
}
// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
std::size_t LockFreeRing::AvailableBytes() const noexcept {
	const std::size_t t = m_tail.load(std::memory_order_acquire);
	const std::size_t l = m_logical.load(std::memory_order_relaxed);
	return t - l;
}
bool LockFreeRing::Empty() const noexcept {
	return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
}
bool LockFreeRing::EoF() const noexcept {
	if (m_error.load(std::memory_order_acquire)) return true;
	if (!m_closed.load(std::memory_order_acquire)) return false;
	return AvailableBytes() == 0;
}
bool LockFreeRing::HasError() const noexcept {
	return m_error.load(std::memory_order_acquire);
}
bool LockFreeRing::IsReadable() const noexcept {
	return !m_error.load(std::memory_order_acquire);
}
bool LockFreeRing::IsWritable() const noexcept {
	return !m_closed.load(std::memory_order_acquire) &&
		!m_error.load(std::memory_order_acquire);
}
std::size_t LockFreeRing::Size() const noexcept {
	const std::size_t h = m_head.load(std::memory_order_acquire);
	const std::size_t t = m_tail.load(std::memory_order_acquire);
	return t - h;
}
const DataType& LockFreeRing::Data() const noexcept {
	std::lock_guard lock(m_wait_mtx);
	const std::size_t h = m_head.load(std::memory_order_relaxed);
	const std::size_t t = m_tail.load(std::memory_order_relaxed);
	const std::size_t sz = t - h;
	const std::size_t mask = m_mask;
	m_data_cache.clear();
	m_data_cache.reserve(sz);
	for (std::size_t i = 0; i < sz; ++i)
		m_data_cache.push_back(m_storage[(h + i) & mask]);
	return m_data_cache;
}
// ---------------------------------------------------------------------------
// Mutators
// ---------------------------------------------------------------------------
void LockFreeRing::Clean() noexcept {
	const std::size_t l = m_logical.load(std::memory_order_relaxed);
	m_head.store(l, std::memory_order_release);
	m_cv.notify_all();
}
void LockFreeRing::Clear() noexcept {
	std::lock_guard lock(m_wait_mtx);
	m_head.store(0, std::memory_order_relaxed);
	m_tail.store(0, std::memory_order_relaxed);
	m_logical.store(0, std::memory_order_relaxed);
	m_cv.notify_all();
}
void LockFreeRing::Close() noexcept {
	m_closed.store(true, std::memory_order_release);
	m_cv.notify_all();
}
void LockFreeRing::SetError() noexcept {
	m_error.store(true, std::memory_order_release);
	m_cv.notify_all();
}
bool LockFreeRing::Drop(const std::size_t& count) noexcept {
	if (count == 0) return true;
	const std::size_t avail = AvailableBytes();
	if (count > avail) return false;
	m_logical.fetch_add(count, std::memory_order_relaxed);
	m_head.store(m_logical.load(std::memory_order_relaxed), std::memory_order_release);
	m_cv.notify_all();
	return true;
}
void LockFreeRing::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	std::size_t base = (mode == Position::Absolute)
		? m_head.load(std::memory_order_relaxed)
		: m_logical.load(std::memory_order_relaxed);
	std::ptrdiff_t target = static_cast<std::ptrdiff_t>(base) + offset;
	if (target < static_cast<std::ptrdiff_t>(m_head.load(std::memory_order_relaxed)))
		target = static_cast<std::ptrdiff_t>(m_head.load(std::memory_order_relaxed));
	const std::size_t t = m_tail.load(std::memory_order_acquire);
	if (static_cast<std::size_t>(target) > t)
		target = static_cast<std::ptrdiff_t>(t);
	m_logical.store(static_cast<std::size_t>(target), std::memory_order_relaxed);
}
// ---------------------------------------------------------------------------
// Grow — MUST be called with m_wait_mtx held
// ---------------------------------------------------------------------------
void LockFreeRing::Grow() noexcept {
	// Caller holds m_wait_mtx so no concurrent ReadInternal/WriteInternal touches storage.
	const std::size_t old_cap = m_capacity;
	const std::size_t new_cap = old_cap * 2;
	std::vector<std::byte> new_storage(new_cap);
	const std::size_t h = m_head.load(std::memory_order_relaxed);
	const std::size_t t = m_tail.load(std::memory_order_relaxed);
	const std::size_t sz = t - h;
	const std::size_t old_mask = m_mask;
	for (std::size_t i = 0; i < sz; ++i)
		new_storage[i] = m_storage[(h + i) & old_mask];
	const std::size_t logical = m_logical.load(std::memory_order_relaxed);
	const std::size_t logical_off = logical - h;
	m_storage  = std::move(new_storage);
	m_capacity = new_cap;
	m_mask     = new_cap - 1;
	m_head.store(0, std::memory_order_relaxed);
	m_logical.store(logical_off, std::memory_order_relaxed);
	m_tail.store(sz, std::memory_order_relaxed);
}
// ---------------------------------------------------------------------------
// Wait helper
// ---------------------------------------------------------------------------
bool LockFreeRing::WaitFor(std::size_t n) const {
	if (n == 0) return true;
	std::unique_lock lock(m_wait_mtx);
	m_cv.wait(lock, [&] {
		if (m_error.load(std::memory_order_acquire) ||
			m_closed.load(std::memory_order_acquire))
			return true;
		return AvailableBytes() >= n;
	});
	if (m_error.load(std::memory_order_acquire))
		return false;
	return AvailableBytes() >= n;
}
// ---------------------------------------------------------------------------
// Read path
// ---------------------------------------------------------------------------
bool LockFreeRing::ReadInternal(std::size_t count, DataType& out, Operation op) noexcept {
	if (m_error.load(std::memory_order_acquire))
		return false;
	std::size_t avail = AvailableBytes();
	// count == 0 → "all currently available". If empty and still open, wait for data or close.
	if (count == 0) {
		if (avail == 0) {
			if (m_closed.load(std::memory_order_acquire))
				return false;
			if (!WaitFor(1))
				return false;
			avail = AvailableBytes();
			if (avail == 0)
				return false; // closed/error with nothing left
		}
		count = avail;
	}
	if (avail == 0 && m_closed.load(std::memory_order_acquire))
		return false;
	const std::size_t want = count;
	if (want > avail) {
		if (!WaitFor(want))
			return false;
		avail = AvailableBytes();
		if (want > avail)
			return false;
	}
	// Serialize with Grow()/WriteInternal storage updates
	std::lock_guard lock(m_wait_mtx);
	avail = AvailableBytes();
	if (m_error.load(std::memory_order_acquire))
		return false;
	if (want > avail)
		return false;
	const std::size_t logical = m_logical.load(std::memory_order_relaxed);
	const std::size_t mask    = m_mask;
	out.reserve(out.size() + want);
	for (std::size_t i = 0; i < want; ++i)
		out.push_back(m_storage[(logical + i) & mask]);
	if (op == Operation::Read || op == Operation::Extract) {
		const std::size_t new_logical = logical + want;
		m_logical.store(new_logical, std::memory_order_relaxed);
		// Reclaim slot space so the producer does not Grow forever.
		m_head.store(new_logical, std::memory_order_release);
		m_cv.notify_all();
	}
	// Peek: no advance
	return true;
}
bool LockFreeRing::Peek(const std::size_t& count, DataType& out) const noexcept {
	return const_cast<LockFreeRing*>(this)->ReadInternal(count, out, Operation::Peek);
}
bool LockFreeRing::Peek(const std::size_t& count, WriteOnly& out) const noexcept {
	DataType tmp;
	if (!Peek(count, tmp)) return false;
	return out.Write(std::move(tmp));
}
bool LockFreeRing::Read(const std::size_t& count, DataType& out) const noexcept {
	return const_cast<LockFreeRing*>(this)->ReadInternal(count, out, Operation::Read);
}
bool LockFreeRing::Read(const std::size_t& count, WriteOnly& out) const noexcept {
	DataType tmp;
	if (!Read(count, tmp)) return false;
	return out.Write(std::move(tmp));
}
bool LockFreeRing::Extract(const std::size_t& count, DataType& out) noexcept {
	return ReadInternal(count, out, Operation::Extract);
}
bool LockFreeRing::Extract(const std::size_t& count, WriteOnly& out) noexcept {
	DataType tmp;
	if (!Extract(count, tmp)) return false;
	return out.Write(std::move(tmp));
}
void LockFreeRing::ReadUntilEoF(DataType& out) const noexcept {
	auto* self = const_cast<LockFreeRing*>(this);
	while (true) {
		if (self->m_error.load(std::memory_order_acquire))
			return;
		{
			std::unique_lock lock(self->m_wait_mtx);
			self->m_cv.wait(lock, [&] {
				if (self->m_error.load(std::memory_order_acquire) ||
					self->m_closed.load(std::memory_order_acquire))
					return true;
				return self->AvailableBytes() > 0;
			});
		}
		if (self->m_error.load(std::memory_order_acquire))
			return;
		if (self->AvailableBytes() == 0 &&
			self->m_closed.load(std::memory_order_acquire))
			return; // true EoF
		DataType chunk;
		if (!self->Read(0, chunk) || chunk.empty()) {
			if (self->EoF())
				return;
			continue;
		}
		out.insert(out.end(), chunk.begin(), chunk.end());
	}
}
void LockFreeRing::ReadUntilEoF(WriteOnly& out) const noexcept {
	DataType tmp;
	ReadUntilEoF(tmp);
	if (!tmp.empty()) (void)out.Write(std::move(tmp));
}
void LockFreeRing::ExtractUntilEoF(DataType& out) noexcept {
	while (true) {
		if (m_error.load(std::memory_order_acquire))
			return;
		{
			std::unique_lock lock(m_wait_mtx);
			m_cv.wait(lock, [&] {
				if (m_error.load(std::memory_order_acquire) ||
					m_closed.load(std::memory_order_acquire))
					return true;
				return AvailableBytes() > 0;
			});
		}
		if (m_error.load(std::memory_order_acquire))
			return;
		if (AvailableBytes() == 0 &&
			m_closed.load(std::memory_order_acquire))
			return; // true EoF
		DataType chunk;
		if (!Extract(0, chunk) || chunk.empty()) {
			if (EoF())
				return;
			continue;
		}
		out.insert(out.end(),
			std::make_move_iterator(chunk.begin()),
			std::make_move_iterator(chunk.end()));
	}
}
void LockFreeRing::ExtractUntilEoF(WriteOnly& out) noexcept {
	DataType tmp;
	ExtractUntilEoF(tmp);
	if (!tmp.empty()) (void)out.Write(std::move(tmp));
}
// ---------------------------------------------------------------------------
// Write path (producer)
// ---------------------------------------------------------------------------
bool LockFreeRing::WriteInternal(std::size_t count, const std::byte* src) noexcept {
	if (m_closed.load(std::memory_order_acquire) ||
		m_error.load(std::memory_order_acquire))
		return false;
	if (count == 0) return true;
	std::lock_guard lock(m_wait_mtx);
	if (m_closed.load(std::memory_order_acquire) ||
		m_error.load(std::memory_order_acquire))
		return false;
	while (true) {
		const std::size_t h = m_head.load(std::memory_order_relaxed);
		const std::size_t t = m_tail.load(std::memory_order_relaxed);
		const std::size_t used = t - h;
		if (used + count <= m_capacity)
			break;
		Grow(); // holds m_wait_mtx already
	}
	const std::size_t t    = m_tail.load(std::memory_order_relaxed);
	const std::size_t mask = m_mask;
	for (std::size_t i = 0; i < count; ++i)
		m_storage[(t + i) & mask] = src[i];
	m_tail.store(t + count, std::memory_order_release);
	m_cv.notify_all();
	return true;
}
bool LockFreeRing::Write(const std::size_t& count, const DataType& data) noexcept {
	const std::size_t n = (count == 0) ? data.size() : std::min(count, data.size());
	return WriteInternal(n, data.data());
}
bool LockFreeRing::Write(const std::size_t& count, DataType&& data) noexcept {
	const std::size_t n = (count == 0) ? data.size() : std::min(count, data.size());
	bool ok = WriteInternal(n, data.data());
	if (ok && n == data.size())
		data.clear();
	return ok;
}
bool LockFreeRing::Write(const std::size_t& count, const ReadOnly& data) noexcept {
	DataType tmp;
	if (!data.Read(count, tmp)) return false;
	return Write(0, std::move(tmp));
}
bool LockFreeRing::Write(const std::size_t& count, ReadOnly&& data) noexcept {
	DataType tmp;
	if (!data.Extract(count, tmp)) return false;
	return Write(0, std::move(tmp));
}
