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

#include <StormByte/buffer/producer.hxx>

using namespace StormByte::Buffer;

Producer::~Producer() noexcept = default;
void Producer::Close() noexcept {
	m_buffer->Close();
}

void Producer::SetError() noexcept {
	m_buffer->SetError();
}

bool Producer::IsWritable() const noexcept {
	return m_buffer->IsWritable();
}

bool Producer::Write(const std::size_t& count, const DataType& data) noexcept {
	return m_buffer->Write(count, data);
}

bool Producer::Write(const std::size_t& count, DataType&& data) noexcept {
	return m_buffer->Write(count, std::move(data));
}

bool Producer::Write(const std::size_t& count, const ReadOnly& data) noexcept {
	return m_buffer->Write(count, data);
}

bool Producer::Write(const std::size_t& count, ReadOnly&& data) noexcept {
	return m_buffer->Write(count, std::move(data));
}