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

#include <StormByte/buffer/generic.hxx>

using namespace StormByte::Buffer;

Generic::~Generic() noexcept = default;

ReadOnly::~ReadOnly() noexcept = default;

WriteOnly::~WriteOnly() noexcept = default;

ReadWrite::~ReadWrite() noexcept = default;

template DataType Generic::DataConvert<DataType>(const DataType&) noexcept;
template DataType Generic::DataConvert<DataType>(DataType&&) noexcept;
template DataType Generic::DataConvert<std::span<const std::byte>>(const std::span<const std::byte>&) noexcept;
template DataType Generic::DataConvert<std::span<std::byte>>(const std::span<std::byte>&) noexcept;
template DataType Generic::DataConvert<std::span<std::byte>>(std::span<std::byte>&&) noexcept;

template bool WriteOnly::Write<DataType>(const DataType&) noexcept;
template bool WriteOnly::Write<DataType>(DataType&&) noexcept;
template bool WriteOnly::Write<DataType>(const std::size_t&, const DataType&) noexcept;
template bool WriteOnly::Write<DataType>(const std::size_t&, DataType&&) noexcept;
template bool WriteOnly::Write<std::span<const std::byte>>(const std::span<const std::byte>&) noexcept;
template bool WriteOnly::Write<std::span<const std::byte>>(const std::size_t&, const std::span<const std::byte>&) noexcept;
template bool WriteOnly::Write<std::span<std::byte>>(const std::span<std::byte>&) noexcept;
template bool WriteOnly::Write<std::span<std::byte>>(const std::size_t&, const std::span<std::byte>&) noexcept;
template bool WriteOnly::Write<DataType::const_iterator, DataType::const_iterator>(DataType::const_iterator, DataType::const_iterator) noexcept;
template bool WriteOnly::Write<DataType::iterator, DataType::iterator>(DataType::iterator, DataType::iterator) noexcept;
template bool WriteOnly::Write<DataType::const_iterator, DataType::const_iterator>(const std::size_t&, DataType::const_iterator, DataType::const_iterator) noexcept;
template bool WriteOnly::Write<DataType::iterator, DataType::iterator>(const std::size_t&, DataType::iterator, DataType::iterator) noexcept;
