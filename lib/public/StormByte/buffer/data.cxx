/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Buffer source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-Logger tree and
 * the rest of the StormByte suite it vendors), which remains under its own
 * license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/buffer/data.hxx>

#include <algorithm>
#include <utility>
#include <vector>

using namespace StormByte::Buffer;

class Data::Storage {
	public:
		std::vector<std::byte> bytes;
};

Data::Data() noexcept
	: m_storage(std::make_unique<Storage>()) {}

Data::Data(const StormByte::Size& count, std::byte value)
	: m_storage(std::make_unique<Storage>()) {
	m_storage->bytes.assign(static_cast<std::size_t>(count), value);
}

Data::Data(const StormByte::Size& count)
	: Data(count, std::byte{0}) {}

Data::Data(std::span<const std::byte> bytes)
	: Data(bytes.data(), StormByte::Size{bytes.size()}) {}

Data::Data(const std::byte* bytes, const StormByte::Size& count)
	: m_storage(std::make_unique<Storage>()) {
	if (count > StormByte::Size{0} && bytes != nullptr)
		m_storage->bytes.assign(bytes, bytes + static_cast<std::size_t>(count));
}

Data::Data(std::initializer_list<std::byte> list)
	: m_storage(std::make_unique<Storage>()) {
	m_storage->bytes.assign(list);
}

Data::Data(std::string_view sv)
	: m_storage(std::make_unique<Storage>()) {
	m_storage->bytes.reserve(sv.size());
	for (char c : sv)
		m_storage->bytes.push_back(static_cast<std::byte>(c));
}

Data::Data(const char* s)
	: Data(s ? std::string_view(s) : std::string_view{}) {}

Data::Data(const Data& other)
	: m_storage(std::make_unique<Storage>()) {
	m_storage->bytes = other.m_storage->bytes;
}

Data::Data(Data&& other) noexcept
	: m_storage(std::move(other.m_storage)) {
	other.m_storage = std::make_unique<Storage>();
}

Data::~Data() noexcept = default;

Data& Data::operator=(const Data& other) {
	if (this != &other)
		m_storage->bytes = other.m_storage->bytes;
	return *this;
}

Data& Data::operator=(Data&& other) noexcept {
	if (this != &other) {
		m_storage = std::move(other.m_storage);
		other.m_storage = std::make_unique<Storage>();
	}
	return *this;
}

Data& Data::operator=(std::initializer_list<std::byte> list) {
	assign(list);
	return *this;
}

bool Data::operator==(const Data& other) const noexcept {
	return m_storage->bytes == other.m_storage->bytes;
}

bool Data::operator!=(const Data& other) const noexcept {
	return !(*this == other);
}

std::strong_ordering Data::operator<=>(const Data& other) const noexcept {
	return std::lexicographical_compare_three_way(
		m_storage->bytes.begin(), m_storage->bytes.end(),
		other.m_storage->bytes.begin(), other.m_storage->bytes.end());
}

Data::iterator Data::begin() noexcept {
	return data();
}

Data::const_iterator Data::begin() const noexcept {
	return data();
}

Data::iterator Data::end() noexcept {
	return data() + static_cast<std::size_t>(size());
}

Data::const_iterator Data::end() const noexcept {
	return data() + static_cast<std::size_t>(size());
}

Data::const_iterator Data::cbegin() const noexcept {
	return begin();
}

Data::const_iterator Data::cend() const noexcept {
	return end();
}

Data::reverse_iterator Data::rbegin() noexcept {
	return reverse_iterator(end());
}

Data::reverse_iterator Data::rend() noexcept {
	return reverse_iterator(begin());
}

Data::const_reverse_iterator Data::rbegin() const noexcept {
	return const_reverse_iterator(end());
}

Data::const_reverse_iterator Data::rend() const noexcept {
	return const_reverse_iterator(begin());
}

Data::const_reverse_iterator Data::crbegin() const noexcept {
	return rbegin();
}

Data::const_reverse_iterator Data::crend() const noexcept {
	return rend();
}

StormByte::Size Data::size() const noexcept {
	return StormByte::Size{m_storage->bytes.size()};
}

StormByte::Size Data::max_size() const noexcept {
	return StormByte::Size{m_storage->bytes.max_size()};
}

StormByte::Size Data::capacity() const noexcept {
	return StormByte::Size{m_storage->bytes.capacity()};
}

bool Data::empty() const noexcept {
	return m_storage->bytes.empty();
}

void Data::reserve(const StormByte::Size& new_cap) {
	m_storage->bytes.reserve(static_cast<std::size_t>(new_cap));
}

void Data::resize(const StormByte::Size& new_size) {
	m_storage->bytes.resize(static_cast<std::size_t>(new_size));
}

void Data::resize(const StormByte::Size& new_size, std::byte value) {
	m_storage->bytes.resize(static_cast<std::size_t>(new_size), value);
}

void Data::shrink_to_fit() {
	m_storage->bytes.shrink_to_fit();
}

void Data::clear() noexcept {
	m_storage->bytes.clear();
}

std::byte& Data::operator[](const StormByte::Size& index) noexcept {
	return m_storage->bytes[static_cast<std::size_t>(index)];
}

const std::byte& Data::operator[](const StormByte::Size& index) const noexcept {
	return m_storage->bytes[static_cast<std::size_t>(index)];
}

std::byte& Data::at(const StormByte::Size& index) {
	if (index >= size())
		throw Exception("Data index out of range");
	return m_storage->bytes[static_cast<std::size_t>(index)];
}

const std::byte& Data::at(const StormByte::Size& index) const {
	if (index >= size())
		throw Exception("Data index out of range");
	return m_storage->bytes[static_cast<std::size_t>(index)];
}

std::byte& Data::front() noexcept {
	return m_storage->bytes.front();
}

const std::byte& Data::front() const noexcept {
	return m_storage->bytes.front();
}

std::byte& Data::back() noexcept {
	return m_storage->bytes.back();
}

const std::byte& Data::back() const noexcept {
	return m_storage->bytes.back();
}

std::byte* Data::data() noexcept {
	return m_storage->bytes.empty() ? nullptr : m_storage->bytes.data();
}

const std::byte* Data::data() const noexcept {
	return m_storage->bytes.empty() ? nullptr : m_storage->bytes.data();
}

std::span<std::byte> Data::span() noexcept {
	return std::span<std::byte>(data(), static_cast<std::size_t>(size()));
}

std::span<const std::byte> Data::span() const noexcept {
	return std::span<const std::byte>(data(), static_cast<std::size_t>(size()));
}

Data::operator std::span<std::byte>() noexcept {
	return span();
}

Data::operator std::span<const std::byte>() const noexcept {
	return span();
}

void Data::push_back(std::byte value) {
	m_storage->bytes.push_back(value);
}

void Data::pop_back() {
	m_storage->bytes.pop_back();
}

Data::iterator Data::insert(const_iterator pos, std::byte value) {
	return insert(pos, StormByte::Size{1}, value);
}

Data::iterator Data::insert(const_iterator pos, const StormByte::Size& count, std::byte value) {
	const StormByte::Size index = offset_of(pos);
	if (count == StormByte::Size{0})
		return begin() + static_cast<std::ptrdiff_t>(index);
	m_storage->bytes.insert(m_storage->bytes.begin() + static_cast<std::ptrdiff_t>(index),
		static_cast<std::size_t>(count), value);
	return begin() + static_cast<std::ptrdiff_t>(index);
}

Data::iterator Data::insert(const_iterator pos, std::initializer_list<std::byte> list) {
	return insert(pos, std::span<const std::byte>(list.begin(), list.size()));
}

Data::iterator Data::insert(const_iterator pos, std::span<const std::byte> bytes) {
	return insert_at(offset_of(pos), bytes.data(), StormByte::Size{bytes.size()});
}

Data::iterator Data::erase(const_iterator pos) {
	const StormByte::Size index = offset_of(pos);
	m_storage->bytes.erase(m_storage->bytes.begin() + static_cast<std::ptrdiff_t>(index));
	return begin() + static_cast<std::ptrdiff_t>(index);
}

Data::iterator Data::erase(const_iterator first, const_iterator last) {
	const StormByte::Size index = offset_of(first);
	const StormByte::Size last_index = offset_of(last);
	m_storage->bytes.erase(
		m_storage->bytes.begin() + static_cast<std::ptrdiff_t>(index),
		m_storage->bytes.begin() + static_cast<std::ptrdiff_t>(last_index));
	return begin() + static_cast<std::ptrdiff_t>(index);
}

void Data::assign(const StormByte::Size& count, std::byte value) {
	m_storage->bytes.assign(static_cast<std::size_t>(count), value);
}

void Data::assign(std::initializer_list<std::byte> list) {
	m_storage->bytes.assign(list);
}

void Data::assign(std::span<const std::byte> bytes) {
	clear();
	append(bytes);
}

void Data::append(std::span<const std::byte> bytes) {
	append(bytes.data(), StormByte::Size{bytes.size()});
}

void Data::append(const std::byte* bytes, const StormByte::Size& count) {
	if (count == StormByte::Size{0} || bytes == nullptr)
		return;
	m_storage->bytes.insert(m_storage->bytes.end(), bytes, bytes + static_cast<std::size_t>(count));
}

void Data::append(const Data& other) {
	append(other.span());
}

void Data::append(Data&& other) {
	if (this == &other)
		return;
	if (empty()) {
		m_storage.swap(other.m_storage);
		other.m_storage = std::make_unique<Storage>();
		return;
	}
	append(other.span());
	other.clear();
}

void Data::swap(Data& other) noexcept {
	m_storage.swap(other.m_storage);
}

StormByte::Size Data::offset_of(const_iterator pos) const noexcept {
	if (data() == nullptr)
		return StormByte::Size{0};
	const auto distance = pos - data();
	if (distance <= 0)
		return StormByte::Size{0};
	const auto cap = static_cast<std::ptrdiff_t>(size());
	if (distance >= cap)
		return size();
	return StormByte::Size{static_cast<std::size_t>(distance)};
}

Data::iterator Data::insert_at(const StormByte::Size& index, const std::byte* bytes, const StormByte::Size& count) {
	if (count == StormByte::Size{0} || bytes == nullptr)
		return begin() + static_cast<std::ptrdiff_t>(index);
	m_storage->bytes.insert(m_storage->bytes.begin() + static_cast<std::ptrdiff_t>(index),
		bytes, bytes + static_cast<std::size_t>(count));
	return begin() + static_cast<std::ptrdiff_t>(index);
}

namespace StormByte::Buffer {
	void swap(Data& lhs, Data& rhs) noexcept {
		lhs.swap(rhs);
	}
}
