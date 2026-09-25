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
#include <StormByte/buffer/exception.hxx>
#include <StormByte/size.hxx>
#include <StormByte/test_handlers.h>
#include <StormByte/type_traits.hxx>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

using StormByte::Size;
using StormByte::Buffer::Data;
using StormByte::Buffer::Exception;

namespace {
	Data Bytes(std::initializer_list<unsigned char> list) {
		Data data;
		data.reserve(Size{list.size()});
		for (unsigned char value : list)
			data.push_back(static_cast<std::byte>(value));
		return data;
	}

	bool SameBytes(const Data& data, std::initializer_list<unsigned char> list) {
		if (data.size() != Size{list.size()})
			return false;
		auto it = data.begin();
		for (unsigned char value : list) {
			if (*it != static_cast<std::byte>(value))
				return false;
			++it;
		}
		return true;
	}
}

// -------------------
// Algorithms
// -------------------

int test_data_algorithm_copy() {
	Data source = Bytes({1, 2, 3, 4});
	Data dest(Size{4}, std::byte{0});
	std::copy(source.begin(), source.end(), dest.begin());
	ASSERT_TRUE("test_data_algorithm_copy", source == dest);
	RETURN_TEST("test_data_algorithm_copy", 0);
}

int test_data_algorithm_count() {
	Data data = Bytes({1, 2, 1, 1, 3});
	const auto found = std::count(data.begin(), data.end(), std::byte{1});
	ASSERT_EQUAL("test_data_algorithm_count", 3, found);
	RETURN_TEST("test_data_algorithm_count", 0);
}

int test_data_algorithm_equal() {
	Data a = Bytes({9, 8, 7});
	Data b = Bytes({9, 8, 7});
	ASSERT_TRUE("test_data_algorithm_equal", std::equal(a.begin(), a.end(), b.begin(), b.end()));
	RETURN_TEST("test_data_algorithm_equal", 0);
}

int test_data_algorithm_fill() {
	Data data(Size{4}, std::byte{0});
	std::fill(data.begin(), data.end(), std::byte{0xAB});
	ASSERT_TRUE("test_data_algorithm_fill", SameBytes(data, {0xAB, 0xAB, 0xAB, 0xAB}));
	RETURN_TEST("test_data_algorithm_fill", 0);
}

int test_data_algorithm_find() {
	Data data = Bytes({10, 20, 30, 40});
	auto it = std::find(data.begin(), data.end(), std::byte{30});
	ASSERT_TRUE("test_data_algorithm_find", it != data.end());
	ASSERT_TRUE("test_data_algorithm_find", *it == std::byte{30});
	ASSERT_EQUAL("test_data_algorithm_find", 2, it - data.begin());
	RETURN_TEST("test_data_algorithm_find", 0);
}

int test_data_algorithm_ranges_sort() {
	Data data = Bytes({4, 1, 3, 2});
	std::ranges::sort(data);
	ASSERT_TRUE("test_data_algorithm_ranges_sort", SameBytes(data, {1, 2, 3, 4}));
	RETURN_TEST("test_data_algorithm_ranges_sort", 0);
}

int test_data_algorithm_reverse() {
	Data data = Bytes({1, 2, 3, 4});
	std::reverse(data.begin(), data.end());
	ASSERT_TRUE("test_data_algorithm_reverse", SameBytes(data, {4, 3, 2, 1}));
	RETURN_TEST("test_data_algorithm_reverse", 0);
}

int test_data_algorithm_sort() {
	Data data = Bytes({9, 1, 5, 3});
	std::sort(data.begin(), data.end());
	ASSERT_TRUE("test_data_algorithm_sort", SameBytes(data, {1, 3, 5, 9}));
	RETURN_TEST("test_data_algorithm_sort", 0);
}

// -------------------
// Capacity
// -------------------

int test_data_capacity_clear_keeps_capacity() {
	Data data(Size{8}, std::byte{1});
	data.reserve(Size{32});
	const Size cap = data.capacity();
	data.clear();
	ASSERT_TRUE("test_data_capacity_clear_keeps_capacity", data.empty());
	ASSERT_TRUE("test_data_capacity_clear_keeps_capacity", data.capacity() >= cap);
	RETURN_TEST("test_data_capacity_clear_keeps_capacity", 0);
}

int test_data_capacity_reserve() {
	Data data;
	data.reserve(Size{64});
	ASSERT_TRUE("test_data_capacity_reserve", data.empty());
	ASSERT_TRUE("test_data_capacity_reserve", data.capacity() >= Size{64});
	RETURN_TEST("test_data_capacity_reserve", 0);
}

int test_data_capacity_resize_grow_zero() {
	Data data = Bytes({1, 2});
	data.resize(Size{4});
	ASSERT_TRUE("test_data_capacity_resize_grow_zero", SameBytes(data, {1, 2, 0, 0}));
	RETURN_TEST("test_data_capacity_resize_grow_zero", 0);
}

int test_data_capacity_resize_grow_value() {
	Data data = Bytes({1});
	data.resize(Size{3}, std::byte{9});
	ASSERT_TRUE("test_data_capacity_resize_grow_value", SameBytes(data, {1, 9, 9}));
	RETURN_TEST("test_data_capacity_resize_grow_value", 0);
}

int test_data_capacity_resize_shrink() {
	Data data = Bytes({1, 2, 3, 4});
	data.resize(Size{2});
	ASSERT_TRUE("test_data_capacity_resize_shrink", SameBytes(data, {1, 2}));
	RETURN_TEST("test_data_capacity_resize_shrink", 0);
}

int test_data_capacity_shrink_to_fit() {
	Data data;
	data.reserve(Size{128});
	data.push_back(std::byte{1});
	data.shrink_to_fit();
	ASSERT_TRUE("test_data_capacity_shrink_to_fit", data.size() == Size{1});
	ASSERT_TRUE("test_data_capacity_shrink_to_fit", data.capacity() >= data.size());
	RETURN_TEST("test_data_capacity_shrink_to_fit", 0);
}

// -------------------
// Comparison
// -------------------

int test_data_compare_equal() {
	ASSERT_TRUE("test_data_compare_equal", Bytes({1, 2}) == Bytes({1, 2}));
	ASSERT_TRUE("test_data_compare_equal", Bytes({1, 2}) != Bytes({1, 3}));
	RETURN_TEST("test_data_compare_equal", 0);
}

int test_data_compare_order() {
	ASSERT_TRUE("test_data_compare_order", Bytes({1, 2}) < Bytes({1, 3}));
	ASSERT_TRUE("test_data_compare_order", Bytes({1, 2}) < Bytes({1, 2, 0}));
	ASSERT_TRUE("test_data_compare_order", (Bytes({1, 2}) <=> Bytes({1, 2})) == std::strong_ordering::equal);
	RETURN_TEST("test_data_compare_order", 0);
}

// -------------------
// Concepts
// -------------------

int test_data_concepts_container_shape() {
	static_assert(StormByte::Type::Container<Data>);
	static_assert(StormByte::Type::Sized<Data>);
	static_assert(StormByte::Type::HasPushBack<Data>);
	static_assert(StormByte::Type::HasSubscript<Data, Size>);
	static_assert(StormByte::Type::HasSubscript<Data, std::size_t>);
	static_assert(!StormByte::Type::HasPushFront<Data>);
	static_assert(!StormByte::Type::HasInsert<Data>);
	static_assert(!StormByte::Type::HasKeyType<Data>);
	static_assert(!StormByte::Type::HasMappedType<Data>);
	static_assert(!StormByte::Type::String<Data>);
	RETURN_TEST("test_data_concepts_container_shape", 0);
}

int test_data_concepts_range() {
	static_assert(StormByte::Type::ByteInputRange<Data>);
	Data data = Bytes({1, 2, 3});
	ASSERT_TRUE("test_data_concepts_range", std::ranges::contiguous_range<Data>);
	ASSERT_TRUE("test_data_concepts_range", std::ranges::sized_range<Data>);
	ASSERT_TRUE("test_data_concepts_range", std::ranges::size(data) == 3);
	RETURN_TEST("test_data_concepts_range", 0);
}

// -------------------
// Construction
// -------------------

int test_data_construct_char_pointer() {
	Data data("AB");
	ASSERT_TRUE("test_data_construct_char_pointer", SameBytes(data, {'A', 'B'}));
	Data empty(static_cast<const char*>(nullptr));
	ASSERT_TRUE("test_data_construct_char_pointer", empty.empty());
	RETURN_TEST("test_data_construct_char_pointer", 0);
}

int test_data_construct_copy_move() {
	Data original = Bytes({1, 2, 3});
	Data copied(original);
	ASSERT_TRUE("test_data_construct_copy_move", copied == original);
	Data moved(std::move(original));
	ASSERT_TRUE("test_data_construct_copy_move", SameBytes(moved, {1, 2, 3}));
	ASSERT_TRUE("test_data_construct_copy_move", original.empty());
	RETURN_TEST("test_data_construct_copy_move", 0);
}

int test_data_construct_count_fill() {
	Data data(Size{3}, std::byte{7});
	ASSERT_TRUE("test_data_construct_count_fill", SameBytes(data, {7, 7, 7}));
	RETURN_TEST("test_data_construct_count_fill", 0);
}

int test_data_construct_count_zero() {
	Data data(Size{3});
	ASSERT_TRUE("test_data_construct_count_zero", SameBytes(data, {0, 0, 0}));
	RETURN_TEST("test_data_construct_count_zero", 0);
}

int test_data_construct_empty() {
	Data data;
	ASSERT_TRUE("test_data_construct_empty", data.empty());
	ASSERT_TRUE("test_data_construct_empty", data.size() == Size{0});
	ASSERT_TRUE("test_data_construct_empty", data.begin() == data.end());
	RETURN_TEST("test_data_construct_empty", 0);
}

int test_data_construct_initializer_list() {
	Data data{std::byte{1}, std::byte{2}, std::byte{3}};
	ASSERT_TRUE("test_data_construct_initializer_list", SameBytes(data, {1, 2, 3}));
	RETURN_TEST("test_data_construct_initializer_list", 0);
}

int test_data_construct_pointer_count() {
	const std::byte raw[] = {std::byte{9}, std::byte{8}};
	Data data(raw, Size{2});
	ASSERT_TRUE("test_data_construct_pointer_count", SameBytes(data, {9, 8}));
	Data empty(static_cast<const std::byte*>(nullptr), Size{0});
	ASSERT_TRUE("test_data_construct_pointer_count", empty.empty());
	RETURN_TEST("test_data_construct_pointer_count", 0);
}

int test_data_construct_range() {
	const std::array<unsigned char, 3> raw{1, 2, 3};
	Data data(raw);
	ASSERT_TRUE("test_data_construct_range", SameBytes(data, {1, 2, 3}));
	RETURN_TEST("test_data_construct_range", 0);
}

int test_data_construct_span() {
	const std::byte raw[] = {std::byte{4}, std::byte{5}};
	Data data(std::span<const std::byte>(raw, 2));
	ASSERT_TRUE("test_data_construct_span", SameBytes(data, {4, 5}));
	RETURN_TEST("test_data_construct_span", 0);
}

int test_data_construct_string_view() {
	Data data(std::string_view("Hi"));
	ASSERT_TRUE("test_data_construct_string_view", SameBytes(data, {'H', 'i'}));
	RETURN_TEST("test_data_construct_string_view", 0);
}

// -------------------
// Element access
// -------------------

int test_data_access_at_throws() {
	Data data = Bytes({1});
	bool threw = false;
	try {
		(void)data.at(Size{1});
	} catch (const Exception&) {
		threw = true;
	}
	ASSERT_TRUE("test_data_access_at_throws", threw);
	RETURN_TEST("test_data_access_at_throws", 0);
}

int test_data_access_front_back() {
	Data data = Bytes({1, 2, 3});
	ASSERT_TRUE("test_data_access_front_back", data.front() == std::byte{1});
	ASSERT_TRUE("test_data_access_front_back", data.back() == std::byte{3});
	data.front() = std::byte{9};
	data.back() = std::byte{8};
	ASSERT_TRUE("test_data_access_front_back", SameBytes(data, {9, 2, 8}));
	RETURN_TEST("test_data_access_front_back", 0);
}

int test_data_access_span() {
	Data data = Bytes({1, 2, 3});
	std::span<std::byte> view = data;
	ASSERT_EQUAL("test_data_access_span", 3, view.size());
	view[1] = std::byte{9};
	ASSERT_TRUE("test_data_access_span", data[Size{1}] == std::byte{9});
	RETURN_TEST("test_data_access_span", 0);
}

int test_data_access_subscript() {
	Data data = Bytes({1, 2, 3});
	ASSERT_TRUE("test_data_access_subscript", data[Size{0}] == std::byte{1});
	data[Size{1}] = std::byte{9};
	ASSERT_TRUE("test_data_access_subscript", data.at(Size{1}) == std::byte{9});
	RETURN_TEST("test_data_access_subscript", 0);
}

// -------------------
// Iterators
// -------------------

int test_data_iterators_contiguous() {
	Data data = Bytes({1, 2, 3, 4});
	ASSERT_TRUE("test_data_iterators_contiguous", data.data() + 2 == &data[Size{2}]);
	ASSERT_TRUE("test_data_iterators_contiguous", data.end() == data.begin() + 4);
	ASSERT_TRUE("test_data_iterators_contiguous", std::contiguous_iterator<Data::iterator>);
	ASSERT_TRUE("test_data_iterators_contiguous", std::contiguous_iterator<Data::const_iterator>);
	RETURN_TEST("test_data_iterators_contiguous", 0);
}

int test_data_iterators_forward() {
	Data data = Bytes({1, 2, 3});
	unsigned sum = 0;
	for (auto it = data.begin(); it != data.end(); ++it)
		sum += static_cast<unsigned>(*it);
	ASSERT_EQUAL("test_data_iterators_forward", 6u, sum);
	ASSERT_TRUE("test_data_iterators_forward", data.cbegin() == data.begin());
	ASSERT_TRUE("test_data_iterators_forward", data.cend() == data.end());
	RETURN_TEST("test_data_iterators_forward", 0);
}

int test_data_iterators_reverse() {
	Data data = Bytes({1, 2, 3});
	std::vector<unsigned char> seen;
	for (auto it = data.rbegin(); it != data.rend(); ++it)
		seen.push_back(static_cast<unsigned char>(*it));
	ASSERT_TRUE("test_data_iterators_reverse", seen.size() == 3 && seen[0] == 3 && seen[1] == 2 && seen[2] == 1);
	ASSERT_TRUE("test_data_iterators_reverse", *data.crbegin() == std::byte{3});
	RETURN_TEST("test_data_iterators_reverse", 0);
}

// -------------------
// Modifiers
// -------------------

int test_data_modifiers_append() {
	Data data = Bytes({1});
	const std::byte extra[] = {std::byte{2}, std::byte{3}};
	data.append(std::span<const std::byte>(extra, 2));
	data.append(Bytes({4}));
	ASSERT_TRUE("test_data_modifiers_append", SameBytes(data, {1, 2, 3, 4}));
	Data moved = Bytes({5});
	data.append(std::move(moved));
	ASSERT_TRUE("test_data_modifiers_append", SameBytes(data, {1, 2, 3, 4, 5}));
	ASSERT_TRUE("test_data_modifiers_append", moved.empty());
	RETURN_TEST("test_data_modifiers_append", 0);
}

int test_data_modifiers_assign() {
	Data data = Bytes({9, 9});
	data.assign(Size{3}, std::byte{1});
	ASSERT_TRUE("test_data_modifiers_assign", SameBytes(data, {1, 1, 1}));
	data.assign({std::byte{2}, std::byte{3}});
	ASSERT_TRUE("test_data_modifiers_assign", SameBytes(data, {2, 3}));
	const std::byte raw[] = {std::byte{4}};
	data.assign(std::span<const std::byte>(raw, 1));
	ASSERT_TRUE("test_data_modifiers_assign", SameBytes(data, {4}));
	const unsigned char more[] = {5, 6};
	data.assign(more, more + 2);
	ASSERT_TRUE("test_data_modifiers_assign", SameBytes(data, {5, 6}));
	RETURN_TEST("test_data_modifiers_assign", 0);
}

int test_data_modifiers_erase() {
	Data data = Bytes({1, 2, 3, 4});
	auto it = data.erase(data.begin() + 1);
	ASSERT_TRUE("test_data_modifiers_erase", *it == std::byte{3});
	ASSERT_TRUE("test_data_modifiers_erase", SameBytes(data, {1, 3, 4}));
	data.erase(data.begin(), data.begin() + 2);
	ASSERT_TRUE("test_data_modifiers_erase", SameBytes(data, {4}));
	RETURN_TEST("test_data_modifiers_erase", 0);
}

int test_data_modifiers_insert() {
	Data data = Bytes({1, 4});
	auto it = data.insert(data.begin() + 1, std::byte{2});
	ASSERT_TRUE("test_data_modifiers_insert", *it == std::byte{2});
	data.insert(data.begin() + 2, Size{1}, std::byte{3});
	ASSERT_TRUE("test_data_modifiers_insert", SameBytes(data, {1, 2, 3, 4}));
	data.insert(data.end(), {std::byte{5}});
	ASSERT_TRUE("test_data_modifiers_insert", SameBytes(data, {1, 2, 3, 4, 5}));
	const unsigned char extra[] = {6};
	data.insert(data.end(), extra, extra + 1);
	ASSERT_TRUE("test_data_modifiers_insert", SameBytes(data, {1, 2, 3, 4, 5, 6}));
	RETURN_TEST("test_data_modifiers_insert", 0);
}

int test_data_modifiers_push_pop() {
	Data data;
	data.push_back(std::byte{1});
	data.emplace_back(2);
	ASSERT_TRUE("test_data_modifiers_push_pop", SameBytes(data, {1, 2}));
	data.pop_back();
	ASSERT_TRUE("test_data_modifiers_push_pop", SameBytes(data, {1}));
	RETURN_TEST("test_data_modifiers_push_pop", 0);
}

int test_data_modifiers_swap() {
	Data a = Bytes({1, 2});
	Data b = Bytes({3});
	a.swap(b);
	ASSERT_TRUE("test_data_modifiers_swap", SameBytes(a, {3}));
	ASSERT_TRUE("test_data_modifiers_swap", SameBytes(b, {1, 2}));
	swap(a, b);
	ASSERT_TRUE("test_data_modifiers_swap", SameBytes(a, {1, 2}));
	ASSERT_TRUE("test_data_modifiers_swap", SameBytes(b, {3}));
	RETURN_TEST("test_data_modifiers_swap", 0);
}

int main() {
	int result = 0;

	// -------------------
	// Algorithms
	// -------------------
	result += test_data_algorithm_copy();
	result += test_data_algorithm_count();
	result += test_data_algorithm_equal();
	result += test_data_algorithm_fill();
	result += test_data_algorithm_find();
	result += test_data_algorithm_ranges_sort();
	result += test_data_algorithm_reverse();
	result += test_data_algorithm_sort();

	// -------------------
	// Capacity
	// -------------------
	result += test_data_capacity_clear_keeps_capacity();
	result += test_data_capacity_reserve();
	result += test_data_capacity_resize_grow_zero();
	result += test_data_capacity_resize_grow_value();
	result += test_data_capacity_resize_shrink();
	result += test_data_capacity_shrink_to_fit();

	// -------------------
	// Comparison
	// -------------------
	result += test_data_compare_equal();
	result += test_data_compare_order();

	// -------------------
	// Concepts
	// -------------------
	result += test_data_concepts_container_shape();
	result += test_data_concepts_range();

	// -------------------
	// Construction
	// -------------------
	result += test_data_construct_char_pointer();
	result += test_data_construct_copy_move();
	result += test_data_construct_count_fill();
	result += test_data_construct_count_zero();
	result += test_data_construct_empty();
	result += test_data_construct_initializer_list();
	result += test_data_construct_pointer_count();
	result += test_data_construct_range();
	result += test_data_construct_span();
	result += test_data_construct_string_view();

	// -------------------
	// Element access
	// -------------------
	result += test_data_access_at_throws();
	result += test_data_access_front_back();
	result += test_data_access_span();
	result += test_data_access_subscript();

	// -------------------
	// Iterators
	// -------------------
	result += test_data_iterators_contiguous();
	result += test_data_iterators_forward();
	result += test_data_iterators_reverse();

	// -------------------
	// Modifiers
	// -------------------
	result += test_data_modifiers_append();
	result += test_data_modifiers_assign();
	result += test_data_modifiers_erase();
	result += test_data_modifiers_insert();
	result += test_data_modifiers_push_pop();
	result += test_data_modifiers_swap();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
