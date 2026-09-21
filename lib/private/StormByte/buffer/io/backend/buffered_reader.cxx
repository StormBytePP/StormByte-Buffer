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

#include <StormByte/buffer/io/backend/buffered_reader.hxx>

using namespace StormByte::Buffer::IO::Backend;
using Result = StormByte::Buffer::IO::Result;
using State = StormByte::Buffer::IO::State;

BufferedReader::BufferedReader(IO::BufferedReader& owner, const std::size_t read_ahead,
		const std::size_t max_memory, const std::chrono::milliseconds max_wait):
	m_owner(&owner),
	m_read_ahead(read_ahead),
	m_max_memory(max_memory),
	m_max_wait(max_wait),
	m_state(State::Unavailable) {
	StartWorker();
}

BufferedReader::~BufferedReader() {
	Shutdown();
}

void BufferedReader::Rebind(IO::BufferedReader& owner) noexcept {
	m_owner = &owner;
}

BufferedReader::operator bool() const noexcept {
	return IsReadable();
}

State BufferedReader::State() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_state;
}

void BufferedReader::SetState(const enum State state) noexcept {
	std::lock_guard lock(m_mutex);
	m_state = state;
}

bool BufferedReader::Open() {
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

Result BufferedReader::Close() {
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

void BufferedReader::Shutdown() {
	FlushPrefetch();
	StopWorker();
	std::lock_guard lock(m_mutex);
	m_open = false;
	m_state = State::Unavailable;
	m_origin_exhausted = true;
	DropWindow();
}

bool BufferedReader::Rewind() {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open)
			return false;
	}
	static_cast<void>(Close());
	return Open();
}

bool BufferedReader::IsOpen() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_open;
}

bool BufferedReader::IsReadable() const noexcept {
	std::lock_guard lock(m_mutex);
	if (m_state != State::Idle || !m_open || m_failed)
		return false;
	return !(m_origin_exhausted && m_window.AvailableBytes() == 0);
}

bool BufferedReader::EoF() const noexcept {
	std::lock_guard lock(m_mutex);
	if (!m_open)
		return true;
	return m_origin_exhausted && m_window.AvailableBytes() == 0;
}

Result BufferedReader::Read(const std::size_t n, FIFO& dest) const {
	return Serve(n, dest, true);
}

Result BufferedReader::Peek(const std::size_t n, FIFO& dest) const {
	return Serve(n, dest, false);
}

Result BufferedReader::Seek(const std::ptrdiff_t offset, const Position mode) const {
	FlushPrefetch();

	IO::BufferedReader* owner = nullptr;
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

std::size_t BufferedReader::Tell() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_tell;
}

bool BufferedReader::IsSeekable() const noexcept {
	return m_owner && m_owner->OriginCanSeek();
}

bool BufferedReader::IsSized() const noexcept {
	return m_owner && m_owner->OriginHasSize();
}

std::optional<std::size_t> BufferedReader::Size() const noexcept {
	if (!m_owner)
		return std::nullopt;
	return m_owner->OriginSize();
}

std::size_t BufferedReader::ReadAhead() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_read_ahead;
}

void BufferedReader::ReadAhead(const std::size_t bytes) {
	FlushPrefetch();
	std::lock_guard lock(m_mutex);
	m_read_ahead = bytes;
	if (bytes == 0 || m_window.AvailableBytes() > bytes)
		DropWindow();
	else
		TrimWindow();
}

std::size_t BufferedReader::MaxMemory() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_max_memory;
}

void BufferedReader::MaxMemory(const std::size_t bytes) {
	FlushPrefetch();
	std::lock_guard lock(m_mutex);
	m_max_memory = bytes;
	TrimWindow();
}

std::chrono::milliseconds BufferedReader::MaxWait() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_max_wait;
}

void BufferedReader::MaxWait(const std::chrono::milliseconds wait) {
	std::lock_guard lock(m_mutex);
	m_max_wait = wait;
}

void BufferedReader::StartWorker() {
	if (m_worker.joinable())
		return;
	m_stop.store(false);
	m_worker = std::thread(&BufferedReader::Worker, this);
}

void BufferedReader::StopWorker() {
	m_stop.store(true);
	m_cancel_prefetch.store(true);
	m_cv.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

void BufferedReader::RequestPrefetch() const {
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

void BufferedReader::FlushPrefetch() const {
	m_cancel_prefetch.store(true);
	m_cv.notify_all();
	std::unique_lock lock(m_mutex);
	m_cv.wait(lock, [this] {
		return m_stop.load() || !m_prefetch_run;
	});
}

void BufferedReader::Worker() {
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

void BufferedReader::DropWindow() const {
	m_window.Clear();
	m_window_origin = m_tell;
}

Result BufferedReader::PullIntoWindow(const std::size_t n) const {
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

Result BufferedReader::Serve(const std::size_t n, FIFO& dest, const bool consume) const {
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

void BufferedReader::TrimWindow() const {
	if (m_max_memory == 0) {
		DropWindow();
		return;
	}
	if (m_window.Size() > m_max_memory)
		m_window.Clean();
}
