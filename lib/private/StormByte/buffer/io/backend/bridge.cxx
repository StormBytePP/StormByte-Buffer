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

#include <StormByte/buffer/io/backend/bridge.hxx>

#include <algorithm>
#include <chrono>

using namespace StormByte::Buffer;

IO::Backend::Bridge::Bridge(ExternalReader& in, ExternalWriter& out, const std::size_t high_water) noexcept:
	m_ext_in(&in),
	m_ext_out(&out),
	m_high_water(high_water),
	m_status(IO::Drainer::Status::Started) {
	Launch();
}

IO::Backend::Bridge::Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out, const std::size_t high_water) noexcept:
	m_io_in(&in),
	m_io_out(&out),
	m_high_water(high_water),
	m_status(IO::Drainer::Status::Started) {
	Launch();
}

IO::Backend::Bridge::Bridge(ExternalReader& in, IO::BufferedWriter& out, const std::size_t high_water) noexcept:
	m_ext_in(&in),
	m_io_out(&out),
	m_high_water(high_water),
	m_status(IO::Drainer::Status::Started) {
	Launch();
}

IO::Backend::Bridge::Bridge(const IO::BufferedReader& in, ExternalWriter& out, const std::size_t high_water) noexcept:
	m_ext_out(&out),
	m_io_in(&in),
	m_high_water(high_water),
	m_status(IO::Drainer::Status::Started) {
	Launch();
}

IO::Backend::Bridge::~Bridge() noexcept {
	{
		std::lock_guard lock(m_mutex);
		m_stop = true;
	}
	m_cv.notify_all();
	if (m_worker.joinable())
		m_worker.join();
}

void IO::Backend::Bridge::Launch() noexcept {
	m_worker = std::thread([this] { Worker(); });
}

bool IO::Backend::Bridge::EoF() const noexcept {
	if (m_ext_in)
		return m_ext_in->EoF();
	if (m_io_in)
		return m_io_in->EoF();
	return true;
}

bool IO::Backend::Bridge::IsReadable() const noexcept {
	if (m_ext_in)
		return m_ext_in->IsReadable();
	if (m_io_in)
		return static_cast<bool>(*m_io_in);
	return false;
}

bool IO::Backend::Bridge::IsWritable() const noexcept {
	if (m_ext_out)
		return m_ext_out->IsWritable();
	if (m_io_out)
		return static_cast<bool>(*m_io_out);
	return false;
}

std::size_t IO::Backend::Bridge::HighWater() const noexcept {
	return m_high_water.load();
}

void IO::Backend::Bridge::HighWater(const std::size_t high_water) noexcept {
	m_high_water.store(high_water);
	m_cv.notify_all();
}

IO::Drainer::Status IO::Backend::Bridge::Drainer() const noexcept {
	std::lock_guard lock(m_mutex);
	return m_status;
}

bool IO::Backend::Bridge::Drainer(const IO::Drainer::Operation operation) noexcept {
	std::unique_lock lock(m_mutex);
	if (operation == IO::Drainer::Operation::Toggle) {
		if (m_status == IO::Drainer::Status::Started)
			m_status = IO::Drainer::Status::Paused;
		else if (m_status == IO::Drainer::Status::Paused)
			m_status = IO::Drainer::Status::Started;
		else
			return false;
		m_cv.notify_all();
		return true;
	}

	if (operation != IO::Drainer::Operation::Flush)
		return false;

	m_hurry = true;
	m_cv.notify_all();
	m_cv.wait(lock, [this] { return m_stop || !m_hurry; });
	return !m_failed;
}

bool IO::Backend::Bridge::BarrierFlush() noexcept {
	std::unique_lock lock(m_mutex);
	m_barrier = true;
	m_cv.notify_all();
	m_cv.wait(lock, [this] { return m_stop || !m_barrier; });
	return !m_failed;
}

bool IO::Backend::Bridge::FlushAndClose() noexcept {
	const bool ok = BarrierFlush();
	if (m_ext_out)
		m_ext_out->Close();
	return ok;
}

void IO::Backend::Bridge::SetError() noexcept {
	if (m_ext_out)
		m_ext_out->SetError();
}

bool IO::Backend::Bridge::SourceDone() const noexcept {
	if (m_ext_in)
		return m_ext_in->EoF() && m_ext_in->AvailableBytes() == 0;
	if (m_io_in)
		return m_io_in->EoF();
	return true;
}

std::size_t IO::Backend::Bridge::AvailableNow() const noexcept {
	if (m_ext_in)
		return m_ext_in->AvailableBytes();
	return 0;
}

std::size_t IO::Backend::Bridge::OccupiedNow() const noexcept {
	if (m_ext_out)
		return m_ext_out->Occupied();
	if (m_io_out)
		return m_io_out->Dirty();
	return 0;
}

bool IO::Backend::Bridge::DestFlush() noexcept {
	if (!m_io_out)
		return true;
	return m_io_out->Flush().status == IO::Status::Ok;
}

bool IO::Backend::Bridge::Pull(const std::size_t n, FIFO& dest) noexcept {
	dest.Clear();
	if (n == 0)
		return true;

	if (m_ext_in) {
		DataType chunk;
		if (!m_ext_in->Extract(n, chunk)) {
			if (!m_ext_in->Read(n, chunk))
				return m_ext_in->EoF();
		}
		if (chunk.empty())
			return true;
		return dest.Write(std::move(chunk));
	}

	if (m_io_in) {
		const IO::Result pulled = m_io_in->Read(n, dest);
		if (pulled.status == IO::Status::Ok || pulled.status == IO::Status::End)
			return true;
		return m_io_in->EoF();
	}

	return false;
}

bool IO::Backend::Bridge::Push(FIFO& src) noexcept {
	if (src.AvailableBytes() == 0)
		return true;

	if (m_ext_out) {
		DataType chunk;
		if (!src.Extract(0, chunk))
			return false;
		if (chunk.empty())
			return true;
		return m_ext_out->Write(std::move(chunk));
	}

	if (m_io_out) {
		for (;;) {
			const IO::Result pushed = m_io_out->Write(src);
			if (pushed.status == IO::Status::Ok)
				return true;
			if (pushed.status != IO::Status::TryAgain)
				return false;
			static_cast<void>(m_io_out->Flush());
		}
	}

	return false;
}

bool IO::Backend::Bridge::Passthrough(const std::size_t bytes) noexcept {
	if (bytes == 0)
		return true;
	if (!IsWritable())
		return false;

	if (m_io_out) {
		while (!m_io_out->WillWrite(bytes)) {
			if (m_io_out->Flush().status != IO::Status::Ok)
				return false;
			if (!m_io_out->WillWrite(bytes))
				return false;
		}
	}

	FIFO work;
	if (!Pull(bytes, work))
		return false;
	return Push(work);
}

void IO::Backend::Bridge::Worker() noexcept {
	for (;;) {
		std::unique_lock lock(m_mutex);
		m_cv.wait(lock, [this] {
			return m_stop || m_barrier || m_hurry || m_status == IO::Drainer::Status::Started;
		});
		if (m_stop)
			break;

		const bool barrier = m_barrier;
		const bool hurry = m_hurry;
		m_busy = true;
		lock.unlock();

		if (SourceDone()) {
			if (barrier) {
				if (!DestFlush())
					m_failed = true;
			}
			lock.lock();
			m_busy = false;
			if (m_barrier) {
				m_barrier = false;
				m_cv.notify_all();
			}
			if (m_hurry) {
				m_hurry = false;
				m_cv.notify_all();
			}
			m_cv.wait(lock, [this] {
				return m_stop || m_barrier || m_hurry ||
					(m_status == IO::Drainer::Status::Started && !SourceDone());
			});
			continue;
		}

		if (!IsWritable()) {
			m_failed = true;
			lock.lock();
			m_busy = false;
			m_barrier = false;
			m_hurry = false;
			m_cv.notify_all();
			continue;
		}

		const std::size_t hw = m_high_water.load();
		const std::size_t occupied = OccupiedNow();
		std::size_t room = 0;
		if (hw == 0)
			room = m_chunk_max;
		else if (hw > occupied)
			room = hw - occupied;

		if (!hurry && !barrier && hw != 0 && room == 0) {
			lock.lock();
			m_busy = false;
			m_cv.wait_for(lock, std::chrono::milliseconds(10), [this] {
				return m_stop || m_barrier || m_hurry ||
					m_status != IO::Drainer::Status::Started ||
					m_high_water.load() == 0 ||
					(m_high_water.load() > OccupiedNow());
			});
			continue;
		}

		std::size_t want = 0;
		if (hurry) {
			if (m_io_in)
				want = 1;
			else
				want = AvailableNow();
		} else {
			want = std::min(m_chunk_max, room == 0 ? m_chunk_min : room);
			if (want < m_chunk_min)
				want = 0;
			if (m_io_in) {
				if (want == 0)
					want = std::min(m_chunk_min, room == 0 ? m_chunk_min : room);
			} else {
				const std::size_t avail = AvailableNow();
				if (avail == 0) {
					lock.lock();
					m_busy = false;
					m_cv.wait_for(lock, std::chrono::milliseconds(10), [this] {
						return m_stop || m_barrier || m_hurry ||
							m_status != IO::Drainer::Status::Started ||
							AvailableNow() > 0 || SourceDone();
					});
					continue;
				}
				want = std::min(want == 0 ? avail : want, avail);
			}
		}

		if (want > 0 && !Passthrough(want))
			m_failed = true;

		if (barrier) {
			if (!DestFlush())
				m_failed = true;
		}

		lock.lock();
		m_busy = false;
		if (m_barrier) {
			m_barrier = false;
			m_cv.notify_all();
		}
		if (m_hurry) {
			m_hurry = false;
			m_cv.notify_all();
		}
	}
}
