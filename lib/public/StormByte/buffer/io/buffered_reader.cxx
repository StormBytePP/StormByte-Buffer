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
#include <StormByte/buffer/io/buffered_reader.hxx>

#include <algorithm>

using namespace StormByte::Buffer::IO;

namespace {
	StormByte::Buffer::IO::Result CopyToSpan(StormByte::Buffer::FIFO& src,
			const StormByte::Buffer::IO::Result got,
			std::span<std::byte> dest) {
		if (got.count == 0)
			return got;
		if (got.count > dest.size())
			return { StormByte::Buffer::IO::Status::Failed, 0 };

		StormByte::Buffer::DataType raw;
		if (!src.Extract(got.count, raw))
			return { StormByte::Buffer::IO::Status::Failed, 0 };
		std::copy_n(raw.begin(), got.count, dest.begin());
		return got;
	}
}

BufferedReader::BufferedReader(const std::size_t read_ahead, const std::size_t max_memory,
		const std::chrono::milliseconds max_wait):
	m_io(std::make_unique<Backend::BufferedReader>(*this, read_ahead, max_memory, max_wait)) {}

BufferedReader::BufferedReader(BufferedReader&& other) noexcept:
	m_io(std::move(other.m_io)) {
	if (m_io) {
		m_io->FlushPrefetch();
		m_io->Rebind(*this);
	}
}

BufferedReader::~BufferedReader() noexcept {
	if (m_io)
		m_io->Shutdown();
}

BufferedReader& BufferedReader::operator=(BufferedReader&& other) noexcept {
	if (this != &other) {
		if (m_io)
			static_cast<void>(Close());
		m_io = std::move(other.m_io);
		if (m_io) {
			m_io->FlushPrefetch();
			m_io->Rebind(*this);
		}
	}
	return *this;
}

BufferedReader::operator bool() const noexcept {
	return m_io && static_cast<bool>(*m_io);
}

State BufferedReader::State() const noexcept {
	return m_io ? m_io->State() : State::Unavailable;
}

void BufferedReader::SetState(const enum State state) noexcept {
	if (m_io)
		m_io->SetState(state);
}

void BufferedReader::Setup() {}

bool BufferedReader::Open() {
	if (!m_io)
		return false;
	Setup();
	return m_io->Open();
}

Result BufferedReader::Close() {
	if (!m_io)
		return { IO::Status::Ok, 0 };
	return m_io->Close();
}

bool BufferedReader::Rewind() {
	if (!m_io)
		return false;
	return m_io->Rewind();
}

bool BufferedReader::IsOpen() const noexcept {
	return m_io && m_io->IsOpen();
}

bool BufferedReader::IsReadable() const noexcept {
	return m_io && m_io->IsReadable();
}

bool BufferedReader::EoF() const noexcept {
	return !m_io || m_io->EoF();
}

Result BufferedReader::Read(const std::size_t n, FIFO& dest) const {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Read(n, dest);
}

Result BufferedReader::Read(const std::span<std::byte> dest) const {
	if (dest.empty())
		return { IO::Status::Ok, 0 };
	FIFO fifo;
	const Result got = Read(dest.size(), fifo);
	return CopyToSpan(fifo, got, dest);
}

Result BufferedReader::Peek(const std::size_t n, FIFO& dest) const {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Peek(n, dest);
}

Result BufferedReader::Peek(const std::span<std::byte> dest) const {
	if (dest.empty())
		return { IO::Status::Ok, 0 };
	FIFO fifo;
	const Result got = Peek(dest.size(), fifo);
	return CopyToSpan(fifo, got, dest);
}

Result BufferedReader::Seek(const std::ptrdiff_t offset, const Position mode) const {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Seek(offset, mode);
}

std::size_t BufferedReader::Tell() const noexcept {
	return m_io ? m_io->Tell() : 0;
}

bool BufferedReader::IsSeekable() const noexcept {
	return m_io && m_io->IsSeekable();
}

bool BufferedReader::IsSized() const noexcept {
	return m_io && m_io->IsSized();
}

std::optional<std::size_t> BufferedReader::Size() const noexcept {
	if (!m_io)
		return std::nullopt;
	return m_io->Size();
}

std::size_t BufferedReader::ReadAhead() const noexcept {
	return m_io ? m_io->ReadAhead() : 0;
}

void BufferedReader::ReadAhead(const std::size_t bytes) {
	if (m_io)
		m_io->ReadAhead(bytes);
}

std::size_t BufferedReader::MaxMemory() const noexcept {
	return m_io ? m_io->MaxMemory() : 0;
}

void BufferedReader::MaxMemory(const std::size_t bytes) {
	if (m_io)
		m_io->MaxMemory(bytes);
}

std::chrono::milliseconds BufferedReader::MaxWait() const noexcept {
	return m_io ? m_io->MaxWait() : std::chrono::milliseconds{0};
}

void BufferedReader::MaxWait(const std::chrono::milliseconds wait) {
	if (m_io)
		m_io->MaxWait(wait);
}
