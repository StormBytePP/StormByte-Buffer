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

#include <StormByte/buffer/buffered_file_reader.hxx>
#include <StormByte/buffer/fifo.hxx>

#include <ios>
#include <system_error>
#include <utility>

using namespace StormByte::Buffer;

BufferedFileReader::BufferedFileReader(std::filesystem::path path, const std::size_t read_ahead,
		const std::size_t max_memory):
	BufferedRead(read_ahead, max_memory),
	m_path(std::move(path)) {}

BufferedFileReader::BufferedFileReader(BufferedFileReader&& other) noexcept:
	BufferedRead(std::move(other)),
	m_path(std::move(other.m_path)),
	m_file(std::move(other.m_file)),
	m_size(other.m_size) {
	other.m_size.reset();
}

BufferedFileReader::~BufferedFileReader() noexcept {
	static_cast<void>(Close());
}

BufferedFileReader& BufferedFileReader::operator=(BufferedFileReader&& other) noexcept {
	if (this != &other) {
		static_cast<void>(Close());
		BufferedRead::operator=(std::move(other));
		m_path = std::move(other.m_path);
		m_file = std::move(other.m_file);
		m_size = other.m_size;
		other.m_size.reset();
	}
	return *this;
}

const std::filesystem::path& BufferedFileReader::Path() const noexcept {
	return m_path;
}

IO::Result BufferedFileReader::OriginOpen() {
	std::lock_guard lock(m_file_mutex);
	if (m_file.is_open())
		return { IO::Status::Failed, 0 };

	std::error_code ec;
	const auto st = std::filesystem::status(m_path, ec);
	if (ec) {
		if (ec == std::errc::no_such_file_or_directory)
			SetState(IO::State::Missing);
		else
			SetState(IO::State::Permission);
		return { IO::Status::Failed, 0 };
	}

	if (std::filesystem::is_directory(st)) {
		SetState(IO::State::Directory);
		return { IO::Status::Failed, 0 };
	}

	if (!std::filesystem::is_regular_file(st)) {
		SetState(IO::State::Permission);
		return { IO::Status::Failed, 0 };
	}

	const auto size = std::filesystem::file_size(m_path, ec);
	if (ec) {
		SetState(IO::State::Permission);
		return { IO::Status::Failed, 0 };
	}

	m_file.open(m_path, std::ios::in | std::ios::binary);
	if (!m_file) {
		SetState(IO::State::Permission);
		return { IO::Status::Failed, 0 };
	}

	m_size = static_cast<std::size_t>(size);
	SetState(IO::State::Idle);
	return { IO::Status::Ok, 0 };
}

IO::Result BufferedFileReader::OriginClose() {
	std::lock_guard lock(m_file_mutex);
	if (m_file.is_open())
		m_file.close();
	m_size.reset();
	SetState(IO::State::Unavailable);
	return { IO::Status::Ok, 0 };
}

IO::Result BufferedFileReader::OriginPull(const std::size_t n, FIFO& dest) {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { IO::Status::Failed, 0 };
	if (n == 0)
		return { IO::Status::Ok, 0 };

	DataType chunk(n);
	m_file.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(n));
	const auto got = static_cast<std::size_t>(m_file.gcount());
	if (m_file.bad()) {
		SetState(IO::State::Fault);
		return { IO::Status::Error, 0 };
	}

	chunk.resize(got);
	if (got > 0 && !dest.Write(got, std::move(chunk))) {
		SetState(IO::State::Fault);
		return { IO::Status::Error, 0 };
	}

	if (got < n || m_file.eof())
		return { IO::Status::End, got };
	return { IO::Status::Ok, got };
}

bool BufferedFileReader::OriginCanSeek() const noexcept {
	return true;
}

IO::Result BufferedFileReader::OriginSeek(const std::ptrdiff_t offset, const Position mode) {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { IO::Status::Failed, 0 };

	m_file.clear();
	if (mode == Position::Absolute) {
		if (offset < 0)
			return { IO::Status::Failed, 0 };
		m_file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	} else {
		m_file.seekg(static_cast<std::streamoff>(offset), std::ios::cur);
	}

	if (!m_file)
		return { IO::Status::Failed, 0 };
	return { IO::Status::Ok, 0 };
}

bool BufferedFileReader::OriginHasSize() const noexcept {
	return m_size.has_value();
}

std::optional<std::size_t> BufferedFileReader::OriginSize() const noexcept {
	return m_size;
}
