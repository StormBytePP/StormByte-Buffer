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

#include <StormByte/buffer/external.hxx>
#include <algorithm>
#include <iterator>
using namespace StormByte::Buffer;
// ---------------------------------------------------------------------------
// ExternalBufferReader
// ---------------------------------------------------------------------------
std::size_t ExternalBufferReader::AvailableBytes() const noexcept {
	return m_buffer.get().AvailableBytes();
}
bool ExternalBufferReader::Empty() const noexcept {
	return m_buffer.get().Empty();
}
bool ExternalBufferReader::EoF() const noexcept {
	return m_buffer.get().EoF();
}
bool ExternalBufferReader::IsReadable() const noexcept {
	return m_buffer.get().IsReadable();
}
bool ExternalBufferReader::Read(std::size_t count, DataType& out) const noexcept {
	return m_buffer.get().Read(count, out);
}
bool ExternalBufferReader::Extract(std::size_t count, DataType& out) noexcept {
	return m_buffer.get().Extract(count, out);
}
bool ExternalBufferReader::Peek(std::size_t count, DataType& out) const noexcept {
	return m_buffer.get().Peek(count, out);
}
void ExternalBufferReader::ReadUntilEoF(DataType& out) const noexcept {
	m_buffer.get().ReadUntilEoF(out);
}
void ExternalBufferReader::ExtractUntilEoF(DataType& out) noexcept {
	m_buffer.get().ExtractUntilEoF(out);
}
void ExternalBufferReader::Seek(std::ptrdiff_t offset, Position mode) const noexcept {
	m_buffer.get().Seek(offset, mode);
}
void ExternalBufferReader::Clean() noexcept {
	m_buffer.get().Clean();
}
// ---------------------------------------------------------------------------
// ExternalWriter convenience
// ---------------------------------------------------------------------------
bool ExternalWriter::Write(std::string_view sv) noexcept {
	if (sv.empty()) return Write(DataType{});
	DataType tmp;
	tmp.reserve(sv.size());
	std::transform(sv.begin(), sv.end(), std::back_inserter(tmp),
		[](char c) noexcept { return static_cast<std::byte>(c); });
	return Write(std::move(tmp));
}
bool ExternalWriter::Write(const char* s) noexcept {
	if (!s) return Write(DataType{});
	return Write(std::string_view(s));
}
bool ExternalWriter::Write(std::size_t count, std::string_view sv) noexcept {
	const std::size_t to_write = (count == 0)
		? sv.size()
		: std::min(count, static_cast<std::size_t>(sv.size()));
	DataType tmp;
	if (to_write > 0) tmp.reserve(to_write);
	std::transform(sv.begin(), sv.begin() + static_cast<std::ptrdiff_t>(to_write),
		std::back_inserter(tmp),
		[](char c) noexcept { return static_cast<std::byte>(c); });
	return Write(std::move(tmp));
}
// ---------------------------------------------------------------------------
// ExternalBufferWriter
// ---------------------------------------------------------------------------
bool ExternalBufferWriter::IsWritable() const noexcept {
	return m_buffer.get().IsWritable();
}
bool ExternalBufferWriter::Write(const DataType& data) noexcept {
	return m_buffer.get().Write(0, data);
}
bool ExternalBufferWriter::Write(DataType&& data) noexcept {
	return m_buffer.get().Write(0, std::move(data));
}
bool ExternalBufferWriter::Write(std::size_t count, const DataType& data) noexcept {
	return m_buffer.get().Write(count, data);
}
bool ExternalBufferWriter::Write(std::size_t count, DataType&& data) noexcept {
	return m_buffer.get().Write(count, std::move(data));
}
void ExternalBufferWriter::Close() noexcept {
	m_buffer.get().Close();
}
void ExternalBufferWriter::SetError() noexcept {
	m_buffer.get().SetError();
}
