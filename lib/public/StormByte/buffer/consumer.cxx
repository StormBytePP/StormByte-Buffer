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

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/buffer/producer.hxx>

using namespace StormByte::Buffer;

Consumer::~Consumer() noexcept = default;

Producer Consumer::Producer() const noexcept {
	return StormByte::Buffer::Producer{m_buffer};
}

std::size_t Consumer::AvailableBytes() const noexcept {
	return m_buffer->AvailableBytes();
}

const DataType& Consumer::Data() const noexcept {
	return m_buffer->Data();
}

bool Consumer::Empty() const noexcept {
	return m_buffer->Empty();
}

bool Consumer::EoF() const noexcept {
	return m_buffer->EoF();
}

bool Consumer::IsReadable() const noexcept {
	return m_buffer->IsReadable();
}

std::size_t Consumer::Size() const noexcept {
	return m_buffer->Size();
}

void Consumer::Clean() noexcept {
	m_buffer->Clean();
}

void Consumer::Clear() noexcept {
	m_buffer->Clear();
}

bool Consumer::Drop(const std::size_t& count) noexcept {
	return m_buffer->Drop(count);
}

void Consumer::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	m_buffer->Seek(offset, mode);
}

bool Consumer::Extract(const std::size_t& count, DataType& out) noexcept {
	return m_buffer->Extract(count, out);
}

bool Consumer::Extract(const std::size_t& count, WriteOnly& out) noexcept {
	return m_buffer->Extract(count, out);
}

void Consumer::ExtractUntilEoF(DataType& out) noexcept {
	m_buffer->ExtractUntilEoF(out);
}

void Consumer::ExtractUntilEoF(WriteOnly& out) noexcept {
	m_buffer->ExtractUntilEoF(out);
}

bool Consumer::Read(const std::size_t& count, DataType& out) const noexcept {
	return m_buffer->Read(count, out);
}

bool Consumer::Read(const std::size_t& count, WriteOnly& out) const noexcept {
	return m_buffer->Read(count, out);
}

void Consumer::ReadUntilEoF(DataType& out) const noexcept {
	m_buffer->ReadUntilEoF(out);
}

void Consumer::ReadUntilEoF(WriteOnly& out) const noexcept {
	m_buffer->ReadUntilEoF(out);
}

bool Consumer::Peek(const std::size_t& count, DataType& out) const noexcept {
	return m_buffer->Peek(count, out);
}

bool Consumer::Peek(const std::size_t& count, WriteOnly& out) const noexcept {
	return m_buffer->Peek(count, out);
}
