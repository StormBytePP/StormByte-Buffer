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

#include <StormByte/buffer/io/buffered_read.hxx>

using namespace StormByte::Buffer;

IO::BufferedRead::BufferedRead(Buffer::BufferedRead& owner, const std::size_t read_ahead,
		const std::size_t max_memory, const std::chrono::milliseconds max_wait):
	m_owner(&owner),
	m_read_ahead(read_ahead),
	m_max_memory(max_memory),
	m_max_wait(max_wait),
	m_state(State::Unavailable) {
	StartWorker();
}

IO::BufferedRead::~BufferedRead() {
	Shutdown();
}

void IO::BufferedRead::Rebind(Buffer::BufferedRead& owner) noexcept {
	m_owner = &owner;
}

IO::BufferedRead::operator bool() const noexcept {
	return IsReadable();
}

IO::State IO::BufferedRead::State() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_state;
}

void IO::BufferedRead::SetState(const enum State state) noexcept {
	std::lock_guard lock(m_mutex);
	m_state = state;
}

bool IO::BufferedRead::Open() {
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
	m_origin_exhausted = false;
	m_tell = 0;
	DropWindow();
	return m_state == State::Idle;
}

IO::Result IO::BufferedRead::Close() {
	FlushPrefetch();

	bool was_open = false;
	{
		std::lock_guard lock(m_mutex);
		was_open = m_open;
	}

	if (was_open && m_owner)
		static_cast<void>(m_owner->OriginClose());

	std::lock_guard lock(m_mutex);
	m_open = false;
	m_failed = false;
	m_origin_exhausted = true;
	m_state = State::Unavailable;
	DropWindow();
	return { Status::Ok, 0 };
}

void IO::BufferedRead::Shutdown() {
	FlushPrefetch();
	StopWorker();
	std::lock_guard lock(m_mutex);
	m_open = false;
	m_state = State::Unavailable;
	m_origin_exhausted = true;
	DropWindow();
}

bool IO::BufferedRead::Rewind() {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open)
			return false;
	}
	static_cast<void>(Close());
	return Open();
}

bool IO::BufferedRead::IsOpen() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_open;
}

bool IO::BufferedRead::IsReadable() const noexcept {
	std::lock_guard lock(m_mutex);
	if (m_state != State::Idle || !m_open || m_failed)
		return false;
	return !(m_origin_exhausted && m_window.AvailableBytes() == 0);
}

bool IO::BufferedRead::EoF() const noexcept {
	std::lock_guard lock(m_mutex);
	if (!m_open)
		return true;
	return m_origin_exhausted && m_window.AvailableBytes() == 0;
}

IO::Result IO::BufferedRead::Read(const std::size_t n, FIFO& dest) const {
	return Serve(n, dest, true);
}

IO::Result IO::BufferedRead::Peek(const std::size_t n, FIFO& dest) const {
	return Serve(n, dest, false);
}

IO::Result IO::BufferedRead::Seek(const std::ptrdiff_t offset, const Position mode) const {
	FlushPrefetch();

	Buffer::BufferedRead* owner = nullptr;
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle || !m_owner || !m_owner->OriginCanSeek())
			return { Status::Failed, 0 };
		owner = m_owner;
	}

	std::size_t target = 0;
	{
		std::lock_guard lock(m_mutex);
		target = m_tell;
		if (mode == Position::Absolute) {
			if (offset < 0)
				return { Status::Failed, 0 };
			target = static_cast<std::size_t>(offset);
		} else {
			if (offset < 0) {
				const auto back = static_cast<std::size_t>(-offset);
				if (back > m_tell)
					return { Status::Failed, 0 };
				target = m_tell - back;
			} else {
				target = m_tell + static_cast<std::size_t>(offset);
			}
		}
	}

	const Result seeked = owner->OriginSeek(static_cast<std::ptrdiff_t>(target), Position::Absolute);
	if (seeked.status != Status::Ok)
		return { Status::Failed, 0 };

	std::lock_guard lock(m_mutex);
	m_tell = target;
	m_origin_exhausted = false;
	const std::size_t win_end = m_window_origin + m_window.AvailableBytes();
	if (target < m_window_origin || target > win_end)
		DropWindow();
	return { Status::Ok, 0 };
}

std::size_t IO::BufferedRead::Tell() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_tell;
}

bool IO::BufferedRead::IsSeekable() const noexcept {
	return m_owner && m_owner->OriginCanSeek();
}

bool IO::BufferedRead::IsSized() const noexcept {
	return m_owner && m_owner->OriginHasSize();
}

std::optional<std::size_t> IO::BufferedRead::Size() const noexcept {
	if (!m_owner)
		return std::nullopt;
	return m_owner->OriginSize();
}

std::size_t IO::BufferedRead::ReadAhead() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_read_ahead;
}

void IO::BufferedRead::ReadAhead(const std::size_t bytes) {
	std::lock_guard lock(m_mutex);
	m_read_ahead = bytes;
}

std::size_t IO::BufferedRead::MaxMemory() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_max_memory;
}

void IO::BufferedRead::MaxMemory(const std::size_t bytes) {
	std::lock_guard lock(m_mutex);
	m_max_memory = bytes;
}

std::chrono::milliseconds IO::BufferedRead::MaxWait() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_max_wait;
}

void IO::BufferedRead::MaxWait(const std::chrono::milliseconds wait) {
	std::lock_guard lock(m_mutex);
	m_max_wait = wait;
}

void IO::BufferedRead::StartWorker() {
	if (m_worker.joinable())
		return;
	m_stop.store(false);
	m_worker = std::thread(&BufferedRead::Worker, this);
}

void IO::BufferedRead::StopWorker() {
	m_stop.store(true);
	m_cancel_prefetch.store(true);
	m_cv.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

void IO::BufferedRead::RequestPrefetch() const {
	std::lock_guard lock(m_mutex);
	if (!m_open || m_failed || m_state != State::Idle || m_origin_exhausted)
		return;
	if (m_max_memory == 0 && m_read_ahead == 0)
		return;
	m_prefetch_target = m_read_ahead;
	m_prefetch_run = true;
	m_cancel_prefetch.store(false);
	m_cv.notify_all();
}

void IO::BufferedRead::FlushPrefetch() const {
	m_cancel_prefetch.store(true);
	m_cv.notify_all();
	std::unique_lock lock(m_mutex);
	m_cv.wait(lock, [this] {
		return m_stop.load() || !m_prefetch_run;
	});
}

void IO::BufferedRead::Worker() {
	for (;;) {
		std::unique_lock lock(m_mutex);
		m_cv.wait(lock, [this] {
			return m_stop.load() || m_prefetch_run;
		});
		if (m_stop.load())
			return;

		const std::size_t target = m_prefetch_target;
		lock.unlock();

		while (!m_stop.load() && !m_cancel_prefetch.load()) {
			{
				std::lock_guard inner(m_mutex);
				if (!m_open || m_origin_exhausted || m_state != State::Idle || m_window.AvailableBytes() >= target)
					break;
			}
			if (!m_owner)
				break;
			FIFO chunk;
			constexpr std::size_t batch = 4096;
			const Result pulled = m_owner->OriginPull(batch, chunk);
			std::lock_guard inner(m_mutex);
			if (pulled.status == Status::Failed || pulled.status == Status::Error) {
				m_failed = true;
				m_cv.notify_all();
				break;
			}
			if (pulled.count > 0)
				static_cast<void>(m_window.Write(pulled.count, std::move(chunk)));
			if (pulled.status == Status::End)
				m_origin_exhausted = true;
			m_cv.notify_all();
			if (pulled.status != Status::Ok)
				break;
		}

		lock.lock();
		m_prefetch_run = false;
		m_cancel_prefetch.store(false);
		m_cv.notify_all();
	}
}

void IO::BufferedRead::DropWindow() const {
	m_window.Clear();
	m_window_origin = m_tell;
}

IO::Result IO::BufferedRead::PullIntoWindow(const std::size_t n) const {
	if (!m_owner)
		return { Status::Failed, 0 };
	std::size_t need = n;
	while (need > 0) {
		{
			std::lock_guard lock(m_mutex);
			if (m_origin_exhausted)
				break;
		}
		FIFO chunk;
		const Result pulled = m_owner->OriginPull(need, chunk);
		if (pulled.status == Status::Failed || pulled.status == Status::Error)
			return { pulled.status, 0 };
		{
			std::lock_guard lock(m_mutex);
			if (pulled.count > 0)
				static_cast<void>(m_window.Write(pulled.count, std::move(chunk)));
			if (pulled.count >= need)
				need = 0;
			else
				need -= pulled.count;
			if (pulled.status == Status::End) {
				m_origin_exhausted = true;
				break;
			}
		}
		if (pulled.count == 0)
			break;
	}
	std::lock_guard lock(m_mutex);
	return { m_origin_exhausted ? Status::End : Status::Ok, m_window.AvailableBytes() };
}

IO::Result IO::BufferedRead::Serve(const std::size_t n, FIFO& dest, const bool consume) const {
	FlushPrefetch();

	std::chrono::milliseconds wait{0};
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle || !m_owner)
			return { Status::Failed, 0 };
		wait = m_max_wait;
	}

	if (n == 0) {
		std::unique_lock lock(m_mutex);
		FIFO out;
		if (m_window.AvailableBytes() > 0) {
			if (consume)
				static_cast<void>(m_window.Extract(0, out));
			else
				static_cast<void>(m_window.Peek(0, out));
		}
		const std::size_t count = out.AvailableBytes();
		if (count > 0)
			dest = std::move(out);
		if (consume && count > 0) {
			m_tell += count;
			m_window_origin = m_tell;
			TrimWindow();
		}
		const bool ended = count == 0 && m_origin_exhausted;
		lock.unlock();
		RequestPrefetch();
		if (ended)
			return { Status::End, 0 };
		return { Status::Ok, count };
	}

	if (wait.count() == 0) {
		for (;;) {
			bool need_pull = false;
			{
				std::lock_guard lock(m_mutex);
				need_pull = m_window.AvailableBytes() < n && !m_origin_exhausted && !m_failed;
			}
			if (!need_pull)
				break;
			std::size_t missing = 0;
			{
				std::lock_guard lock(m_mutex);
				missing = n - m_window.AvailableBytes();
			}
			const Result pulled = PullIntoWindow(missing);
			if (pulled.status == Status::Failed || pulled.status == Status::Error) {
				std::lock_guard lock(m_mutex);
				m_failed = true;
				return { pulled.status, 0 };
			}
		}
	} else {
		{
			std::lock_guard lock(m_mutex);
			if (m_window.AvailableBytes() < n && !m_origin_exhausted && !m_failed) {
				const std::size_t need = n - m_window.AvailableBytes();
				m_prefetch_target = m_read_ahead > need ? m_read_ahead : need;
				m_prefetch_run = true;
				m_cancel_prefetch.store(false);
				m_cv.notify_all();
			}
		}
		std::unique_lock lock(m_mutex);
		const auto deadline = std::chrono::steady_clock::now() + wait;
		while (m_window.AvailableBytes() < n && !m_origin_exhausted && !m_failed && !m_stop.load()) {
			if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout)
				break;
		}
		if (m_window.AvailableBytes() < n && !m_origin_exhausted && !m_failed)
			return { Status::TryAgain, 0 };
	}

	std::unique_lock lock(m_mutex);
	const std::size_t available = m_window.AvailableBytes();
	const std::size_t take = available < n ? available : n;
	FIFO out;
	if (take > 0) {
		if (consume)
			static_cast<void>(m_window.Extract(take, out));
		else
			static_cast<void>(m_window.Peek(take, out));
	}

	if (take < n && !m_origin_exhausted && !m_failed)
		return { Status::Failed, 0 };

	if (take > 0)
		dest = std::move(out);
	if (consume && take > 0) {
		m_tell += take;
		if (m_window.AvailableBytes() == 0)
			m_window_origin = m_tell;
		TrimWindow();
	}
	const bool end = take < n || (take == 0 && m_origin_exhausted);
	lock.unlock();
	RequestPrefetch();
	return { end ? Status::End : Status::Ok, take };
}

void IO::BufferedRead::TrimWindow() const {
	if (m_max_memory == 0) {
		DropWindow();
		return;
	}
	if (m_window.Size() > m_max_memory)
		m_window.Clean();
}
