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

#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/buffer/io/device_throughput.hxx>

#include <cstdint>
#include <ios>
#include <system_error>

#ifdef WINDOWS
#	include <windows.h>
#else
#	include <sys/statvfs.h>
#endif

using namespace StormByte::Buffer::IO;

namespace {
	constexpr std::size_t MinWindow = 16ull * 1024ull;
	constexpr std::size_t MaxWindow = 1024ull * 1024ull;
	constexpr std::size_t DefaultBackPressure = 4;

	std::size_t WindowFromBps(const std::size_t bps) noexcept {
		const std::size_t raw = bps / 500ull;
		if (raw < MinWindow)
			return MinWindow;
		if (raw > MaxWindow)
			return MaxWindow;
		return raw;
	}

	std::filesystem::path SpacePath(const std::filesystem::path& path) {
		std::error_code ec;
		const auto parent = path.parent_path();
		if (!parent.empty())
			return parent;
		const auto cwd = std::filesystem::current_path(ec);
		if (!ec)
			return cwd;
		return {};
	}

	bool VolumeHas(const std::filesystem::path& path, const std::size_t n) {
		if (n == 0)
			return true;
		const auto probe = SpacePath(path);
		if (probe.empty())
			return false;

#ifdef WINDOWS
		ULARGE_INTEGER avail {};
		const auto wide = probe.wstring();
		if (wide.empty())
			return false;
		if (!GetDiskFreeSpaceExW(wide.c_str(), &avail, nullptr, nullptr))
			return false;
		return avail.QuadPart >= static_cast<ULONGLONG>(n);
#else
		struct statvfs st {};
		if (statvfs(probe.c_str(), &st) != 0)
			return false;
		const std::uint64_t avail = static_cast<std::uint64_t>(st.f_bavail) *
			static_cast<std::uint64_t>(st.f_frsize);
		return avail >= static_cast<std::uint64_t>(n);
#endif
	}

	bool OpenRandomAccess(std::ofstream& file, const std::filesystem::path& path) {
		file.open(path, std::ios::in | std::ios::out | std::ios::binary);
		if (file)
			return true;
		file.clear();
		file.open(path, std::ios::out | std::ios::binary);
		if (!file)
			return false;
		file.close();
		file.open(path, std::ios::in | std::ios::out | std::ios::binary);
		return static_cast<bool>(file);
	}
}

BufferedFileWriter::BufferedFileWriter(std::filesystem::path path):
	BufferedWriter(0, 0),
	m_path(std::move(path)),
	m_probe_on_setup(true) {}

BufferedFileWriter::BufferedFileWriter(std::filesystem::path path, const std::size_t write_chunk,
		const std::size_t back_pressure, const std::chrono::milliseconds max_wait):
	BufferedWriter(write_chunk, back_pressure, max_wait),
	m_path(std::move(path)),
	m_probe_on_setup(false) {}

BufferedFileWriter::BufferedFileWriter(BufferedFileWriter&& other) noexcept:
	BufferedWriter(std::move(other)),
	m_path(std::move(other.m_path)),
	m_file(std::move(other.m_file)),
	m_probe_on_setup(other.m_probe_on_setup) {
	other.m_probe_on_setup = false;
}

BufferedFileWriter::~BufferedFileWriter() noexcept {
	static_cast<void>(Close());
}

BufferedFileWriter& BufferedFileWriter::operator=(BufferedFileWriter&& other) noexcept {
	if (this != &other) {
		static_cast<void>(Close());
		BufferedWriter::operator=(std::move(other));
		m_path = std::move(other.m_path);
		m_file = std::move(other.m_file);
		m_probe_on_setup = other.m_probe_on_setup;
		other.m_probe_on_setup = false;
	}
	return *this;
}

const std::filesystem::path& BufferedFileWriter::Path() const noexcept {
	return m_path;
}

std::size_t BufferedFileWriter::Size() const noexcept {
	std::error_code ec;
	const auto disk = std::filesystem::file_size(m_path, ec);
	const std::size_t on_disk = ec ? 0 : static_cast<std::size_t>(disk);
	const std::size_t logical = Tell();
	return on_disk > logical ? on_disk : logical;
}

Result BufferedFileWriter::Seek(const std::ptrdiff_t offset, const Position mode) {
	const auto flushed = Flush();
	if (flushed.status != Status::Ok)
		return flushed;

	std::size_t abs = Tell();
	if (mode == Position::Absolute) {
		if (offset < 0)
			return { Status::Failed, 0 };
		abs = static_cast<std::size_t>(offset);
	} else {
		if (offset < 0 && static_cast<std::size_t>(-offset) > abs)
			return { Status::Failed, 0 };
		abs = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(abs) + offset);
	}

	const auto moved = OriginSeek(abs);
	if (moved.status != Status::Ok)
		return moved;
	SetTell(abs);
	return { Status::Ok, 0 };
}

void BufferedFileWriter::Setup() {
	if (!m_probe_on_setup)
		return;
	const auto rate = ProbeDeviceThroughput(m_path);
	WriteChunk(WindowFromBps(rate.write_bps));
	BackPressure(DefaultBackPressure);
}

bool BufferedFileWriter::WillWrite(const std::size_t n) const {
	if (!BufferedWriter::WillWrite(n))
		return false;
	return VolumeHas(m_path, n);
}

Result BufferedFileWriter::OriginOpen() {
	std::lock_guard lock(m_file_mutex);
	if (m_file.is_open())
		return { Status::Failed, 0 };

	std::error_code ec;
	const auto parent = m_path.parent_path();
	if (!parent.empty()) {
		const auto pst = std::filesystem::status(parent, ec);
		if (ec) {
			SetState(State::Missing);
			return { Status::Failed, 0 };
		}
		if (!std::filesystem::is_directory(pst)) {
			SetState(State::Missing);
			return { Status::Failed, 0 };
		}
	}

	const auto st = std::filesystem::status(m_path, ec);
	if (ec && ec != std::errc::no_such_file_or_directory) {
		SetState(State::NotWritable);
		return { Status::Failed, 0 };
	}

	if (!ec) {
		if (std::filesystem::is_directory(st)) {
			SetState(State::Directory);
			return { Status::Failed, 0 };
		}
		if (!std::filesystem::is_regular_file(st)) {
			SetState(State::NotWritable);
			return { Status::Failed, 0 };
		}
	}

	if (!OpenRandomAccess(m_file, m_path)) {
		SetState(State::NotWritable);
		return { Status::Failed, 0 };
	}

	SetState(State::Idle);
	return { Status::Ok, 0 };
}

Result BufferedFileWriter::OriginClose() {
	std::lock_guard lock(m_file_mutex);
	if (m_file.is_open())
		m_file.close();
	SetState(State::Unavailable);
	return { Status::Ok, 0 };
}

Result BufferedFileWriter::OriginPush(const std::span<const std::byte> data) {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { Status::Failed, 0 };
	if (data.empty())
		return { Status::Ok, 0 };

	m_file.write(reinterpret_cast<const char*>(data.data()),
		static_cast<std::streamsize>(data.size()));
	if (m_file.bad()) {
		SetState(State::Fault);
		return { Status::Error, 0 };
	}

	return { Status::Ok, data.size() };
}

Result BufferedFileWriter::OriginFlush() {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { Status::Failed, 0 };
	m_file.flush();
	if (m_file.bad()) {
		SetState(State::Fault);
		return { Status::Error, 0 };
	}
	return { Status::Ok, 0 };
}

Result BufferedFileWriter::OriginTruncate() {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { Status::Failed, 0 };

	m_file.close();
	std::error_code ec;
	std::filesystem::resize_file(m_path, 0, ec);
	if (ec) {
		SetState(State::Fault);
		return { Status::Failed, 0 };
	}

	if (!OpenRandomAccess(m_file, m_path)) {
		SetState(State::NotWritable);
		return { Status::Failed, 0 };
	}
	return { Status::Ok, 0 };
}

Result BufferedFileWriter::OriginSeek(const std::size_t absolute) {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { Status::Failed, 0 };
	m_file.clear();
	m_file.seekp(static_cast<std::streamoff>(absolute), std::ios::beg);
	if (!m_file)
		return { Status::Failed, 0 };
	return { Status::Ok, 0 };
}
