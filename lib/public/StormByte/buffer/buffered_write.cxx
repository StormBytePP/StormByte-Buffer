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

#include <StormByte/buffer/buffered_write.hxx>
#include <StormByte/buffer/io/buffered_write.hxx>

using namespace StormByte::Buffer;

BufferedWrite::BufferedWrite(const std::size_t write_chunk, const std::size_t back_pressure,
		const std::chrono::milliseconds max_wait):
	m_io(std::make_unique<IO::BufferedWrite>(*this, write_chunk, back_pressure, max_wait)) {}

BufferedWrite::BufferedWrite(BufferedWrite&& other) noexcept:
	m_io(std::move(other.m_io)) {
	if (m_io)
		m_io->Rebind(*this);
}

BufferedWrite::~BufferedWrite() noexcept {
	if (m_io)
		m_io->Shutdown();
}

BufferedWrite& BufferedWrite::operator=(BufferedWrite&& other) noexcept {
	if (this != &other) {
		if (m_io)
			static_cast<void>(Close());
		m_io = std::move(other.m_io);
		if (m_io)
			m_io->Rebind(*this);
	}
	return *this;
}

BufferedWrite::operator bool() const noexcept {
	return m_io && static_cast<bool>(*m_io);
}

IO::State BufferedWrite::State() const noexcept {
	return m_io ? m_io->State() : IO::State::Unavailable;
}

void BufferedWrite::SetState(const IO::State state) noexcept {
	if (m_io)
		m_io->SetState(state);
}

bool BufferedWrite::Open() {
	if (!m_io)
		return false;
	return m_io->Open();
}

bool BufferedWrite::Close() {
	if (!m_io)
		return true;
	return m_io->Close();
}

bool BufferedWrite::Rewind() {
	if (!m_io)
		return false;
	return m_io->Rewind();
}

bool BufferedWrite::IsOpen() const noexcept {
	return m_io && m_io->IsOpen();
}

IO::Result BufferedWrite::Flush() {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Flush();
}

IO::Result BufferedWrite::Truncate() {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Truncate();
}

IO::Result BufferedWrite::Write(const FIFO& src) {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Write(src);
}

IO::Result BufferedWrite::Write(FIFO& src) {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Write(src);
}

IO::Result BufferedWrite::Write(const std::span<const std::byte> src) {
	if (!m_io)
		return { IO::Status::Failed, 0 };
	return m_io->Write(src);
}

std::size_t BufferedWrite::Tell() const noexcept {
	return m_io ? m_io->Tell() : 0;
}

std::size_t BufferedWrite::Dirty() const noexcept {
	return m_io ? m_io->Dirty() : 0;
}

std::size_t BufferedWrite::WriteChunk() const noexcept {
	return m_io ? m_io->WriteChunk() : 0;
}

void BufferedWrite::WriteChunk(const std::size_t bytes) {
	if (m_io)
		m_io->WriteChunk(bytes);
}

std::size_t BufferedWrite::BackPressure() const noexcept {
	return m_io ? m_io->BackPressure() : 0;
}

void BufferedWrite::BackPressure(const std::size_t chunks) {
	if (m_io)
		m_io->BackPressure(chunks);
}

std::chrono::milliseconds BufferedWrite::MaxWait() const noexcept {
	return m_io ? m_io->MaxWait() : std::chrono::milliseconds{0};
}

void BufferedWrite::MaxWait(const std::chrono::milliseconds wait) {
	if (m_io)
		m_io->MaxWait(wait);
}
