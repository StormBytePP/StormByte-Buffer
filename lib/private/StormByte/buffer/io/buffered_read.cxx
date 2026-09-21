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
		const std::size_t max_memory):
	m_owner(owner),
	m_read_ahead(read_ahead),
	m_max_memory(max_memory) {
	StartWorker();
}

IO::BufferedRead::~BufferedRead() {
	static_cast<void>(Close());
	StopWorker();
}

IO::BufferedRead::operator bool() const noexcept {
	return IsReadable();
}

IO::Result IO::BufferedRead::Open() {
	std::lock_guard lock(m_mutex);
	if (m_open)
		return { Status::Ok, 0 };
	const Result opened = m_owner.OriginOpen();
	if (opened.status != Status::Ok) {
		m_failed = true;
		m_open = false;
		return { Status::Failed, 0 };
	}
	m_open = true;
	m_failed = false;
	m_origin_exhausted = false;
	m_tell = 0;
	DropWindow();
	return { Status::Ok, 0 };
}

IO::Result IO::BufferedRead::Close() {
	FlushPrefetch();
	std::lock_guard lock(m_mutex);
	if (!m_open)
		return { Status::Ok, 0 };
	static_cast<void>(m_owner.OriginClose());
	m_open = false;
	m_origin_exhausted = true;
	DropWindow();
	return { Status::Ok, 0 };
}

IO::Result IO::BufferedRead::Rewind() {
	{
		std::lock_guard lock(m_mutex);
		if (!m_open)
			return { Status::Failed, 0 };
	}
	const Result closed = Close();
	if (closed.status == Status::Failed)
		return closed;
	return Open();
}

bool IO::BufferedRead::IsOpen() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_open;
}

bool IO::BufferedRead::IsReadable() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_open && !m_failed && !(m_origin_exhausted && m_window.AvailableBytes() == 0);
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
	std::lock_guard lock(m_mutex);
	if (!m_open || m_failed || !m_owner.OriginCanSeek())
		return { Status::Failed, 0 };

	std::size_t target = m_tell;
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

	const Result seeked = m_owner.OriginSeek(static_cast<std::ptrdiff_t>(target), Position::Absolute);
	if (seeked.status != Status::Ok)
		return { Status::Failed, 0 };

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
	return m_owner.OriginCanSeek();
}

bool IO::BufferedRead::IsSized() const noexcept {
	return m_owner.OriginHasSize();
}

std::optional<std::size_t> IO::BufferedRead::Size() const noexcept {
	return m_owner.OriginSize();
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
	if (!m_open || m_failed || m_max_memory == 0 || m_read_ahead == 0 || m_origin_exhausted)
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
				if (!m_open || m_origin_exhausted || m_window.AvailableBytes() >= target)
					break;
			}
			FIFO chunk;
			constexpr std::size_t batch = 4096;
			const Result pulled = m_owner.OriginPull(batch, chunk);
			std::lock_guard inner(m_mutex);
			if (pulled.status == Status::Failed) {
				m_failed = true;
				break;
			}
			if (pulled.count > 0 && pulled.status != Status::Failed)
				static_cast<void>(m_window.Write(pulled.count, std::move(chunk)));
			if (pulled.status == Status::End)
				m_origin_exhausted = true;
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
	std::size_t need = n;
	while (need > 0 && !m_origin_exhausted) {
		FIFO chunk;
		const Result pulled = m_owner.OriginPull(need, chunk);
		if (pulled.status == Status::Failed)
			return { Status::Failed, 0 };
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
		if (pulled.count == 0)
			break;
	}
	return { m_origin_exhausted ? Status::End : Status::Ok, m_window.AvailableBytes() };
}

IO::Result IO::BufferedRead::Serve(const std::size_t n, FIFO& dest, const bool consume) const {
	FlushPrefetch();

	std::unique_lock lock(m_mutex);
	if (!m_open || m_failed)
		return { Status::Failed, 0 };

	if (n == 0) {
		FIFO out;
		if (m_window.AvailableBytes() > 0) {
			if (consume)
				static_cast<void>(m_window.Extract(0, out));
			else
				static_cast<void>(m_window.Peek(0, out));
		}
		const std::size_t count = out.AvailableBytes();
		dest = std::move(out);
		if (consume) {
			m_tell += count;
			m_window_origin = m_tell;
			TrimWindow();
		}
		lock.unlock();
		if (consume || n == 0)
			RequestPrefetch();
		if (count == 0 && m_origin_exhausted)
			return { Status::End, 0 };
		return { Status::Ok, count };
	}

	while (m_window.AvailableBytes() < n && !m_origin_exhausted && !m_failed) {
		lock.unlock();
		const Result pulled = [&] {
			std::lock_guard inner(m_mutex);
			return PullIntoWindow(n - m_window.AvailableBytes());
		}();
		lock.lock();
		if (pulled.status == Status::Failed) {
			m_failed = true;
			return { Status::Failed, 0 };
		}
	}

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

	dest = std::move(out);
	if (consume) {
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
