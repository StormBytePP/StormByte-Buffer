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

#include <StormByte/buffer/io/backend/buffered_writer.hxx>
#include <StormByte/buffer/lockfree_ring.hxx>

#include <algorithm>

using namespace StormByte::Buffer::IO::Backend;
using Result = StormByte::Buffer::IO::Result;
using State = StormByte::Buffer::IO::State;

BufferedWriter::BufferedWriter(IO::BufferedWriter& owner, const std::size_t write_chunk,
		const std::size_t back_pressure, const std::chrono::milliseconds max_wait):
	m_owner(&owner),
	m_write_chunk(write_chunk),
	m_back_pressure(back_pressure),
	m_max_wait(max_wait),
	m_state(State::Unavailable) {
	if (BufferedMode())
		m_ring = std::make_unique<LockFreeRing>(PendingCap());
	StartWorker();
}

BufferedWriter::~BufferedWriter() {
	Shutdown();
}

void BufferedWriter::Rebind(IO::BufferedWriter& owner) noexcept {
	m_owner = &owner;
}

BufferedWriter::operator bool() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_state == State::Idle;
}

State BufferedWriter::State() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_state;
}

void BufferedWriter::SetState(const enum State state) noexcept {
	std::lock_guard lock(m_mutex);
	m_state = state;
}

bool BufferedWriter::BufferedMode() const noexcept {
	return m_write_chunk > 0 && m_back_pressure > 0;
}

std::size_t BufferedWriter::PendingCap() const noexcept {
	if (!BufferedMode())
		return 0;
	return m_back_pressure * m_write_chunk;
}

bool BufferedWriter::WouldAccept(const std::size_t bytes) const noexcept {
	if (!BufferedMode())
		return true;
	if (!m_ring)
		return false;
	return m_ring->AvailableBytes() + bytes <= PendingCap();
}

bool BufferedWriter::WillWrite(const std::size_t n) const noexcept {
	return WouldAccept(n);
}

bool BufferedWriter::Open() {
	if (!m_owner)
		return false;

	bool already = false;
	{
		std::lock_guard lock(m_mutex);
		already = m_open;
	}

	const Result opened = m_owner->OriginOpen();

	std::lock_guard lock(m_mutex);
	if (already)
		return false;

	if (opened.status != Status::Ok) {
		m_failed = true;
		m_open = false;
		return false;
	}

	m_open = true;
	m_failed = false;
	m_tell = 0;
	if (m_ring)
		m_ring->Clear();
	return m_state == State::Idle;
}

bool BufferedWriter::Close() {
	const Result flushed = Flush();

	bool was_open = false;
	{
		std::lock_guard lock(m_mutex);
		was_open = m_open;
	}

	if (was_open && m_owner)
		static_cast<void>(m_owner->OriginClose());

	std::lock_guard lock(m_mutex);
	m_open = false;
	if (flushed.status == Status::Error) {
		m_failed = true;
		m_state = State::Fault;
		return false;
	}
	if (flushed.status == Status::Failed && was_open) {
		m_failed = true;
		m_state = State::Fault;
		return false;
	}
	m_failed = false;
	m_state = State::Unavailable;
	if (m_ring)
		m_ring->Clear();
	return true;
}

void BufferedWriter::Shutdown() {
	m_stop.store(true);
	m_cv.notify_all();
	StopWorker();
	std::lock_guard lock(m_mutex);
	m_open = false;
	m_state = State::Unavailable;
}

bool BufferedWriter::Rewind() {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open)
			return false;
	}
	if (!Close())
		return false;
	return Open();
}

bool BufferedWriter::IsOpen() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_open;
}

Result BufferedWriter::Flush() {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || !m_owner)
			return { Status::Failed, 0 };
		if (m_ring && m_ring->AvailableBytes() > 0) {
			m_flush.store(true);
			m_drain_run = true;
			m_cv.notify_all();
		}
	}

	if (m_ring) {
		std::unique_lock lock(m_mutex);
		m_cv.wait(lock, [this] {
			return m_stop.load() || m_failed
				|| !m_ring || m_ring->AvailableBytes() == 0;
		});
		m_flush.store(false);
		m_drain_run = false;
		if (m_failed)
			return { Status::Error, 0 };
	}

	const Result visible = m_owner->OriginFlush();
	if (visible.status != Status::Ok) {
		std::lock_guard lock(m_mutex);
		m_failed = true;
		if (m_state == State::Idle)
			m_state = State::Fault;
		return visible;
	}
	return { Status::Ok, 0 };
}

Result BufferedWriter::Truncate() {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || !m_owner)
			return { Status::Failed, 0 };
		if (m_ring)
			m_ring->Clear();
	}

	const Result truncated = m_owner->OriginTruncate();
	std::lock_guard lock(m_mutex);
	if (truncated.status != Status::Ok) {
		m_failed = true;
		m_state = State::Fault;
		return { Status::Failed, 0 };
	}
	m_tell = 0;
	return { Status::Ok, 0 };
}

Result BufferedWriter::Write(const FIFO& src) {
	const std::size_t need = src.AvailableBytes();
	if (need == 0)
		return { Status::Ok, 0 };
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle)
			return { Status::Failed, 0 };
		if (!WouldAccept(need))
			return { Status::TryAgain, 0 };
	}
	DataType chunk;
	if (!src.Read(need, chunk))
		return { Status::Failed, 0 };
	return WriteSpan(std::span<const std::byte>(chunk.data(), chunk.size()));
}

Result BufferedWriter::Write(FIFO& src) {
	return Write(static_cast<const FIFO&>(src));
}

Result BufferedWriter::Write(const std::span<const std::byte> src) {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle)
			return { Status::Failed, 0 };
		if (src.empty())
			return { Status::Ok, 0 };
		if (!WouldAccept(src.size()))
			return { Status::TryAgain, 0 };
	}
	return WriteSpan(src);
}

Result BufferedWriter::WriteSpan(const std::span<const std::byte> src) {
	if (src.empty())
		return { Status::Ok, 0 };

	if (!BufferedMode()) {
		const Result pushed = PushAll(src);
		if (pushed.status != Status::Ok) {
			std::lock_guard lock(m_mutex);
			m_failed = true;
			if (m_state == State::Idle)
				m_state = State::Fault;
			return pushed;
		}
		const Result visible = m_owner->OriginFlush();
		if (visible.status != Status::Ok) {
			std::lock_guard lock(m_mutex);
			m_failed = true;
			if (m_state == State::Idle)
				m_state = State::Fault;
			return visible;
		}
		std::lock_guard lock(m_mutex);
		m_tell += src.size();
		return { Status::Ok, src.size() };
	}

	if (!m_ring)
		return { Status::Failed, 0 };

	if (!m_ring->Write(src))
		return { Status::Failed, 0 };

	{
		std::lock_guard lock(m_mutex);
		m_tell += src.size();
	}
	RequestDrain();
	return { Status::Ok, src.size() };
}

std::size_t BufferedWriter::Tell() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_tell;
}

std::size_t BufferedWriter::Dirty() const noexcept {
	if (!m_ring)
		return 0;
	return m_ring->AvailableBytes();
}

std::size_t BufferedWriter::WriteChunk() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_write_chunk;
}

void BufferedWriter::WriteChunk(const std::size_t bytes) {
	const bool disable = bytes == 0 || m_back_pressure == 0;
	if (disable && Dirty() > 0)
		static_cast<void>(Flush());

	{
		std::lock_guard lock(m_mutex);
		m_write_chunk = bytes;
		if (!BufferedMode()) {
			m_ring.reset();
			return;
		}
		if (!m_ring)
			m_ring = std::make_unique<LockFreeRing>(PendingCap());
	}
	RequestDrain();
}

std::size_t BufferedWriter::BackPressure() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_back_pressure;
}

void BufferedWriter::BackPressure(const std::size_t chunks) {
	const std::size_t unit = WriteChunk();
	const std::size_t cap = (chunks == 0 || unit == 0) ? 0 : chunks * unit;
	if (cap == 0 || Dirty() > cap)
		static_cast<void>(Flush());

	{
		std::lock_guard lock(m_mutex);
		m_back_pressure = chunks;
		if (!BufferedMode()) {
			m_ring.reset();
			return;
		}
		if (!m_ring)
			m_ring = std::make_unique<LockFreeRing>(PendingCap());
	}
	RequestDrain();
}

std::chrono::milliseconds BufferedWriter::MaxWait() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_max_wait;
}

void BufferedWriter::MaxWait(const std::chrono::milliseconds wait) {
	std::lock_guard lock(m_mutex);
	m_max_wait = wait;
}

void BufferedWriter::StartWorker() {
	if (m_worker.joinable())
		return;
	m_stop.store(false);
	m_worker = std::thread(&BufferedWriter::Worker, this);
}

void BufferedWriter::StopWorker() {
	m_stop.store(true);
	m_cv.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

void BufferedWriter::RequestDrain() const {
	std::lock_guard lock(m_mutex);
	if (!BufferedMode() || !m_ring)
		return;
	if (m_ring->AvailableBytes() < m_write_chunk && !m_flush.load())
		return;
	m_drain_run = true;
	m_cv.notify_all();
}

void BufferedWriter::Worker() {
	for (;;) {
		std::unique_lock lock(m_mutex);
		m_cv.wait(lock, [this] {
			return m_stop.load() || m_drain_run || m_flush.load();
		});
		if (m_stop.load()) {
			m_cv.notify_all();
			return;
		}

		const std::size_t chunk = m_write_chunk;
		lock.unlock();

		while (!m_stop.load() && m_ring && m_owner) {
			const bool flush = m_flush.load();
			const std::size_t dirty = m_ring->AvailableBytes();
			if (dirty == 0)
				break;
			if (!flush && dirty < chunk)
				break;

			auto front = m_ring->FrontSpan();
			if (front.empty())
				break;
			const std::size_t want = flush ? front.size() : std::min(front.size(), chunk);
			front = front.first(want);

			const Result pushed = PushAll(front);
			if (pushed.status != Status::Ok) {
				std::lock_guard inner(m_mutex);
				m_failed = true;
				if (m_state == State::Idle)
					m_state = State::Fault;
				m_cv.notify_all();
				break;
			}
			static_cast<void>(m_ring->Consume(front.size()));
		}

		lock.lock();
		m_drain_run = false;
		m_cv.notify_all();
		if (!m_stop.load() && m_flush.load() && m_ring
				&& m_ring->AvailableBytes() > 0) {
			m_drain_run = true;
			continue;
		}
	}
}

Result BufferedWriter::PushAll(const std::span<const std::byte> data) const {
	if (!m_owner)
		return { Status::Failed, 0 };
	if (data.empty())
		return { Status::Ok, 0 };

	const auto deadline = m_max_wait.count() == 0
		? std::chrono::steady_clock::time_point::max()
		: std::chrono::steady_clock::now() + m_max_wait;

	std::size_t off = 0;
	while (off < data.size()) {
		if (m_stop.load())
			return { Status::Failed, 0 };
		if (m_max_wait.count() != 0 && std::chrono::steady_clock::now() >= deadline)
			return { Status::Error, off };

		const auto rest = data.subspan(off);
		const Result pushed = m_owner->OriginPush(rest);
		if (pushed.status == Status::Failed || pushed.status == Status::Error)
			return { pushed.status, off };
		if (pushed.count == 0)
			return { Status::Error, off };
		if (pushed.count > rest.size())
			return { Status::Error, off };
		off += pushed.count;
	}
	return { Status::Ok, data.size() };
}
