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

#include <StormByte/buffer/buffered_read.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_read.hxx>

using namespace StormByte::Buffer;

BufferedRead::BufferedRead(const std::size_t read_ahead, const std::size_t max_memory):
	m_io(std::make_unique<IO::BufferedRead>(*this, read_ahead, max_memory)) {}

BufferedRead::BufferedRead(BufferedRead&& other) noexcept:
	m_io(std::move(other.m_io)) {
	if (m_io)
		m_io->Rebind(*this);
}

BufferedRead::~BufferedRead() noexcept {
	if (m_io)
		m_io->Shutdown();
}

BufferedRead& BufferedRead::operator=(BufferedRead&& other) noexcept {
	if (this != &other) {
		if (m_io)
			static_cast<void>(Close());
		m_io = std::move(other.m_io);
		if (m_io)
			m_io->Rebind(*this);
	}
	return *this;
}

BufferedRead::operator bool() const noexcept {
	return m_io && static_cast<bool>(*m_io);
}

IO::Result BufferedRead::Open() {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Open();
}

IO::Result BufferedRead::Close() {
	if (!m_io)
		return { IO::Status::Ok, 0 };
	return m_io->Close();
}

IO::Result BufferedRead::Rewind() {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Rewind();
}

bool BufferedRead::IsOpen() const noexcept {
	return m_io && m_io->IsOpen();
}

bool BufferedRead::IsReadable() const noexcept {
	return m_io && m_io->IsReadable();
}

bool BufferedRead::EoF() const noexcept {
	return !m_io || m_io->EoF();
}

IO::Result BufferedRead::Read(const std::size_t n, FIFO& dest) const {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Read(n, dest);
}

IO::Result BufferedRead::Peek(const std::size_t n, FIFO& dest) const {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Peek(n, dest);
}

IO::Result BufferedRead::Seek(const std::ptrdiff_t offset, const Position mode) const {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Seek(offset, mode);
}

std::size_t BufferedRead::Tell() const noexcept {
	return m_io ? m_io->Tell() : 0;
}

bool BufferedRead::IsSeekable() const noexcept {
	return m_io && m_io->IsSeekable();
}

bool BufferedRead::IsSized() const noexcept {
	return m_io && m_io->IsSized();
}

std::optional<std::size_t> BufferedRead::Size() const noexcept {
	if (!m_io)
		return std::nullopt;
	return m_io->Size();
}

std::size_t BufferedRead::ReadAhead() const noexcept {
	return m_io ? m_io->ReadAhead() : 0;
}

void BufferedRead::ReadAhead(const std::size_t bytes) {
	if (m_io)
		m_io->ReadAhead(bytes);
}

std::size_t BufferedRead::MaxMemory() const noexcept {
	return m_io ? m_io->MaxMemory() : 0;
}

void BufferedRead::MaxMemory(const std::size_t bytes) {
	if (m_io)
		m_io->MaxMemory(bytes);
}
