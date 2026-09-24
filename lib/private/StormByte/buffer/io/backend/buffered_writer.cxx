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

#include <StormByte/buffer/io/backend/buffered_writer.hxx>
#include <StormByte/buffer/lockfree_ring.hxx>

#include <algorithm>

using namespace StormByte::Buffer::IO::Backend;
using Result = StormByte::Buffer::IO::Result;
using State = StormByte::Buffer::IO::State;

BufferedWriter::BufferedWriter(IO::BufferedWriter& owner, const StormByte::Size write_chunk,
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

void BufferedWriter::SetTell(const StormByte::Size offset) noexcept {
	std::lock_guard lock(m_mutex);
	m_tell = offset;
}

bool BufferedWriter::BufferedMode() const noexcept {
	return m_write_chunk > StormByte::Size{0} && m_back_pressure > 0;
}

StormByte::Size BufferedWriter::PendingCap() const noexcept {
	if (!BufferedMode())
		return StormByte::Size{0};
	return m_back_pressure * m_write_chunk;
}

bool BufferedWriter::WouldAccept(const StormByte::Size bytes) const noexcept {
	if (!BufferedMode())
		return true;
	if (!m_ring)
		return false;
	return m_ring->AvailableBytes() + bytes <= PendingCap();
}

bool BufferedWriter::WillWrite(const StormByte::Size n) const noexcept {
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
	m_tell = StormByte::Size{0};
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
	m_stop.store(true, std::memory_order_release);
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
	std::unique_lock lock(m_mutex);
	if (!m_open || m_failed || !m_owner)
		return { Status::Failed, 0 };

	const bool need_drain = m_ring && m_ring->AvailableBytes() > StormByte::Size{0};
	if (need_drain) {
		m_flush.store(true, std::memory_order_release);
		m_drain_run = true;
		m_cv.notify_all();
		m_cv.wait(lock, [this] {
			return m_stop.load(std::memory_order_acquire)
				|| m_failed
				|| !m_ring
				|| m_ring->AvailableBytes() == StormByte::Size{0};
		});
		m_flush.store(false, std::memory_order_release);
		m_drain_run = false;
		if (m_failed)
			return { Status::Error, 0 };
		if (m_stop.load(std::memory_order_acquire))
			return { Status::Failed, 0 };
	}
	lock.unlock();

	const Result visible = m_owner->OriginFlush();
	if (visible.status != Status::Ok) {
		std::lock_guard fault(m_mutex);
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
	m_tell = StormByte::Size{0};
	return { Status::Ok, 0 };
}

Result BufferedWriter::Write(const FIFO& src) {
	const StormByte::Size need = src.AvailableBytes();
	if (need == StormByte::Size{0})
		return { Status::Ok, 0 };
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle)
			return { Status::Failed, 0 };
		if (!WouldAccept(need))
			return { Status::TryAgain, 0 };
	}
	Data chunk;
	if (!src.Read(need, chunk))
		return { Status::Failed, 0 };
	return WriteSpan(std::span<const std::byte>(chunk.data(), static_cast<std::size_t>(chunk.size())));
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
		if (!WouldAccept(StormByte::Size{src.size()}))
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
		m_tell = m_tell + StormByte::Size{src.size()};
		return { Status::Ok, StormByte::Size{src.size()} };
	}

	if (!m_ring)
		return { Status::Failed, 0 };

	if (!m_ring->Write(src))
		return { Status::Failed, 0 };

	{
		std::lock_guard lock(m_mutex);
		m_tell = m_tell + StormByte::Size{src.size()};
	}
	RequestDrain();
	return { Status::Ok, StormByte::Size{src.size()} };
}

StormByte::Size BufferedWriter::Tell() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_tell;
}

StormByte::Size BufferedWriter::Dirty() const noexcept {
	if (!m_ring)
		return StormByte::Size{0};
	return m_ring->AvailableBytes();
}

StormByte::Size BufferedWriter::WriteChunk() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_write_chunk;
}

void BufferedWriter::WriteChunk(const StormByte::Size bytes) {
	const bool disable = bytes == StormByte::Size{0} || m_back_pressure == 0;
	if (disable && Dirty() > StormByte::Size{0})
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
	const StormByte::Size unit = WriteChunk();
	const StormByte::Size cap = (chunks == 0 || unit == StormByte::Size{0})
		? StormByte::Size{0}
		: chunks * unit;
	if (cap == StormByte::Size{0} || Dirty() > cap)
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
	m_stop.store(false, std::memory_order_release);
	m_worker = std::thread(&BufferedWriter::Worker, this);
}

void BufferedWriter::StopWorker() {
	m_stop.store(true, std::memory_order_release);
	m_cv.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

void BufferedWriter::RequestDrain() const {
	std::lock_guard lock(m_mutex);
	if (!BufferedMode() || !m_ring)
		return;
	if (m_ring->AvailableBytes() < m_write_chunk
			&& !m_flush.load(std::memory_order_acquire))
		return;
	m_drain_run = true;
	m_cv.notify_all();
}

void BufferedWriter::Worker() {
	for (;;) {
		std::unique_lock lock(m_mutex);
		m_cv.wait(lock, [this] {
			return m_stop.load(std::memory_order_acquire)
				|| m_drain_run
				|| m_flush.load(std::memory_order_acquire);
		});
		if (m_stop.load(std::memory_order_acquire)) {
			m_cv.notify_all();
			return;
		}

		const StormByte::Size chunk = m_write_chunk;
		lock.unlock();

		while (!m_stop.load(std::memory_order_acquire) && m_ring && m_owner) {
			const bool flush = m_flush.load(std::memory_order_acquire);
			const StormByte::Size dirty = m_ring->AvailableBytes();
			if (dirty == StormByte::Size{0})
				break;
			if (!flush && dirty < chunk)
				break;

			auto front = m_ring->FrontSpan();
			if (front.empty())
				break;
			const StormByte::Size want = flush
				? StormByte::Size{front.size()}
				: std::min(StormByte::Size{front.size()}, chunk);
			front = front.first(static_cast<std::size_t>(want));

			const Result pushed = PushAll(front);
			if (pushed.status != Status::Ok) {
				std::lock_guard inner(m_mutex);
				m_failed = true;
				if (m_state == State::Idle)
					m_state = State::Fault;
				m_cv.notify_all();
				break;
			}
			static_cast<void>(m_ring->Consume(StormByte::Size{front.size()}));
		}

		lock.lock();
		m_drain_run = false;
		m_cv.notify_all();
		if (!m_stop.load(std::memory_order_acquire)
				&& m_flush.load(std::memory_order_acquire)
				&& m_ring
				&& m_ring->AvailableBytes() > StormByte::Size{0}) {
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

	StormByte::Size off{0};
	while (off < StormByte::Size{data.size()}) {
		if (m_stop.load(std::memory_order_acquire))
			return { Status::Failed, 0 };
		if (m_max_wait.count() != 0 && std::chrono::steady_clock::now() >= deadline)
			return { Status::Error, off };

		const auto rest = data.subspan(static_cast<std::size_t>(off));
		const Result pushed = m_owner->OriginPush(rest);
		if (pushed.status == Status::Failed || pushed.status == Status::Error)
			return { pushed.status, off };
		if (pushed.count == StormByte::Size{0})
			return { Status::Error, off };
		if (pushed.count > StormByte::Size{rest.size()})
			return { Status::Error, off };
		off = off + pushed.count;
	}
	return { Status::Ok, StormByte::Size{data.size()} };
}
