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

#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/test_handlers.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using StormByte::Buffer::DataType;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::IO::ToString;

static std::filesystem::path File(const char* name) {
	return CurrentFileDirectory / "files" / name;
}

static std::string Text(FIFO& fifo) {
	DataType data;
	static_cast<void>(fifo.Peek(0, data));
	return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

static DataType Bytes(FIFO& fifo) {
	DataType data;
	static_cast<void>(fifo.Peek(0, data));
	return data;
}

// -------------------
// Session / errors
// -------------------

int test_ctor_unavailable_bool_false() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_EQUAL("test_ctor_unavailable_bool_false", ToString(State::Unavailable), ToString(in.State()));
	ASSERT_FALSE("test_ctor_unavailable_bool_false", static_cast<bool>(in));
	ASSERT_FALSE("test_ctor_unavailable_bool_false", in.IsOpen());
	ASSERT_FALSE("test_ctor_unavailable_bool_false", in.IsReadable());
	RETURN_TEST("test_ctor_unavailable_bool_false", 0);
}

int test_open_missing() {
	BufferedFileReader in(File("does-not-exist.bin"));
	ASSERT_FALSE("test_open_missing", in.Open());
	ASSERT_EQUAL("test_open_missing", ToString(State::Missing), ToString(in.State()));
	ASSERT_FALSE("test_open_missing", static_cast<bool>(in));
	ASSERT_FALSE("test_open_missing", in.IsOpen());
	RETURN_TEST("test_open_missing", 0);
}

int test_open_directory() {
	BufferedFileReader in(CurrentFileDirectory / "files");
	ASSERT_FALSE("test_open_directory", in.Open());
	ASSERT_EQUAL("test_open_directory", ToString(State::Directory), ToString(in.State()));
	ASSERT_FALSE("test_open_directory", static_cast<bool>(in));
	RETURN_TEST("test_open_directory", 0);
}

int test_rewind_without_open_fails() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_FALSE("test_rewind_without_open_fails", in.Rewind());
	ASSERT_EQUAL("test_rewind_without_open_fails", ToString(State::Unavailable), ToString(in.State()));
	RETURN_TEST("test_rewind_without_open_fails", 0);
}

int test_read_before_open_leaves_dest() {
	BufferedFileReader in(File("five.bin"));
	FIFO dest("KEEP");
	const auto read = in.Read(1, dest);
	ASSERT_EQUAL("test_read_before_open_leaves_dest", ToString(Status::Failed), ToString(read.status));
	ASSERT_EQUAL("test_read_before_open_leaves_dest", static_cast<std::size_t>(0), read.count);
	ASSERT_EQUAL("test_read_before_open_leaves_dest", std::string("KEEP"), Text(dest));
	RETURN_TEST("test_read_before_open_leaves_dest", 0);
}

int test_open_not_idempotent() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_open_not_idempotent", in.Open());
	ASSERT_EQUAL("test_open_not_idempotent", ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE("test_open_not_idempotent", static_cast<bool>(in));
	ASSERT_FALSE("test_open_not_idempotent", in.Open());
	ASSERT_EQUAL("test_open_not_idempotent", ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE("test_open_not_idempotent", static_cast<bool>(in));
	FIFO dest;
	ASSERT_EQUAL("test_open_not_idempotent", ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL("test_open_not_idempotent", std::string("ABCDE"), Text(dest));
	RETURN_TEST("test_open_not_idempotent", 0);
}

int test_close_then_reopen() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_close_then_reopen", in.Open());
	ASSERT_EQUAL("test_close_then_reopen", ToString(Status::Ok), ToString(in.Close().status));
	ASSERT_EQUAL("test_close_then_reopen", ToString(State::Unavailable), ToString(in.State()));
	ASSERT_FALSE("test_close_then_reopen", static_cast<bool>(in));
	ASSERT_EQUAL("test_close_then_reopen", ToString(Status::Ok), ToString(in.Close().status));
	FIFO dest("KEEP");
	ASSERT_EQUAL("test_close_then_reopen", ToString(Status::Failed), ToString(in.Read(1, dest).status));
	ASSERT_EQUAL("test_close_then_reopen", std::string("KEEP"), Text(dest));
	ASSERT_FALSE("test_close_then_reopen", in.Rewind());
	ASSERT_TRUE("test_close_then_reopen", in.Open());
	ASSERT_EQUAL("test_close_then_reopen", ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE("test_close_then_reopen", static_cast<bool>(in));
	FIFO again;
	ASSERT_EQUAL("test_close_then_reopen", ToString(Status::Ok), ToString(in.Read(5, again).status));
	ASSERT_EQUAL("test_close_then_reopen", std::string("ABCDE"), Text(again));
	RETURN_TEST("test_close_then_reopen", 0);
}

int test_rewind_rereads() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_rewind_rereads", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_rewind_rereads", ToString(Status::Ok), ToString(in.Read(5, first).status));
	ASSERT_TRUE("test_rewind_rereads", in.Rewind());
	ASSERT_EQUAL("test_rewind_rereads", ToString(State::Idle), ToString(in.State()));
	ASSERT_EQUAL("test_rewind_rereads", static_cast<std::size_t>(0), in.Tell());
	FIFO second;
	ASSERT_EQUAL("test_rewind_rereads", ToString(Status::Ok), ToString(in.Read(5, second).status));
	ASSERT_EQUAL("test_rewind_rereads", std::string("ABCDE"), Text(second));
	RETURN_TEST("test_rewind_rereads", 0);
}

// -------------------
// Read / Peek
// -------------------

int test_open_five_read_exact() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_open_five_read_exact", in.Open());
	ASSERT_TRUE("test_open_five_read_exact", in.IsOpen());
	ASSERT_TRUE("test_open_five_read_exact", static_cast<bool>(in));
	ASSERT_EQUAL("test_open_five_read_exact", ToString(State::Idle), ToString(in.State()));
	ASSERT_TRUE("test_open_five_read_exact", in.IsSeekable());
	ASSERT_TRUE("test_open_five_read_exact", in.IsSized());
	const auto size = in.Size();
	ASSERT_TRUE("test_open_five_read_exact", size.has_value());
	ASSERT_EQUAL("test_open_five_read_exact", static_cast<std::size_t>(5), *size);
	ASSERT_EQUAL("test_open_five_read_exact", static_cast<std::size_t>(0), in.Tell());
	ASSERT_EQUAL("test_open_five_read_exact", File("five.bin"), in.Path());

	FIFO dest;
	const auto read = in.Read(5, dest);
	ASSERT_EQUAL("test_open_five_read_exact", ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL("test_open_five_read_exact", static_cast<std::size_t>(5), read.count);
	ASSERT_EQUAL("test_open_five_read_exact", std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL("test_open_five_read_exact", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_open_five_read_exact", 0);
}

int test_read_overwrites_dest() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_read_overwrites_dest", in.Open());
	FIFO dest("OLD");
	const auto read = in.Read(5, dest);
	ASSERT_EQUAL("test_read_overwrites_dest", ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL("test_read_overwrites_dest", std::string("ABCDE"), Text(dest));
	RETURN_TEST("test_read_overwrites_dest", 0);
}

int test_read_past_end_is_short() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_read_past_end_is_short", in.Open());
	FIFO dest;
	const auto read = in.Read(100, dest);
	ASSERT_EQUAL("test_read_past_end_is_short", ToString(Status::End), ToString(read.status));
	ASSERT_EQUAL("test_read_past_end_is_short", static_cast<std::size_t>(5), read.count);
	ASSERT_EQUAL("test_read_past_end_is_short", std::string("ABCDE"), Text(dest));
	ASSERT_TRUE("test_read_past_end_is_short", in.EoF());
	ASSERT_FALSE("test_read_past_end_is_short", static_cast<bool>(in));
	ASSERT_EQUAL("test_read_past_end_is_short", ToString(State::Idle), ToString(in.State()));
	RETURN_TEST("test_read_past_end_is_short", 0);
}

int test_second_read_after_end() {
	BufferedFileReader in(File("one.bin"));
	ASSERT_TRUE("test_second_read_after_end", in.Open());
	FIFO dest;
	ASSERT_EQUAL("test_second_read_after_end", ToString(Status::Ok), ToString(in.Read(1, dest).status));
	ASSERT_EQUAL("test_second_read_after_end", std::string("A"), Text(dest));
	FIFO again("KEEP");
	const auto end = in.Read(1, again);
	ASSERT_EQUAL("test_second_read_after_end", ToString(Status::End), ToString(end.status));
	ASSERT_EQUAL("test_second_read_after_end", static_cast<std::size_t>(0), end.count);
	ASSERT_EQUAL("test_second_read_after_end", std::string("KEEP"), Text(again));
	ASSERT_FALSE("test_second_read_after_end", static_cast<bool>(in));
	RETURN_TEST("test_second_read_after_end", 0);
}

int test_empty_file() {
	BufferedFileReader in(File("empty.bin"));
	ASSERT_TRUE("test_empty_file", in.Open());
	const auto size = in.Size();
	ASSERT_TRUE("test_empty_file", size.has_value());
	ASSERT_EQUAL("test_empty_file", static_cast<std::size_t>(0), *size);
	FIFO dest("KEEP");
	const auto read = in.Read(1, dest);
	ASSERT_EQUAL("test_empty_file", ToString(Status::End), ToString(read.status));
	ASSERT_EQUAL("test_empty_file", static_cast<std::size_t>(0), read.count);
	ASSERT_EQUAL("test_empty_file", std::string("KEEP"), Text(dest));
	ASSERT_TRUE("test_empty_file", in.EoF());
	ASSERT_FALSE("test_empty_file", static_cast<bool>(in));
	RETURN_TEST("test_empty_file", 0);
}

int test_peek_does_not_consume() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_peek_does_not_consume", in.Open());
	FIFO peek;
	const auto peeked = in.Peek(3, peek);
	ASSERT_EQUAL("test_peek_does_not_consume", ToString(Status::Ok), ToString(peeked.status));
	ASSERT_EQUAL("test_peek_does_not_consume", std::string("ABC"), Text(peek));
	ASSERT_EQUAL("test_peek_does_not_consume", static_cast<std::size_t>(0), in.Tell());
	FIFO read;
	ASSERT_EQUAL("test_peek_does_not_consume", ToString(Status::Ok), ToString(in.Read(3, read).status));
	ASSERT_EQUAL("test_peek_does_not_consume", std::string("ABC"), Text(read));
	ASSERT_EQUAL("test_peek_does_not_consume", static_cast<std::size_t>(3), in.Tell());
	RETURN_TEST("test_peek_does_not_consume", 0);
}

int test_read_zero_serves_window() {
	BufferedFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE("test_read_zero_serves_window", in.Open());
	FIFO empty;
	const auto first = in.Read(0, empty);
	ASSERT_EQUAL("test_read_zero_serves_window", ToString(Status::Ok), ToString(first.status));
	ASSERT_EQUAL("test_read_zero_serves_window", static_cast<std::size_t>(0), first.count);
	FIFO chunk;
	ASSERT_EQUAL("test_read_zero_serves_window", ToString(Status::Ok), ToString(in.Read(2, chunk).status));
	ASSERT_EQUAL("test_read_zero_serves_window", std::string("AB"), Text(chunk));
	RETURN_TEST("test_read_zero_serves_window", 0);
}

int test_sequential_splits() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_sequential_splits", in.Open());
	FIFO a, b;
	ASSERT_EQUAL("test_sequential_splits", ToString(Status::Ok), ToString(in.Read(3, a).status));
	ASSERT_EQUAL("test_sequential_splits", ToString(Status::Ok), ToString(in.Read(2, b).status));
	ASSERT_EQUAL("test_sequential_splits", std::string("ABC"), Text(a));
	ASSERT_EQUAL("test_sequential_splits", std::string("DE"), Text(b));
	ASSERT_EQUAL("test_sequential_splits", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_sequential_splits", 0);
}

// -------------------
// Fixtures
// -------------------

int test_lines_are_octets() {
	BufferedFileReader in(File("lines.txt"));
	ASSERT_TRUE("test_lines_are_octets", in.Open());
	FIFO dest;
	const auto read = in.Read(18, dest);
	ASSERT_EQUAL("test_lines_are_octets", ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL("test_lines_are_octets", std::string("line1\nline2\nline3\n"), Text(dest));
	RETURN_TEST("test_lines_are_octets", 0);
}

int test_no_nl_and_nul() {
	BufferedFileReader text(File("no_nl.txt"));
	ASSERT_TRUE("test_no_nl_and_nul", text.Open());
	FIFO t;
	ASSERT_EQUAL("test_no_nl_and_nul", ToString(Status::Ok), ToString(text.Read(17, t).status));
	ASSERT_EQUAL("test_no_nl_and_nul", std::string("no newline at end"), Text(t));

	BufferedFileReader raw(File("nul.bin"));
	ASSERT_TRUE("test_no_nl_and_nul", raw.Open());
	FIFO n;
	ASSERT_EQUAL("test_no_nl_and_nul", ToString(Status::Ok), ToString(raw.Read(5, n).status));
	const auto bytes = Bytes(n);
	ASSERT_EQUAL("test_no_nl_and_nul", static_cast<std::size_t>(5), bytes.size());
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{'A'}, bytes[0]);
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{0}, bytes[1]);
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{'B'}, bytes[2]);
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{0}, bytes[3]);
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{'C'}, bytes[4]);
	RETURN_TEST("test_no_nl_and_nul", 0);
}

int test_pattern_256() {
	BufferedFileReader in(File("pattern_256.bin"));
	ASSERT_TRUE("test_pattern_256", in.Open());
	FIFO dest;
	const auto read = in.Read(256, dest);
	ASSERT_EQUAL("test_pattern_256", ToString(Status::Ok), ToString(read.status));
	const auto bytes = Bytes(dest);
	ASSERT_EQUAL("test_pattern_256", static_cast<std::size_t>(256), bytes.size());
	for (std::size_t i = 0; i < 256; ++i)
		ASSERT_EQUAL("test_pattern_256", static_cast<std::byte>(i), bytes[i]);
	RETURN_TEST("test_pattern_256", 0);
}

// -------------------
// Seek
// -------------------

int test_seek_absolute_and_relative() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_absolute_and_relative", in.Open());
	FIFO dest;
	ASSERT_EQUAL("test_seek_absolute_and_relative", ToString(Status::Ok), ToString(in.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_absolute_and_relative", static_cast<std::size_t>(4), in.Tell());
	ASSERT_EQUAL("test_seek_absolute_and_relative", ToString(Status::Ok), ToString(in.Read(2, dest).status));
	ASSERT_EQUAL("test_seek_absolute_and_relative", std::string("45"), Text(dest));
	ASSERT_EQUAL("test_seek_absolute_and_relative", ToString(Status::Ok), ToString(in.Seek(-2, Position::Relative).status));
	ASSERT_EQUAL("test_seek_absolute_and_relative", static_cast<std::size_t>(4), in.Tell());
	FIFO again;
	ASSERT_EQUAL("test_seek_absolute_and_relative", ToString(Status::Ok), ToString(in.Read(3, again).status));
	ASSERT_EQUAL("test_seek_absolute_and_relative", std::string("456"), Text(again));
	RETURN_TEST("test_seek_absolute_and_relative", 0);
}

int test_seek_negative_absolute_fails() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_negative_absolute_fails", in.Open());
	ASSERT_EQUAL("test_seek_negative_absolute_fails", ToString(Status::Failed), ToString(in.Seek(-1, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_negative_absolute_fails", static_cast<std::size_t>(0), in.Tell());
	RETURN_TEST("test_seek_negative_absolute_fails", 0);
}

int test_seek_without_open_fails() {
	BufferedFileReader in(File("seek.bin"));
	FIFO dest("KEEP");
	ASSERT_EQUAL("test_seek_without_open_fails", ToString(Status::Failed), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_without_open_fails", static_cast<std::size_t>(0), in.Tell());
	ASSERT_EQUAL("test_seek_without_open_fails", ToString(Status::Failed), ToString(in.Read(1, dest).status));
	ASSERT_EQUAL("test_seek_without_open_fails", std::string("KEEP"), Text(dest));
	RETURN_TEST("test_seek_without_open_fails", 0);
}

int test_seek_relative_before_start_fails() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_relative_before_start_fails", in.Open());
	ASSERT_EQUAL("test_seek_relative_before_start_fails", ToString(Status::Ok), ToString(in.Seek(2, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_relative_before_start_fails", ToString(Status::Failed), ToString(in.Seek(-3, Position::Relative).status));
	ASSERT_EQUAL("test_seek_relative_before_start_fails", static_cast<std::size_t>(2), in.Tell());
	RETURN_TEST("test_seek_relative_before_start_fails", 0);
}

int test_seek_zero_rereads() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_zero_rereads", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_seek_zero_rereads", ToString(Status::Ok), ToString(in.Read(4, first).status));
	ASSERT_EQUAL("test_seek_zero_rereads", std::string("0123"), Text(first));
	ASSERT_EQUAL("test_seek_zero_rereads", ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_zero_rereads", static_cast<std::size_t>(0), in.Tell());
	FIFO again;
	ASSERT_EQUAL("test_seek_zero_rereads", ToString(Status::Ok), ToString(in.Read(4, again).status));
	ASSERT_EQUAL("test_seek_zero_rereads", std::string("0123"), Text(again));
	RETURN_TEST("test_seek_zero_rereads", 0);
}

int test_seek_end_via_size() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_end_via_size", in.Open());
	const auto size = in.Size();
	ASSERT_TRUE("test_seek_end_via_size", size.has_value());
	ASSERT_EQUAL("test_seek_end_via_size", static_cast<std::size_t>(10), *size);

	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size), Position::Absolute).status));
	ASSERT_EQUAL("test_seek_end_via_size", *size, in.Tell());
	FIFO dest("KEEP");
	const auto end = in.Read(1, dest);
	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::End), ToString(end.status));
	ASSERT_EQUAL("test_seek_end_via_size", static_cast<std::size_t>(0), end.count);
	ASSERT_EQUAL("test_seek_end_via_size", std::string("KEEP"), Text(dest));

	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size) - 2, Position::Absolute).status));
	FIFO last;
	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::Ok), ToString(in.Read(2, last).status));
	ASSERT_EQUAL("test_seek_end_via_size", std::string("89"), Text(last));
	RETURN_TEST("test_seek_end_via_size", 0);
}

int test_seek_hits_readahead_then_realigns() {
	BufferedFileReader in(File("seek.bin"), 8, 64);
	ASSERT_TRUE("test_seek_hits_readahead_then_realigns", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok), ToString(in.Read(2, first).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", std::string("01"), Text(first));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok),
		ToString(in.Seek(6, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", static_cast<std::size_t>(6), in.Tell());
	FIFO hit;
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok), ToString(in.Read(2, hit).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", std::string("67"), Text(hit));

	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok),
		ToString(in.Seek(0, Position::Absolute).status));
	FIFO start;
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok), ToString(in.Read(3, start).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", std::string("012"), Text(start));

	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok),
		ToString(in.Seek(8, Position::Absolute).status));
	FIFO tail;
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok), ToString(in.Read(2, tail).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", std::string("89"), Text(tail));
	RETURN_TEST("test_seek_hits_readahead_then_realigns", 0);
}

// -------------------
// Policy / volume
// -------------------

int test_readahead_knobs() {
	BufferedFileReader in(File("ahead.bin"), 25, 1024);
	ASSERT_EQUAL("test_readahead_knobs", static_cast<std::size_t>(25), in.ReadAhead());
	ASSERT_EQUAL("test_readahead_knobs", static_cast<std::size_t>(1024), in.MaxMemory());
	in.ReadAhead(8);
	in.MaxMemory(64);
	ASSERT_EQUAL("test_readahead_knobs", static_cast<std::size_t>(8), in.ReadAhead());
	ASSERT_EQUAL("test_readahead_knobs", static_cast<std::size_t>(64), in.MaxMemory());
	ASSERT_TRUE("test_readahead_knobs", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_readahead_knobs", ToString(Status::Ok), ToString(in.Read(5, first).status));
	ASSERT_EQUAL("test_readahead_knobs", std::string("01234"), Text(first));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	FIFO second;
	ASSERT_EQUAL("test_readahead_knobs", ToString(Status::Ok), ToString(in.Read(5, second).status));
	ASSERT_EQUAL("test_readahead_knobs", std::string("56789"), Text(second));
	RETURN_TEST("test_readahead_knobs", 0);
}

int test_max_memory_zero_still_reads() {
	BufferedFileReader in(File("block_256.bin"), 0, 0);
	ASSERT_TRUE("test_max_memory_zero_still_reads", in.Open());
	FIFO dest;
	const auto read = in.Read(256, dest);
	ASSERT_EQUAL("test_max_memory_zero_still_reads", ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL("test_max_memory_zero_still_reads", static_cast<std::size_t>(256), read.count);
	RETURN_TEST("test_max_memory_zero_still_reads", 0);
}

int test_block_4k_chunked() {
	BufferedFileReader in(File("block_4k.bin"), 512, 2048);
	ASSERT_TRUE("test_block_4k_chunked", in.Open());
	std::size_t total = 0;
	while (!in.EoF()) {
		FIFO dest;
		const auto read = in.Read(1000, dest);
		if (read.status == Status::Failed || read.status == Status::Error)
			return 1;
		total += read.count;
		if (read.status == Status::End)
			break;
	}
	ASSERT_EQUAL("test_block_4k_chunked", static_cast<std::size_t>(4096), total);
	RETURN_TEST("test_block_4k_chunked", 0);
}

// -------------------
// Move
// -------------------

int test_move_transfers_session() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_move_transfers_session", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_move_transfers_session", ToString(Status::Ok), ToString(in.Read(2, first).status));
	BufferedFileReader moved(std::move(in));
	ASSERT_FALSE("test_move_transfers_session", static_cast<bool>(in));
	ASSERT_EQUAL("test_move_transfers_session", ToString(State::Unavailable), ToString(in.State()));
	FIFO rest;
	const auto read = moved.Read(3, rest);
	ASSERT_EQUAL("test_move_transfers_session", ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL("test_move_transfers_session", std::string("CDE"), Text(rest));
	RETURN_TEST("test_move_transfers_session", 0);
}

int main() {
	int result = 0;

	result += test_ctor_unavailable_bool_false();
	result += test_open_missing();
	result += test_open_directory();
	result += test_rewind_without_open_fails();
	result += test_read_before_open_leaves_dest();
	result += test_open_not_idempotent();
	result += test_close_then_reopen();
	result += test_rewind_rereads();

	result += test_open_five_read_exact();
	result += test_read_overwrites_dest();
	result += test_read_past_end_is_short();
	result += test_second_read_after_end();
	result += test_empty_file();
	result += test_peek_does_not_consume();
	result += test_read_zero_serves_window();
	result += test_sequential_splits();

	result += test_lines_are_octets();
	result += test_no_nl_and_nul();
	result += test_pattern_256();

	result += test_seek_absolute_and_relative();
	result += test_seek_negative_absolute_fails();
	result += test_seek_without_open_fails();
	result += test_seek_relative_before_start_fails();
	result += test_seek_zero_rereads();
	result += test_seek_end_via_size();
	result += test_seek_hits_readahead_then_realigns();

	result += test_readahead_knobs();
	result += test_max_memory_zero_still_reads();
	result += test_block_4k_chunked();

	result += test_move_transfers_session();

	if (result == 0)
		std::cout << "BufferedFileReader tests passed!" << std::endl;
	else
		std::cout << result << " BufferedFileReader tests failed." << std::endl;
	return result;
}
