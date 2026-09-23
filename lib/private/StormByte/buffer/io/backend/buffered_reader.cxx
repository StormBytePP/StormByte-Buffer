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

#include <StormByte/buffer/io/backend/buffered_reader.hxx>

#include <limits>
#include <utility>
#include <vector>

using namespace StormByte::Buffer::IO::Backend;
using Result = StormByte::Buffer::IO::Result;
using State = StormByte::Buffer::IO::State;
using Status = StormByte::Buffer::IO::Status;
using FIFO = StormByte::Buffer::FIFO;

namespace {
	constexpr StormByte::Size PullBatch{4096};

	StormByte::Size SpanEnd(const StormByte::Size start, const StormByte::Buffer::FIFO& fifo) noexcept {
		return start + fifo.AvailableBytes();
	}

	StormByte::Size DistanceToTell(const StormByte::Size start, const StormByte::Buffer::FIFO& fifo,
			const StormByte::Size tell) noexcept {
		const StormByte::Size end = SpanEnd(start, fifo);
		if (tell >= start && tell < end)
			return StormByte::Size{0};
		if (tell < start)
			return start - tell;
		return tell - end;
	}

	bool OffsetFits(const StormByte::Size value) noexcept {
		return value <= StormByte::Size{static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())};
	}

	void AppendFifo(FIFO& dest, FIFO& piece) {
		const StormByte::Size n = piece.AvailableBytes();
		if (n == StormByte::Size{0})
			return;
		static_cast<void>(dest.Write(n, std::move(piece)));
	}
}

BufferedReader::BufferedReader(IO::BufferedReader& owner, const StormByte::Size read_ahead,
		const StormByte::Size max_memory, const std::chrono::milliseconds max_wait):
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
	m_tell = StormByte::Size{0};
	m_origin_pos = StormByte::Size{0};
	m_origin_valid = true;
	DropCache();
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
	m_origin_valid = false;
	m_state = State::Unavailable;
	DropCache();
	return { Status::Ok, 0 };
}

void BufferedReader::Shutdown() {
	FlushPrefetch();
	StopWorker();
	std::lock_guard lock(m_mutex);
	m_open = false;
	m_state = State::Unavailable;
	m_origin_exhausted = true;
	m_origin_valid = false;
	DropCache();
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
	return !(m_origin_exhausted && CoverageFrom(m_tell) == StormByte::Size{0});
}

bool BufferedReader::EoF() const noexcept {
	std::lock_guard lock(m_mutex);
	if (!m_open)
		return true;
	return m_origin_exhausted && CoverageFrom(m_tell) == StormByte::Size{0};
}

Result BufferedReader::Read(const StormByte::Size n, FIFO& dest) const {
	return Serve(n, dest, true);
}

Result BufferedReader::Peek(const StormByte::Size n, FIFO& dest) const {
	return Serve(n, dest, false);
}

Result BufferedReader::Seek(const std::ptrdiff_t offset, const Position mode) const {
	FlushPrefetch();

	StormByte::Size target{0};
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle || !m_owner)
			return { Status::Failed, 0 };
		if (!m_owner->OriginCanSeek())
			return { Status::Failed, 0 };

		target = m_tell;
		if (mode == Position::Absolute) {
			if (offset < 0)
				return { Status::Failed, 0 };
			target = StormByte::Size{static_cast<std::size_t>(offset)};
		}
		else {
			if (offset < 0) {
				const StormByte::Size back{static_cast<std::size_t>(-offset)};
				if (back > m_tell)
					return { Status::Failed, 0 };
				target = m_tell - back;
			}
			else {
				target = m_tell + StormByte::Size{static_cast<std::size_t>(offset)};
			}
		}
	}

	if (!OffsetFits(target))
		return { Status::Failed, 0 };

	const Result seeked = m_owner->OriginSeek(static_cast<std::ptrdiff_t>(target), Position::Absolute);
	if (seeked.status != Status::Ok)
		return { Status::Failed, 0 };

	std::lock_guard lock(m_mutex);
	m_tell = target;
	m_origin_pos = target;
	m_origin_valid = true;
	m_origin_exhausted = false;
	return { Status::Ok, 0 };
}

StormByte::Size BufferedReader::Tell() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_tell;
}

bool BufferedReader::IsSeekable() const noexcept {
	return m_owner && m_owner->OriginCanSeek();
}

bool BufferedReader::IsSized() const noexcept {
	return m_owner && m_owner->OriginHasSize();
}

std::optional<StormByte::Size> BufferedReader::Size() const noexcept {
	if (!m_owner)
		return std::nullopt;
	return m_owner->OriginSize();
}

StormByte::Size BufferedReader::ReadAhead() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_read_ahead;
}

void BufferedReader::ReadAhead(const StormByte::Size bytes) {
	FlushPrefetch();
	std::lock_guard lock(m_mutex);
	m_read_ahead = bytes;
	CollectGarbage();
}

StormByte::Size BufferedReader::MaxMemory() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_max_memory;
}

void BufferedReader::MaxMemory(const StormByte::Size bytes) {
	FlushPrefetch();
	std::lock_guard lock(m_mutex);
	m_max_memory = bytes;
	CollectGarbage();
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
	if (m_read_ahead == StormByte::Size{0} || m_max_memory == StormByte::Size{0})
		return;
	if (CoverageFrom(m_tell) >= m_read_ahead)
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

		const StormByte::Size target = m_prefetch_target;
		lock.unlock();

		while (!m_stop.load() && !m_cancel_prefetch.load()) {
			StormByte::Size pull_at{0};
			StormByte::Size need{0};
			{
				std::lock_guard inner(m_mutex);
				if (!m_open || m_origin_exhausted || m_state != State::Idle)
					break;
				const StormByte::Size covered = CoverageFrom(m_tell);
				if (covered >= target)
					break;
				pull_at = m_tell + covered;
				need = target - covered;
				if (need > PullBatch)
					need = PullBatch;
				if (m_max_memory > StormByte::Size{0}) {
					const StormByte::Size used = CachedBytes();
					if (used >= m_max_memory) {
						CollectGarbage();
						if (CachedBytes() >= m_max_memory)
							break;
					}
				}
			}
			if (!m_owner)
				break;

			FIFO chunk;
			const Result pulled = PullAt(pull_at, need, chunk);
			std::lock_guard inner(m_mutex);
			if (pulled.status == Status::Failed || pulled.status == Status::Error) {
				m_failed = true;
				m_cv.notify_all();
				break;
			}
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

void BufferedReader::DropCache() const {
	m_spans.clear();
}

StormByte::Size BufferedReader::CachedBytes() const noexcept {
	StormByte::Size total{0};
	for (const auto& [start, fifo] : m_spans)
		total = total + fifo.AvailableBytes();
	return total;
}

StormByte::Size BufferedReader::CoverageFrom(const StormByte::Size pos) const noexcept {
	const auto it = FindSpan(pos);
	if (it == m_spans.end())
		return StormByte::Size{0};
	return SpanEnd(it->first, it->second) - pos;
}

std::map<StormByte::Size, FIFO>::iterator BufferedReader::FindSpan(const StormByte::Size pos) const noexcept {
	auto it = m_spans.upper_bound(pos);
	if (it == m_spans.begin())
		return m_spans.end();
	--it;
	if (pos < SpanEnd(it->first, it->second))
		return it;
	return m_spans.end();
}

bool BufferedReader::CopyFromCache(const StormByte::Size pos, const StormByte::Size n, FIFO& dest) const {
	const auto it = FindSpan(pos);
	if (it == m_spans.end())
		return false;
	if (n > SpanEnd(it->first, it->second) - pos)
		return false;
	FIFO view = it->second;
	view.Seek(static_cast<std::ptrdiff_t>(pos - it->first), Position::Absolute);
	return view.Peek(n, dest);
}

void BufferedReader::EraseRange(const StormByte::Size from, const StormByte::Size to) const {
	if (from >= to)
		return;

	auto it = m_spans.begin();
	while (it != m_spans.end()) {
		const StormByte::Size start = it->first;
		const StormByte::Size end = SpanEnd(start, it->second);
		if (end <= from || start >= to) {
			++it;
			continue;
		}

		FIFO left;
		FIFO right;
		if (start < from) {
			FIFO view = it->second;
			static_cast<void>(view.Peek(from - start, left));
		}
		if (end > to) {
			FIFO view = it->second;
			view.Seek(static_cast<std::ptrdiff_t>(to - start), Position::Absolute);
			static_cast<void>(view.Peek(end - to, right));
		}

		it = m_spans.erase(it);
		if (left.AvailableBytes() > StormByte::Size{0})
			m_spans.emplace(start, std::move(left));
		if (right.AvailableBytes() > StormByte::Size{0})
			m_spans.emplace(to, std::move(right));
	}
}

void BufferedReader::CommitSpan(const StormByte::Size start, FIFO&& piece) const {
	if (m_max_memory == StormByte::Size{0} || piece.AvailableBytes() == StormByte::Size{0})
		return;

	const StormByte::Size piece_end = SpanEnd(start, piece);
	auto it = m_spans.upper_bound(start);
	if (it != m_spans.begin()) {
		auto prev = std::prev(it);
		if (SpanEnd(prev->first, prev->second) >= start)
			it = prev;
	}

	StormByte::Size merged_start = start;
	StormByte::Size merged_end = piece_end;
	std::vector<std::pair<StormByte::Size, FIFO>> parts;
	parts.emplace_back(start, std::move(piece));

	while (it != m_spans.end() && it->first <= merged_end) {
		const StormByte::Size end = SpanEnd(it->first, it->second);
		if (end < merged_start)
			break;
		merged_start = merged_start < it->first ? merged_start : it->first;
		merged_end = merged_end > end ? merged_end : end;
		parts.emplace_back(it->first, std::move(it->second));
		it = m_spans.erase(it);
	}

	FIFO merged;
	StormByte::Size cursor = merged_start;
	while (cursor < merged_end) {
		bool progressed = false;
		for (auto& [part_start, part] : parts) {
			const StormByte::Size part_end = SpanEnd(part_start, part);
			if (cursor < part_start || cursor >= part_end)
				continue;
			FIFO view = part;
			view.Seek(static_cast<std::ptrdiff_t>(cursor - part_start), Position::Absolute);
			FIFO slice;
			static_cast<void>(view.Peek(part_end - cursor, slice));
			if (slice.AvailableBytes() > StormByte::Size{0})
				static_cast<void>(merged.Write(slice.AvailableBytes(), std::move(slice)));
			cursor = part_end;
			progressed = true;
			break;
		}
		if (!progressed)
			break;
	}

	if (merged.AvailableBytes() > StormByte::Size{0})
		m_spans.insert_or_assign(merged_start, std::move(merged));
	CollectGarbage();
}

void BufferedReader::CollectGarbage() const {
	if (m_max_memory == StormByte::Size{0}) {
		DropCache();
		return;
	}

	while (CachedBytes() > m_max_memory && !m_spans.empty()) {
		auto victim = m_spans.end();
		StormByte::Size farthest{0};
		for (auto it = m_spans.begin(); it != m_spans.end(); ++it) {
			const StormByte::Size distance = DistanceToTell(it->first, it->second, m_tell);
			if (distance == StormByte::Size{0})
				continue;
			if (victim == m_spans.end() || distance > farthest) {
				victim = it;
				farthest = distance;
			}
		}

		if (victim != m_spans.end()) {
			m_spans.erase(victim);
			continue;
		}

		auto it = FindSpan(m_tell);
		if (it == m_spans.end()) {
			m_spans.erase(std::prev(m_spans.end()));
			continue;
		}

		const StormByte::Size start = it->first;
		const StormByte::Size end = SpanEnd(start, it->second);
		const StormByte::Size others = CachedBytes() - it->second.AvailableBytes();
		const StormByte::Size budget = m_max_memory > others ? m_max_memory - others : StormByte::Size{0};
		if (budget == StormByte::Size{0}) {
			m_spans.erase(it);
			break;
		}

		const StormByte::Size keep_from = m_tell < start ? start : m_tell;
		StormByte::Size keep_to = keep_from + budget;
		if (keep_to > end)
			keep_to = end;
		if (keep_from > start)
			EraseRange(start, keep_from);
		if (keep_to < end)
			EraseRange(keep_to, end);
		break;
	}
}

Result BufferedReader::EnsureOrigin(const StormByte::Size pos) const {
	if (!m_owner)
		return { Status::Failed, 0 };

	bool valid = false;
	StormByte::Size origin{0};
	bool seekable = false;
	bool exhausted = false;
	{
		std::lock_guard lock(m_mutex);
		valid = m_origin_valid;
		origin = m_origin_pos;
		exhausted = m_origin_exhausted;
		seekable = m_owner->OriginCanSeek();
	}

	if (valid && origin == pos && !exhausted)
		return { Status::Ok, 0 };
	if (!seekable)
		return { Status::Failed, 0 };
	if (!OffsetFits(pos))
		return { Status::Failed, 0 };

	const Result seeked = m_owner->OriginSeek(static_cast<std::ptrdiff_t>(pos), Position::Absolute);
	if (seeked.status != Status::Ok)
		return { Status::Failed, 0 };

	std::lock_guard lock(m_mutex);
	m_origin_pos = pos;
	m_origin_valid = true;
	m_origin_exhausted = false;
	return { Status::Ok, 0 };
}

Result BufferedReader::PullAt(const StormByte::Size at, const StormByte::Size n, FIFO& dest) const {
	if (!m_owner)
		return { Status::Failed, 0 };
	if (n == StormByte::Size{0})
		return { Status::Ok, 0 };

	const Result aligned = EnsureOrigin(at);
	if (aligned.status != Status::Ok)
		return aligned;

	FIFO chunk;
	const Result pulled = m_owner->OriginPull(n, chunk);
	if (pulled.status == Status::Failed || pulled.status == Status::Error)
		return { pulled.status, 0 };

	std::lock_guard lock(m_mutex);
	if (pulled.count > StormByte::Size{0}) {
		m_origin_pos = at + pulled.count;
		m_origin_valid = true;
		static_cast<void>(dest.Write(pulled.count, chunk));
		FIFO stored;
		static_cast<void>(chunk.Peek(pulled.count, stored));
		CommitSpan(at, std::move(stored));
	}
	if (pulled.status == Status::End)
		m_origin_exhausted = true;
	return { pulled.status, pulled.count };
}

Result BufferedReader::Serve(const StormByte::Size n, FIFO& dest, const bool consume) const {
	FlushPrefetch();

	std::chrono::milliseconds wait{0};
	{
		std::lock_guard lock(m_mutex);
		if (!m_open || m_failed || m_state != State::Idle || !m_owner)
			return { Status::Failed, 0 };
		wait = m_max_wait;
	}

	if (n == StormByte::Size{0}) {
		FIFO out;
		StormByte::Size count{0};
		{
			std::lock_guard lock(m_mutex);
			count = CoverageFrom(m_tell);
			if (count > StormByte::Size{0})
				static_cast<void>(CopyFromCache(m_tell, count, out));
			if (consume && count > StormByte::Size{0}) {
				EraseRange(m_tell, m_tell + count);
				m_tell = m_tell + count;
			}
		}
		if (count > StormByte::Size{0})
			dest = std::move(out);
		bool ended = false;
		{
			std::lock_guard lock(m_mutex);
			ended = count == StormByte::Size{0} && m_origin_exhausted;
		}
		RequestPrefetch();
		if (ended)
			return { Status::End, 0 };
		return { Status::Ok, count };
	}

	FIFO assembled;
	if (wait.count() != 0) {
		{
			std::lock_guard lock(m_mutex);
			if (CoverageFrom(m_tell) < n && !m_origin_exhausted && !m_failed) {
				m_prefetch_target = m_read_ahead > n ? m_read_ahead : n;
				m_prefetch_run = true;
				m_cancel_prefetch.store(false);
				m_cv.notify_all();
			}
		}
		std::unique_lock lock(m_mutex);
		const auto deadline = std::chrono::steady_clock::now() + wait;
		while (CoverageFrom(m_tell) < n && !m_origin_exhausted && !m_failed && !m_stop.load()) {
			if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout)
				break;
		}
		if (CoverageFrom(m_tell) < n && !m_origin_exhausted && !m_failed)
			return { Status::TryAgain, 0 };
	}

	for (;;) {
		StormByte::Size have = assembled.AvailableBytes();
		if (have >= n)
			break;

		StormByte::Size pos{0};
		StormByte::Size cached{0};
		bool exhausted = false;
		bool failed = false;
		{
			std::lock_guard lock(m_mutex);
			pos = m_tell + have;
			cached = CoverageFrom(pos);
			exhausted = m_origin_exhausted;
			failed = m_failed;
		}
		if (failed)
			return { Status::Failed, 0 };

		if (cached > StormByte::Size{0}) {
			const StormByte::Size take = cached < (n - have) ? cached : (n - have);
			if (!CopyFromCache(pos, take, assembled))
				return { Status::Failed, 0 };
			continue;
		}

		if (exhausted) {
			bool seekable = false;
			{
				std::lock_guard lock(m_mutex);
				seekable = m_owner && m_owner->OriginCanSeek();
			}
			if (!seekable)
				break;
		}

		FIFO piece;
		const Result pulled = PullAt(pos, n - have, piece);
		if (pulled.status == Status::Failed || pulled.status == Status::Error) {
			std::lock_guard lock(m_mutex);
			m_failed = true;
			return { pulled.status, 0 };
		}
		AppendFifo(assembled, piece);
		if (pulled.count == StormByte::Size{0})
			break;
	}

	const StormByte::Size take = assembled.AvailableBytes();
	bool exhausted = false;
	bool failed = false;
	{
		std::lock_guard lock(m_mutex);
		exhausted = m_origin_exhausted;
		failed = m_failed;
		if (take < n && !exhausted && !failed)
			return { Status::Failed, 0 };
		if (consume && take > StormByte::Size{0}) {
			EraseRange(m_tell, m_tell + take);
			m_tell = m_tell + take;
			CollectGarbage();
		}
	}

	if (take > StormByte::Size{0})
		dest = std::move(assembled);
	RequestPrefetch();
	const bool end = take < n || (take == StormByte::Size{0} && exhausted);
	return { end ? Status::End : Status::Ok, take };
}
