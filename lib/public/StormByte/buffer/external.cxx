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

ExternalReader::~ExternalReader() noexcept = default;

void ExternalReader::Seek(const std::ptrdiff_t offset, const Position mode) const noexcept {
	(void)offset;
	(void)mode;
}

void ExternalReader::Clean() noexcept {}

ExternalBufferReader::~ExternalBufferReader() noexcept = default;

ReadOnly& ExternalBufferReader::Store() noexcept {
	if (auto* handle = std::get_if<Consumer>(&m_store))
		return *handle;
	return std::get<std::reference_wrapper<ReadOnly>>(m_store).get();
}

const ReadOnly& ExternalBufferReader::Store() const noexcept {
	if (const auto* handle = std::get_if<Consumer>(&m_store))
		return *handle;
	return std::get<std::reference_wrapper<ReadOnly>>(m_store).get();
}

ExternalReader::PointerType ExternalBufferReader::Clone() const noexcept {
	return MakePointer<ExternalBufferReader>(*this);
}

ExternalReader::PointerType ExternalBufferReader::Move() noexcept {
	return MakePointer<ExternalBufferReader>(std::move(*this));
}

std::size_t ExternalBufferReader::AvailableBytes() const noexcept {
	return Store().AvailableBytes();
}

bool ExternalBufferReader::Empty() const noexcept {
	return Store().Empty();
}

bool ExternalBufferReader::EoF() const noexcept {
	return Store().EoF();
}

bool ExternalBufferReader::IsReadable() const noexcept {
	return Store().IsReadable();
}

bool ExternalBufferReader::Read(const std::size_t count, DataType& out) const noexcept {
	return Store().Read(count, out);
}

bool ExternalBufferReader::Extract(const std::size_t count, DataType& out) noexcept {
	return Store().Extract(count, out);
}

bool ExternalBufferReader::Peek(const std::size_t count, DataType& out) const noexcept {
	return Store().Peek(count, out);
}

void ExternalBufferReader::ReadUntilEoF(DataType& out) const noexcept {
	Store().ReadUntilEoF(out);
}

void ExternalBufferReader::ExtractUntilEoF(DataType& out) noexcept {
	Store().ExtractUntilEoF(out);
}

void ExternalBufferReader::Seek(const std::ptrdiff_t offset, const Position mode) const noexcept {
	Store().Seek(offset, mode);
}

void ExternalBufferReader::Clean() noexcept {
	Store().Clean();
}

ExternalWriter::~ExternalWriter() noexcept = default;

ExternalBufferWriter::~ExternalBufferWriter() noexcept = default;

WriteOnly& ExternalBufferWriter::Store() noexcept {
	if (auto* handle = std::get_if<Producer>(&m_store))
		return *handle;
	return std::get<std::reference_wrapper<WriteOnly>>(m_store).get();
}

const WriteOnly& ExternalBufferWriter::Store() const noexcept {
	if (const auto* handle = std::get_if<Producer>(&m_store))
		return *handle;
	return std::get<std::reference_wrapper<WriteOnly>>(m_store).get();
}

ExternalWriter::PointerType ExternalBufferWriter::Clone() const noexcept {
	return MakePointer<ExternalBufferWriter>(*this);
}

ExternalWriter::PointerType ExternalBufferWriter::Move() noexcept {
	return MakePointer<ExternalBufferWriter>(std::move(*this));
}

bool ExternalWriter::Write(const std::string_view sv) noexcept {
	if (sv.empty())
		return Write(DataType{});
	DataType tmp;
	tmp.reserve(sv.size());
	std::transform(sv.begin(), sv.end(), std::back_inserter(tmp),
		[](char c) noexcept { return static_cast<std::byte>(c); });
	return Write(std::move(tmp));
}

bool ExternalWriter::Write(const char* s) noexcept {
	if (!s)
		return Write(DataType{});
	return Write(std::string_view(s));
}

bool ExternalWriter::Write(const std::size_t count, const std::string_view sv) noexcept {
	const std::size_t to_write = (count == 0)
		? sv.size()
		: std::min(count, static_cast<std::size_t>(sv.size()));
	DataType tmp;
	if (to_write > 0)
		tmp.reserve(to_write);
	std::transform(sv.begin(), sv.begin() + static_cast<std::ptrdiff_t>(to_write),
		std::back_inserter(tmp),
		[](char c) noexcept { return static_cast<std::byte>(c); });
	return Write(std::move(tmp));
}

bool ExternalBufferWriter::IsWritable() const noexcept {
	return Store().IsWritable();
}

std::size_t ExternalBufferWriter::Occupied() const noexcept {
	return Store().Size();
}

bool ExternalBufferWriter::Write(const DataType& data) noexcept {
	return Store().Write(0, data);
}

bool ExternalBufferWriter::Write(DataType&& data) noexcept {
	return Store().Write(0, std::move(data));
}

bool ExternalBufferWriter::Write(const std::size_t count, const DataType& data) noexcept {
	return Store().Write(count, data);
}

bool ExternalBufferWriter::Write(const std::size_t count, DataType&& data) noexcept {
	return Store().Write(count, std::move(data));
}

void ExternalBufferWriter::Close() noexcept {
	Store().Close();
}

void ExternalBufferWriter::SetError() noexcept {
	Store().SetError();
}
