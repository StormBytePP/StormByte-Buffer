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

#include <StormByte/buffer/buffered_file_writer.hxx>

#include <ios>
#include <system_error>
#include <utility>

using namespace StormByte::Buffer;

BufferedFileWriter::BufferedFileWriter(std::filesystem::path path, const std::size_t write_chunk,
		const std::size_t back_pressure, const std::chrono::milliseconds max_wait):
	BufferedWrite(write_chunk, back_pressure, max_wait),
	m_path(std::move(path)) {}

BufferedFileWriter::BufferedFileWriter(BufferedFileWriter&& other) noexcept:
	BufferedWrite(std::move(other)),
	m_path(std::move(other.m_path)),
	m_file(std::move(other.m_file)) {}

BufferedFileWriter::~BufferedFileWriter() noexcept {
	static_cast<void>(Close());
}

BufferedFileWriter& BufferedFileWriter::operator=(BufferedFileWriter&& other) noexcept {
	if (this != &other) {
		static_cast<void>(Close());
		BufferedWrite::operator=(std::move(other));
		m_path = std::move(other.m_path);
		m_file = std::move(other.m_file);
	}
	return *this;
}

const std::filesystem::path& BufferedFileWriter::Path() const noexcept {
	return m_path;
}

IO::Result BufferedFileWriter::OriginOpen() {
	std::lock_guard lock(m_file_mutex);
	if (m_file.is_open())
		return { IO::Status::Failed, 0 };

	std::error_code ec;
	const auto parent = m_path.parent_path();
	if (!parent.empty()) {
		const auto pst = std::filesystem::status(parent, ec);
		if (ec) {
			SetState(IO::State::Missing);
			return { IO::Status::Failed, 0 };
		}
		if (!std::filesystem::is_directory(pst)) {
			SetState(IO::State::Missing);
			return { IO::Status::Failed, 0 };
		}
	}

	const auto st = std::filesystem::status(m_path, ec);
	if (ec && ec != std::errc::no_such_file_or_directory) {
		SetState(IO::State::NotWritable);
		return { IO::Status::Failed, 0 };
	}

	if (!ec) {
		if (std::filesystem::is_directory(st)) {
			SetState(IO::State::Directory);
			return { IO::Status::Failed, 0 };
		}
		if (!std::filesystem::is_regular_file(st)) {
			SetState(IO::State::NotWritable);
			return { IO::Status::Failed, 0 };
		}
	}

	m_file.open(m_path, std::ios::out | std::ios::app | std::ios::binary);
	if (!m_file) {
		SetState(IO::State::NotWritable);
		return { IO::Status::Failed, 0 };
	}

	SetState(IO::State::Idle);
	return { IO::Status::Ok, 0 };
}

IO::Result BufferedFileWriter::OriginClose() {
	std::lock_guard lock(m_file_mutex);
	if (m_file.is_open())
		m_file.close();
	SetState(IO::State::Unavailable);
	return { IO::Status::Ok, 0 };
}

IO::Result BufferedFileWriter::OriginPush(const std::span<const std::byte> data) {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { IO::Status::Failed, 0 };
	if (data.empty())
		return { IO::Status::Ok, 0 };

	m_file.write(reinterpret_cast<const char*>(data.data()),
		static_cast<std::streamsize>(data.size()));
	if (m_file.bad()) {
		SetState(IO::State::Fault);
		return { IO::Status::Error, 0 };
	}

	return { IO::Status::Ok, data.size() };
}

IO::Result BufferedFileWriter::OriginFlush() {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { IO::Status::Failed, 0 };
	m_file.flush();
	if (m_file.bad()) {
		SetState(IO::State::Fault);
		return { IO::Status::Error, 0 };
	}
	return { IO::Status::Ok, 0 };
}

IO::Result BufferedFileWriter::OriginTruncate() {
	std::lock_guard lock(m_file_mutex);
	if (!m_file.is_open())
		return { IO::Status::Failed, 0 };

	m_file.close();
	std::error_code ec;
	std::filesystem::resize_file(m_path, 0, ec);
	if (ec) {
		SetState(IO::State::Fault);
		return { IO::Status::Failed, 0 };
	}

	m_file.open(m_path, std::ios::out | std::ios::app | std::ios::binary);
	if (!m_file) {
		SetState(IO::State::NotWritable);
		return { IO::Status::Failed, 0 };
	}
	return { IO::Status::Ok, 0 };
}
