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

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/test_handlers.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <thread>

using StormByte::Buffer::DataType;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::IO::ToString;

namespace {
	std::filesystem::path File(const char* name) {
		return CurrentFileDirectory / "files" / name;
	}

	std::string Text(FIFO& fifo) {
		DataType data;
		static_cast<void>(fifo.Peek(0, data));
		return std::string(reinterpret_cast<const char*>(data.data()), data.size());
	}

	DataType Bytes(FIFO& fifo) {
		DataType data;
		static_cast<void>(fifo.Peek(0, data));
		return data;
	}

	void Fill(std::span<std::byte> dest, const std::byte value) {
		for (auto& b : dest)
			b = value;
	}
}

// -------------------
// Fixtures
// -------------------

int test_lines_are_octets() {
	const std::string fn = "test_lines_are_octets";
	BufferedFileReader in(File("lines.txt"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	const auto read = in.Read(18, dest);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL(fn, std::string("line1\nline2\nline3\n"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{18}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_no_nl_and_nul() {
	const std::string fn = "test_no_nl_and_nul";
	BufferedFileReader text(File("no_nl.txt"));
	ASSERT_TRUE(fn, text.Open());
	FIFO t;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(text.Read(17, t).status));
	ASSERT_EQUAL(fn, std::string("no newline at end"), Text(t));
	ASSERT_EQUAL(fn, StormByte::Size{17}, text.Tell());
	BufferedFileReader raw(File("with_nuls.bin"));
	ASSERT_TRUE(fn, raw.Open());
	FIFO n;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(raw.Read(5, n).status));
	const auto bytes = Bytes(n);
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), bytes.size());
	ASSERT_EQUAL(fn, std::byte{'A'}, bytes[0]);
	ASSERT_EQUAL(fn, std::byte{0}, bytes[1]);
	RETURN_TEST(fn, 0);
}

int test_pattern_256() {
	const std::string fn = "test_pattern_256";
	BufferedFileReader in(File("pattern_256.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	const auto read = in.Read(256, dest);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(read.status));
	const auto bytes = Bytes(dest);
	ASSERT_EQUAL(fn, static_cast<std::size_t>(256), bytes.size());
	for (std::size_t i = 0; i < 256; ++i)
		ASSERT_EQUAL(fn, static_cast<std::byte>(i), bytes[i]);
	ASSERT_EQUAL(fn, StormByte::Size{256}, in.Tell());
	RETURN_TEST(fn, 0);
}

// -------------------
// Move
// -------------------

int test_move_transfers_session() {
	const std::string fn = "test_move_transfers_session";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO first;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, first).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	BufferedFileReader moved(std::move(in));
	ASSERT_TRUE(fn, moved.IsOpen());
	ASSERT_EQUAL(fn, StormByte::Size{2}, moved.Tell());
	FIFO rest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(moved.Read(3, rest).status));
	ASSERT_EQUAL(fn, std::string("CDE"), Text(rest));
	ASSERT_EQUAL(fn, StormByte::Size{5}, moved.Tell());
	RETURN_TEST(fn, 0);
}

// -------------------
// Path-only / Setup
// -------------------

int test_explicit_readahead_survives_setup() {
	const std::string fn = "test_explicit_readahead_survives_setup";
	BufferedFileReader in(File("ahead.bin"), 25, 1024);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, StormByte::Size{25}, in.ReadAhead());
	ASSERT_EQUAL(fn, StormByte::Size{1024}, in.MaxMemory());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL(fn, std::string("01234"), Text(dest));
	RETURN_TEST(fn, 0);
}

int test_explicit_zero_survives_open() {
	const std::string fn = "test_explicit_zero_survives_open";
	BufferedFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.ReadAhead());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.MaxMemory());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	RETURN_TEST(fn, 0);
}

int test_path_only_setup_sets_readahead() {
	const std::string fn = "test_path_only_setup_sets_readahead";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, in.ReadAhead() > 0);
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	RETURN_TEST(fn, 0);
}

// -------------------
// Policy / volume
// -------------------

int test_block_4k_chunked() {
	const std::string fn = "test_block_4k_chunked";
	BufferedFileReader in(File("block_4k.bin"), 512, 2048);
	ASSERT_TRUE(fn, in.Open());
	StormByte::Size total{0};
	while (!in.EoF()) {
		FIFO dest;
		const auto read = in.Read(1000, dest);
		if (read.status == Status::Failed || read.status == Status::Error)
			return 1;
		total += read.count;
		ASSERT_EQUAL(fn, total, in.Tell());
		if (read.status == Status::End)
			break;
	}
	ASSERT_EQUAL(fn, StormByte::Size{4096}, total);
	RETURN_TEST(fn, 0);
}

int test_max_memory_zero_span_reads() {
	const std::string fn = "test_max_memory_zero_span_reads";
	BufferedFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	std::array<std::byte, 5> raw {};
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(got.status));
	ASSERT_EQUAL(fn, std::string("ABCDE"),
		std::string(reinterpret_cast<const char*>(raw.data()), 5));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_max_memory_zero_still_reads() {
	const std::string fn = "test_max_memory_zero_still_reads";
	BufferedFileReader in(File("block_256.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	const auto read = in.Read(256, dest);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{256}, read.count);
	ASSERT_EQUAL(fn, StormByte::Size{256}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_readahead_knobs() {
	const std::string fn = "test_readahead_knobs";
	BufferedFileReader in(File("ahead.bin"), 25, 1024);
	ASSERT_EQUAL(fn, StormByte::Size{25}, in.ReadAhead());
	ASSERT_EQUAL(fn, StormByte::Size{1024}, in.MaxMemory());
	RETURN_TEST(fn, 0);
}

// -------------------
// Read / Peek
// -------------------

int test_empty_file() {
	const std::string fn = "test_empty_file";
	BufferedFileReader in(File("empty.bin"));
	ASSERT_TRUE(fn, in.Open());
	const auto size = in.Size();
	ASSERT_TRUE(fn, size.has_value());
	ASSERT_EQUAL(fn, StormByte::Size{0}, *size);
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO dest("KEEP");
	const auto read = in.Read(1, dest);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, read.count);
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_TRUE(fn, in.EoF());
	ASSERT_FALSE(fn, static_cast<bool>(in));
	RETURN_TEST(fn, 0);
}

int test_open_five_read_exact() {
	const std::string fn = "test_open_five_read_exact";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, in.IsOpen());
	ASSERT_TRUE(fn, static_cast<bool>(in));
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE(fn, in.IsSeekable());
	ASSERT_TRUE(fn, in.IsSized());
	const auto size = in.Size();
	ASSERT_TRUE(fn, size.has_value());
	ASSERT_EQUAL(fn, StormByte::Size{5}, *size);
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_EQUAL(fn, File("five.bin"), in.Path());
	FIFO dest;
	const auto read = in.Read(5, dest);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, read.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_peek_does_not_consume() {
	const std::string fn = "test_peek_does_not_consume";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO peek;
	const auto peeked = in.Peek(3, peek);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(peeked.status));
	ASSERT_EQUAL(fn, std::string("ABC"), Text(peek));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(3, dest).status));
	ASSERT_EQUAL(fn, std::string("ABC"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_peek_span_does_not_consume() {
	const std::string fn = "test_peek_span_does_not_consume";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	std::array<std::byte, 3> raw {};
	const auto peeked = in.Peek(std::span<std::byte>(raw));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(peeked.status));
	ASSERT_EQUAL(fn, std::string("ABC"),
		std::string(reinterpret_cast<const char*>(raw.data()), 3));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(3, dest).status));
	ASSERT_EQUAL(fn, std::string("ABC"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_read_overwrites_dest() {
	const std::string fn = "test_read_overwrites_dest";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest("OLD");
	const auto read = in.Read(5, dest);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	RETURN_TEST(fn, 0);
}

int test_read_past_end_is_short() {
	const std::string fn = "test_read_past_end_is_short";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	const auto read = in.Read(100, dest);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, read.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	ASSERT_TRUE(fn, in.EoF());
	ASSERT_FALSE(fn, static_cast<bool>(in));
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	RETURN_TEST(fn, 0);
}

int test_read_span_before_open_leaves_dest() {
	const std::string fn = "test_read_span_before_open_leaves_dest";
	BufferedFileReader in(File("five.bin"));
	std::array<std::byte, 2> raw {};
	Fill(raw, std::byte{0x5A});
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(got.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, got.count);
	ASSERT_EQUAL(fn, static_cast<unsigned>(0x5A), static_cast<unsigned>(raw[0]));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_read_span_empty_ok() {
	const std::string fn = "test_read_span_empty_ok";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	const auto got = in.Read(std::span<std::byte>{});
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(got.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, got.count);
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, dest).status));
	ASSERT_EQUAL(fn, std::string("AB"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_read_span_exact() {
	const std::string fn = "test_read_span_exact";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	std::array<std::byte, 5> raw {};
	Fill(raw, std::byte{0x5A});
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(got.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, got.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"),
		std::string(reinterpret_cast<const char*>(raw.data()), raw.size()));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_read_span_short_leaves_tail() {
	const std::string fn = "test_read_span_short_leaves_tail";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	std::array<std::byte, 8> raw {};
	Fill(raw, std::byte{0x5A});
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(got.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, got.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"),
		std::string(reinterpret_cast<const char*>(raw.data()), 5));
	ASSERT_EQUAL(fn, static_cast<unsigned>(0x5A), static_cast<unsigned>(raw[5]));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_read_zero_serves_window() {
	const std::string fn = "test_read_zero_serves_window";
	BufferedFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	FIFO empty;
	const auto first = in.Read(0, empty);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(first.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, first.count);
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO chunk;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, chunk).status));
	ASSERT_EQUAL(fn, std::string("AB"), Text(chunk));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_second_read_after_end() {
	const std::string fn = "test_second_read_after_end";
	BufferedFileReader in(File("one.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(1, dest).status));
	ASSERT_EQUAL(fn, std::string("A"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{1}, in.Tell());
	FIFO again("KEEP");
	const auto end = in.Read(1, again);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(end.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, end.count);
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(again));
	ASSERT_EQUAL(fn, StormByte::Size{1}, in.Tell());
	ASSERT_FALSE(fn, static_cast<bool>(in));
	RETURN_TEST(fn, 0);
}

int test_sequential_splits() {
	const std::string fn = "test_sequential_splits";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO a, b;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(3, a).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, b).status));
	ASSERT_EQUAL(fn, std::string("ABC"), Text(a));
	ASSERT_EQUAL(fn, std::string("DE"), Text(b));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

// -------------------
// Seek
// -------------------

int test_peek_window_survives_seek() {
	const std::string fn = "test_peek_window_survives_seek";
	BufferedFileReader in(File("ahead.bin"), 16, 64);
	ASSERT_TRUE(fn, in.Open());
	FIFO window;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Peek(10, window).status));
	ASSERT_EQUAL(fn, std::string("0123456789"), Text(window));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	FIFO mid;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Peek(4, mid).status));
	ASSERT_EQUAL(fn, std::string("4567"), Text(mid));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	FIFO again;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Peek(6, again).status));
	ASSERT_EQUAL(fn, std::string("012345"), Text(again));
	FIFO consumed;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(6, consumed).status));
	ASSERT_EQUAL(fn, std::string("012345"), Text(consumed));
	ASSERT_EQUAL(fn, StormByte::Size{6}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_absolute_and_relative() {
	const std::string fn = "test_seek_absolute_and_relative";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, dest).status));
	ASSERT_EQUAL(fn, std::string("45"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{6}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(-2, Position::Relative).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	FIFO again;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(3, again).status));
	ASSERT_EQUAL(fn, std::string("456"), Text(again));
	ASSERT_EQUAL(fn, StormByte::Size{7}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_end_via_size() {
	const std::string fn = "test_seek_end_via_size";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, in.IsSized());
	const auto size = in.Size();
	ASSERT_TRUE(fn, size.has_value());
	ASSERT_EQUAL(fn, ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size), Position::Absolute).status));
	ASSERT_EQUAL(fn, *size, in.Tell());
	FIFO dest("KEEP");
	const auto end = in.Read(1, dest);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(end.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, end.count);
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(dest));
	ASSERT_EQUAL(fn, *size, in.Tell());
	ASSERT_TRUE(fn, in.EoF());
	ASSERT_EQUAL(fn, ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size) - 2, Position::Absolute).status));
	ASSERT_EQUAL(fn, *size - 2, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	ASSERT_TRUE(fn, static_cast<bool>(in));
	FIFO tail;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, tail).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, tail.AvailableBytes());
	ASSERT_EQUAL(fn, *size, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_hits_readahead_then_realigns() {
	const std::string fn = "test_seek_hits_readahead_then_realigns";
	BufferedFileReader in(File("ahead.bin"), 16, 64);
	ASSERT_TRUE(fn, in.Open());
	FIFO first;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, first).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(6, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{6}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	FIFO mid;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(4, mid).status));
	ASSERT_EQUAL(fn, std::string("6789"), Text(mid));
	ASSERT_EQUAL(fn, StormByte::Size{10}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_negative_absolute_fails() {
	const std::string fn = "test_seek_negative_absolute_fails";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Seek(-1, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_past_size_then_read() {
	const std::string fn = "test_seek_past_size_then_read";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	const auto size = in.Size();
	ASSERT_TRUE(fn, size.has_value());
	ASSERT_EQUAL(fn, ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size + 8), Position::Absolute).status));
	ASSERT_EQUAL(fn, *size + 8, in.Tell());
	FIFO dest("KEEP");
	const auto got = in.Read(4, dest);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(got.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, got.count);
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(dest));
	ASSERT_EQUAL(fn, *size + 8, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_relative_before_zero_fails() {
	const std::string fn = "test_seek_relative_before_zero_fails";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Seek(-1, Position::Relative).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_without_open_fails() {
	const std::string fn = "test_seek_without_open_fails";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_seek_zero_after_end_clears_eof() {
	const std::string fn = "test_seek_zero_after_end_clears_eof";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO first;
	const auto drain = in.Read(16, first);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(drain.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, drain.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(first));
	ASSERT_TRUE(fn, in.EoF());
	ASSERT_FALSE(fn, static_cast<bool>(in));
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	ASSERT_TRUE(fn, static_cast<bool>(in));
	ASSERT_TRUE(fn, in.IsReadable());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	FIFO second;
	const auto again = in.Read(16, second);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(again.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, again.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(second));
	ASSERT_TRUE(fn, in.EoF());
	RETURN_TEST(fn, 0);
}

int test_seek_zero_after_end_twice() {
	const std::string fn = "test_seek_zero_after_end_twice";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	for (int pass = 0; pass < 2; ++pass) {
		FIFO dest;
		const auto got = in.Read(8, dest);
		ASSERT_EQUAL(fn, ToString(Status::End), ToString(got.status));
		ASSERT_EQUAL(fn, StormByte::Size{5}, got.count);
		ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
		ASSERT_TRUE(fn, in.EoF());
		ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
		ASSERT_FALSE(fn, in.EoF());
		ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	}
	FIFO last;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, last).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(last));
	RETURN_TEST(fn, 0);
}

int test_seek_zero_on_fresh_open() {
	const std::string fn = "test_seek_zero_on_fresh_open";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	ASSERT_TRUE(fn, static_cast<bool>(in));
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	RETURN_TEST(fn, 0);
}

int test_seek_zero_rereads() {
	const std::string fn = "test_seek_zero_rereads";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO first;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(4, first).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	ASSERT_TRUE(fn, static_cast<bool>(in));
	FIFO again;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(4, again).status));
	ASSERT_EQUAL(fn, Text(first), Text(again));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	RETURN_TEST(fn, 0);
}

// -------------------
// Session / errors
// -------------------

int test_close_then_reopen() {
	const std::string fn = "test_close_then_reopen";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Close().status));
	ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(in.State()));
	ASSERT_FALSE(fn, static_cast<bool>(in));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Close().status));
	FIFO dest("KEEP");
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Read(1, dest).status));
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(dest));
	ASSERT_FALSE(fn, in.Rewind());
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE(fn, static_cast<bool>(in));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO again;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, again).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(again));
	RETURN_TEST(fn, 0);
}

int test_ctor_unavailable_bool_false() {
	const std::string fn = "test_ctor_unavailable_bool_false";
	BufferedFileReader in(File("five.bin"));
	ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(in.State()));
	ASSERT_FALSE(fn, static_cast<bool>(in));
	ASSERT_FALSE(fn, in.IsOpen());
	ASSERT_FALSE(fn, in.IsReadable());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.ReadAhead());
	ASSERT_EQUAL(fn, File("five.bin"), in.Path());
	RETURN_TEST(fn, 0);
}

int test_open_directory() {
	const std::string fn = "test_open_directory";
	BufferedFileReader in(CurrentFileDirectory / "files");
	ASSERT_FALSE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(State::Directory), ToString(in.State()));
	ASSERT_FALSE(fn, static_cast<bool>(in));
	RETURN_TEST(fn, 0);
}

int test_open_missing() {
	const std::string fn = "test_open_missing";
	BufferedFileReader in(File("does-not-exist.bin"));
	ASSERT_FALSE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(State::Missing), ToString(in.State()));
	ASSERT_FALSE(fn, static_cast<bool>(in));
	ASSERT_FALSE(fn, in.IsOpen());
	RETURN_TEST(fn, 0);
}

int test_open_not_idempotent() {
	const std::string fn = "test_open_not_idempotent";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE(fn, static_cast<bool>(in));
	ASSERT_FALSE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE(fn, static_cast<bool>(in));
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	RETURN_TEST(fn, 0);
}

int test_read_before_open_leaves_dest() {
	const std::string fn = "test_read_before_open_leaves_dest";
	BufferedFileReader in(File("five.bin"));
	FIFO dest("KEEP");
	const auto read = in.Read(1, dest);
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, read.count);
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_rewind_rereads() {
	const std::string fn = "test_rewind_rereads";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO first;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, first).status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	ASSERT_TRUE(fn, in.Rewind());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(in.State()));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO second;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, second).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(second));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_rewind_without_open_fails() {
	const std::string fn = "test_rewind_without_open_fails";
	BufferedFileReader in(File("five.bin"));
	ASSERT_FALSE(fn, in.Rewind());
	ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(in.State()));
	RETURN_TEST(fn, 0);
}

// -------------------
// Tell
// -------------------

int test_tell_after_span_read_and_seek_zero() {
	const std::string fn = "test_tell_after_span_read_and_seek_zero";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	std::array<std::byte, 3> raw {};
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Read(std::span<std::byte>(raw)).count);
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_FALSE(fn, in.EoF());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_tell_at_size_and_past_size() {
	const std::string fn = "test_tell_at_size_and_past_size";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	const auto size = in.Size();
	ASSERT_TRUE(fn, size.has_value());
	ASSERT_EQUAL(fn, ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size), Position::Absolute).status));
	ASSERT_EQUAL(fn, *size, in.Tell());
	FIFO dest("KEEP");
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Read(3, dest).count);
	ASSERT_EQUAL(fn, *size, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size + 6), Position::Absolute).status));
	ASSERT_EQUAL(fn, *size + 6, in.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Read(1, dest).count);
	ASSERT_EQUAL(fn, *size + 6, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_tell_matches_seek_and_failed_seek_stays() {
	const std::string fn = "test_tell_matches_seek_and_failed_seek_stays";
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(7, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{7}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(-3, Position::Relative).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Seek(-10, Position::Relative).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Seek(-1, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_tell_max_memory_zero_seek_read() {
	const std::string fn = "test_tell_max_memory_zero_seek_read";
	BufferedFileReader in(File("seek.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, in.Tell());
	FIFO dest;
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Read(2, dest).count);
	ASSERT_EQUAL(fn, std::string("45"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{6}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_tell_open_is_zero() {
	const std::string fn = "test_tell_open_is_zero";
	BufferedFileReader in(File("five.bin"));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	ASSERT_TRUE(fn, in.Open());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_tell_tracks_each_read() {
	const std::string fn = "test_tell_tracks_each_read";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO a, b, c;
	ASSERT_EQUAL(fn, StormByte::Size{1}, in.Read(1, a).count);
	ASSERT_EQUAL(fn, StormByte::Size{1}, in.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Read(2, b).count);
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Read(2, c).count);
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_tell_unchanged_on_peek_and_empty_span() {
	const std::string fn = "test_tell_unchanged_on_peek_and_empty_span";
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(2, dest).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	FIFO peek;
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Peek(2, peek).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	std::array<std::byte, 2> raw {};
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Peek(std::span<std::byte>(raw)).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Read(std::span<std::byte>{}).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Tell());
	RETURN_TEST(fn, 0);
}

int main() {
	int result = 0;

	// -------------------
	// Fixtures
	// -------------------
	result += test_lines_are_octets();
	result += test_no_nl_and_nul();
	result += test_pattern_256();

	// -------------------
	// Move
	// -------------------
	result += test_move_transfers_session();

	// -------------------
	// Path-only / Setup
	// -------------------
	result += test_explicit_readahead_survives_setup();
	result += test_explicit_zero_survives_open();
	result += test_path_only_setup_sets_readahead();

	// -------------------
	// Policy / volume
	// -------------------
	result += test_block_4k_chunked();
	result += test_max_memory_zero_span_reads();
	result += test_max_memory_zero_still_reads();
	result += test_readahead_knobs();

	// -------------------
	// Read / Peek
	// -------------------
	result += test_empty_file();
	result += test_open_five_read_exact();
	result += test_peek_does_not_consume();
	result += test_peek_span_does_not_consume();
	result += test_read_overwrites_dest();
	result += test_read_past_end_is_short();
	result += test_read_span_before_open_leaves_dest();
	result += test_read_span_empty_ok();
	result += test_read_span_exact();
	result += test_read_span_short_leaves_tail();
	result += test_read_zero_serves_window();
	result += test_second_read_after_end();
	result += test_sequential_splits();

	// -------------------
	// Seek
	// -------------------
	result += test_peek_window_survives_seek();
	result += test_seek_absolute_and_relative();
	result += test_seek_end_via_size();
	result += test_seek_hits_readahead_then_realigns();
	result += test_seek_negative_absolute_fails();
	result += test_seek_past_size_then_read();
	result += test_seek_relative_before_zero_fails();
	result += test_seek_without_open_fails();
	result += test_seek_zero_after_end_clears_eof();
	result += test_seek_zero_after_end_twice();
	result += test_seek_zero_on_fresh_open();
	result += test_seek_zero_rereads();

	// -------------------
	// Session / errors
	// -------------------
	result += test_close_then_reopen();
	result += test_ctor_unavailable_bool_false();
	result += test_open_directory();
	result += test_open_missing();
	result += test_open_not_idempotent();
	result += test_read_before_open_leaves_dest();
	result += test_rewind_rereads();
	result += test_rewind_without_open_fails();

	// -------------------
	// Tell
	// -------------------
	result += test_tell_after_span_read_and_seek_zero();
	result += test_tell_at_size_and_past_size();
	result += test_tell_matches_seek_and_failed_seek_stays();
	result += test_tell_max_memory_zero_seek_read();
	result += test_tell_open_is_zero();
	result += test_tell_tracks_each_read();
	result += test_tell_unchanged_on_peek_and_empty_span();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
