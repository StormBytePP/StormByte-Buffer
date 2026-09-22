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

#include <StormByte/buffer/io/backend/buffered_writer.hxx>
#include <StormByte/buffer/io/buffered_writer.hxx>

using namespace StormByte::Buffer::IO;

BufferedWriter::BufferedWriter(const std::size_t write_chunk, const std::size_t back_pressure,
		const std::chrono::milliseconds max_wait):
	m_io(std::make_unique<Backend::BufferedWriter>(*this, write_chunk, back_pressure, max_wait)) {}

BufferedWriter::BufferedWriter(BufferedWriter&& other) noexcept:
	m_io(std::move(other.m_io)) {
	if (m_io)
		m_io->Rebind(*this);
}

BufferedWriter::~BufferedWriter() noexcept {
	if (m_io)
		m_io->Shutdown();
}

BufferedWriter& BufferedWriter::operator=(BufferedWriter&& other) noexcept {
	if (this != &other) {
		if (m_io)
			static_cast<void>(Close());
		m_io = std::move(other.m_io);
		if (m_io)
			m_io->Rebind(*this);
	}
	return *this;
}

BufferedWriter::operator bool() const noexcept {
	return m_io && static_cast<bool>(*m_io);
}

enum State BufferedWriter::State() const noexcept {
	return m_io ? m_io->State() : State::Unavailable;
}

void BufferedWriter::SetState(const enum State state) noexcept {
	if (m_io)
		m_io->SetState(state);
}

void BufferedWriter::Setup() {}

bool BufferedWriter::Open() {
	if (!m_io)
		return false;
	Setup();
	return m_io->Open();
}

bool BufferedWriter::Close() {
	if (!m_io)
		return true;
	return m_io->Close();
}

bool BufferedWriter::Rewind() {
	if (!m_io)
		return false;
	return m_io->Rewind();
}

bool BufferedWriter::IsOpen() const noexcept {
	return m_io && m_io->IsOpen();
}

Result BufferedWriter::Flush() {
	if (!m_io)
		return { Status::Failed, 0 };
	return m_io->Flush();
}

Result BufferedWriter::Truncate() {
	if (!m_io)
		return { Status::Failed, 0 };
	return m_io->Truncate();
}

Result BufferedWriter::Write(const FIFO& src) {
	if (!m_io)
		return { Status::Failed, 0 };
	return m_io->Write(src);
}

Result BufferedWriter::Write(FIFO& src) {
	if (!m_io)
		return { Status::Failed, 0 };
	return m_io->Write(src);
}

Result BufferedWriter::Write(const std::span<const std::byte> src) {
	if (!m_io)
		return { Status::Failed, 0 };
	return m_io->Write(src);
}

std::size_t BufferedWriter::Tell() const noexcept {
	return m_io ? m_io->Tell() : 0;
}

std::size_t BufferedWriter::Dirty() const noexcept {
	return m_io ? m_io->Dirty() : 0;
}

std::size_t BufferedWriter::WriteChunk() const noexcept {
	return m_io ? m_io->WriteChunk() : 0;
}

void BufferedWriter::WriteChunk(const std::size_t bytes) {
	if (m_io)
		m_io->WriteChunk(bytes);
}

std::size_t BufferedWriter::BackPressure() const noexcept {
	return m_io ? m_io->BackPressure() : 0;
}

void BufferedWriter::BackPressure(const std::size_t chunks) {
	if (m_io)
		m_io->BackPressure(chunks);
}

std::chrono::milliseconds BufferedWriter::MaxWait() const noexcept {
	return m_io ? m_io->MaxWait() : std::chrono::milliseconds{0};
}

void BufferedWriter::MaxWait(const std::chrono::milliseconds wait) {
	if (m_io)
		m_io->MaxWait(wait);
}

bool BufferedWriter::WillWrite(const std::size_t n) const {
	return m_io && m_io->WillWrite(n);
}
