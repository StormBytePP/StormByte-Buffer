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

static void Fill(std::span<std::byte> dest, const std::byte value) {
	for (auto& b : dest)
		b = value;
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
	ASSERT_EQUAL("test_ctor_unavailable_bool_false", static_cast<std::size_t>(0), in.Tell());
	ASSERT_EQUAL("test_ctor_unavailable_bool_false", static_cast<std::size_t>(0), in.ReadAhead());
	ASSERT_EQUAL("test_ctor_unavailable_bool_false", File("five.bin"), in.Path());
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
	ASSERT_EQUAL("test_read_before_open_leaves_dest", static_cast<std::size_t>(0), in.Tell());
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
	ASSERT_EQUAL("test_close_then_reopen", static_cast<std::size_t>(0), in.Tell());
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
	ASSERT_EQUAL("test_rewind_rereads", static_cast<std::size_t>(5), in.Tell());
	ASSERT_TRUE("test_rewind_rereads", in.Rewind());
	ASSERT_EQUAL("test_rewind_rereads", ToString(State::Idle), ToString(in.State()));
	ASSERT_EQUAL("test_rewind_rereads", static_cast<std::size_t>(0), in.Tell());
	FIFO second;
	ASSERT_EQUAL("test_rewind_rereads", ToString(Status::Ok), ToString(in.Read(5, second).status));
	ASSERT_EQUAL("test_rewind_rereads", std::string("ABCDE"), Text(second));
	ASSERT_EQUAL("test_rewind_rereads", static_cast<std::size_t>(5), in.Tell());
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
	ASSERT_EQUAL("test_read_past_end_is_short", static_cast<std::size_t>(5), in.Tell());
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
	ASSERT_EQUAL("test_second_read_after_end", static_cast<std::size_t>(1), in.Tell());
	FIFO again("KEEP");
	const auto end = in.Read(1, again);
	ASSERT_EQUAL("test_second_read_after_end", ToString(Status::End), ToString(end.status));
	ASSERT_EQUAL("test_second_read_after_end", static_cast<std::size_t>(0), end.count);
	ASSERT_EQUAL("test_second_read_after_end", std::string("KEEP"), Text(again));
	ASSERT_EQUAL("test_second_read_after_end", static_cast<std::size_t>(1), in.Tell());
	ASSERT_FALSE("test_second_read_after_end", static_cast<bool>(in));
	RETURN_TEST("test_second_read_after_end", 0);
}

int test_empty_file() {
	BufferedFileReader in(File("empty.bin"));
	ASSERT_TRUE("test_empty_file", in.Open());
	const auto size = in.Size();
	ASSERT_TRUE("test_empty_file", size.has_value());
	ASSERT_EQUAL("test_empty_file", static_cast<std::size_t>(0), *size);
	ASSERT_EQUAL("test_empty_file", static_cast<std::size_t>(0), in.Tell());
	FIFO dest("KEEP");
	const auto read = in.Read(1, dest);
	ASSERT_EQUAL("test_empty_file", ToString(Status::End), ToString(read.status));
	ASSERT_EQUAL("test_empty_file", static_cast<std::size_t>(0), read.count);
	ASSERT_EQUAL("test_empty_file", std::string("KEEP"), Text(dest));
	ASSERT_EQUAL("test_empty_file", static_cast<std::size_t>(0), in.Tell());
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
	FIFO dest;
	ASSERT_EQUAL("test_peek_does_not_consume", ToString(Status::Ok), ToString(in.Read(3, dest).status));
	ASSERT_EQUAL("test_peek_does_not_consume", std::string("ABC"), Text(dest));
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
	ASSERT_EQUAL("test_read_zero_serves_window", static_cast<std::size_t>(0), in.Tell());
	FIFO chunk;
	ASSERT_EQUAL("test_read_zero_serves_window", ToString(Status::Ok), ToString(in.Read(2, chunk).status));
	ASSERT_EQUAL("test_read_zero_serves_window", std::string("AB"), Text(chunk));
	ASSERT_EQUAL("test_read_zero_serves_window", static_cast<std::size_t>(2), in.Tell());
	RETURN_TEST("test_read_zero_serves_window", 0);
}

int test_sequential_splits() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_sequential_splits", in.Open());
	FIFO a, b;
	ASSERT_EQUAL("test_sequential_splits", ToString(Status::Ok), ToString(in.Read(3, a).status));
	ASSERT_EQUAL("test_sequential_splits", static_cast<std::size_t>(3), in.Tell());
	ASSERT_EQUAL("test_sequential_splits", ToString(Status::Ok), ToString(in.Read(2, b).status));
	ASSERT_EQUAL("test_sequential_splits", std::string("ABC"), Text(a));
	ASSERT_EQUAL("test_sequential_splits", std::string("DE"), Text(b));
	ASSERT_EQUAL("test_sequential_splits", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_sequential_splits", 0);
}

int test_read_span_exact() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_read_span_exact", in.Open());
	std::array<std::byte, 5> raw {};
	Fill(raw, std::byte{0x5A});
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL("test_read_span_exact", ToString(Status::Ok), ToString(got.status));
	ASSERT_EQUAL("test_read_span_exact", static_cast<std::size_t>(5), got.count);
	ASSERT_EQUAL("test_read_span_exact", std::string("ABCDE"),
		std::string(reinterpret_cast<const char*>(raw.data()), raw.size()));
	ASSERT_EQUAL("test_read_span_exact", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_read_span_exact", 0);
}

int test_peek_span_does_not_consume() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_peek_span_does_not_consume", in.Open());
	std::array<std::byte, 3> raw {};
	const auto peeked = in.Peek(std::span<std::byte>(raw));
	ASSERT_EQUAL("test_peek_span_does_not_consume", ToString(Status::Ok), ToString(peeked.status));
	ASSERT_EQUAL("test_peek_span_does_not_consume", std::string("ABC"),
		std::string(reinterpret_cast<const char*>(raw.data()), 3));
	ASSERT_EQUAL("test_peek_span_does_not_consume", static_cast<std::size_t>(0), in.Tell());
	FIFO dest;
	ASSERT_EQUAL("test_peek_span_does_not_consume", ToString(Status::Ok), ToString(in.Read(3, dest).status));
	ASSERT_EQUAL("test_peek_span_does_not_consume", std::string("ABC"), Text(dest));
	ASSERT_EQUAL("test_peek_span_does_not_consume", static_cast<std::size_t>(3), in.Tell());
	RETURN_TEST("test_peek_span_does_not_consume", 0);
}

int test_read_span_empty_ok() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_read_span_empty_ok", in.Open());
	const auto got = in.Read(std::span<std::byte>{});
	ASSERT_EQUAL("test_read_span_empty_ok", ToString(Status::Ok), ToString(got.status));
	ASSERT_EQUAL("test_read_span_empty_ok", static_cast<std::size_t>(0), got.count);
	ASSERT_EQUAL("test_read_span_empty_ok", static_cast<std::size_t>(0), in.Tell());
	FIFO dest;
	ASSERT_EQUAL("test_read_span_empty_ok", ToString(Status::Ok), ToString(in.Read(2, dest).status));
	ASSERT_EQUAL("test_read_span_empty_ok", std::string("AB"), Text(dest));
	ASSERT_EQUAL("test_read_span_empty_ok", static_cast<std::size_t>(2), in.Tell());
	RETURN_TEST("test_read_span_empty_ok", 0);
}

int test_read_span_short_leaves_tail() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_read_span_short_leaves_tail", in.Open());
	std::array<std::byte, 8> raw {};
	Fill(raw, std::byte{0x5A});
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL("test_read_span_short_leaves_tail", ToString(Status::End), ToString(got.status));
	ASSERT_EQUAL("test_read_span_short_leaves_tail", static_cast<std::size_t>(5), got.count);
	ASSERT_EQUAL("test_read_span_short_leaves_tail", std::string("ABCDE"),
		std::string(reinterpret_cast<const char*>(raw.data()), 5));
	ASSERT_EQUAL("test_read_span_short_leaves_tail", static_cast<unsigned>(0x5A),
		static_cast<unsigned>(raw[5]));
	ASSERT_EQUAL("test_read_span_short_leaves_tail", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_read_span_short_leaves_tail", 0);
}

int test_read_span_before_open_leaves_dest() {
	BufferedFileReader in(File("five.bin"));
	std::array<std::byte, 2> raw {};
	Fill(raw, std::byte{0x5A});
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL("test_read_span_before_open_leaves_dest", ToString(Status::Failed), ToString(got.status));
	ASSERT_EQUAL("test_read_span_before_open_leaves_dest", static_cast<std::size_t>(0), got.count);
	ASSERT_EQUAL("test_read_span_before_open_leaves_dest", static_cast<unsigned>(0x5A),
		static_cast<unsigned>(raw[0]));
	ASSERT_EQUAL("test_read_span_before_open_leaves_dest", static_cast<std::size_t>(0), in.Tell());
	RETURN_TEST("test_read_span_before_open_leaves_dest", 0);
}

// -------------------
// Tell
// -------------------

int test_tell_open_is_zero() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_EQUAL("test_tell_open_is_zero", static_cast<std::size_t>(0), in.Tell());
	ASSERT_TRUE("test_tell_open_is_zero", in.Open());
	ASSERT_EQUAL("test_tell_open_is_zero", static_cast<std::size_t>(0), in.Tell());
	RETURN_TEST("test_tell_open_is_zero", 0);
}

int test_tell_tracks_each_read() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_tell_tracks_each_read", in.Open());
	FIFO a, b, c;
	ASSERT_EQUAL("test_tell_tracks_each_read", static_cast<std::size_t>(1), in.Read(1, a).count);
	ASSERT_EQUAL("test_tell_tracks_each_read", static_cast<std::size_t>(1), in.Tell());
	ASSERT_EQUAL("test_tell_tracks_each_read", static_cast<std::size_t>(2), in.Read(2, b).count);
	ASSERT_EQUAL("test_tell_tracks_each_read", static_cast<std::size_t>(3), in.Tell());
	ASSERT_EQUAL("test_tell_tracks_each_read", static_cast<std::size_t>(2), in.Read(2, c).count);
	ASSERT_EQUAL("test_tell_tracks_each_read", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_tell_tracks_each_read", 0);
}

int test_tell_unchanged_on_peek_and_empty_span() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_tell_unchanged_on_peek_and_empty_span", in.Open());
	FIFO dest;
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", ToString(Status::Ok), ToString(in.Read(2, dest).status));
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", static_cast<std::size_t>(2), in.Tell());
	FIFO peek;
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", ToString(Status::Ok), ToString(in.Peek(2, peek).status));
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", static_cast<std::size_t>(2), in.Tell());
	std::array<std::byte, 2> raw {};
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", ToString(Status::Ok),
		ToString(in.Peek(std::span<std::byte>(raw)).status));
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", static_cast<std::size_t>(2), in.Tell());
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", ToString(Status::Ok),
		ToString(in.Read(std::span<std::byte>{}).status));
	ASSERT_EQUAL("test_tell_unchanged_on_peek_and_empty_span", static_cast<std::size_t>(2), in.Tell());
	RETURN_TEST("test_tell_unchanged_on_peek_and_empty_span", 0);
}

int test_tell_matches_seek_and_failed_seek_stays() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_tell_matches_seek_and_failed_seek_stays", in.Open());
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", ToString(Status::Ok),
		ToString(in.Seek(7, Position::Absolute).status));
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", static_cast<std::size_t>(7), in.Tell());
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", ToString(Status::Ok),
		ToString(in.Seek(-3, Position::Relative).status));
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", static_cast<std::size_t>(4), in.Tell());
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", ToString(Status::Failed),
		ToString(in.Seek(-10, Position::Relative).status));
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", static_cast<std::size_t>(4), in.Tell());
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", ToString(Status::Failed),
		ToString(in.Seek(-1, Position::Absolute).status));
	ASSERT_EQUAL("test_tell_matches_seek_and_failed_seek_stays", static_cast<std::size_t>(4), in.Tell());
	RETURN_TEST("test_tell_matches_seek_and_failed_seek_stays", 0);
}

int test_tell_at_size_and_past_size() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_tell_at_size_and_past_size", in.Open());
	const auto size = in.Size();
	ASSERT_TRUE("test_tell_at_size_and_past_size", size.has_value());
	ASSERT_EQUAL("test_tell_at_size_and_past_size", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size), Position::Absolute).status));
	ASSERT_EQUAL("test_tell_at_size_and_past_size", *size, in.Tell());
	FIFO dest("KEEP");
	ASSERT_EQUAL("test_tell_at_size_and_past_size", static_cast<std::size_t>(0), in.Read(3, dest).count);
	ASSERT_EQUAL("test_tell_at_size_and_past_size", *size, in.Tell());
	ASSERT_EQUAL("test_tell_at_size_and_past_size", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size + 6), Position::Absolute).status));
	ASSERT_EQUAL("test_tell_at_size_and_past_size", *size + 6, in.Tell());
	ASSERT_EQUAL("test_tell_at_size_and_past_size", static_cast<std::size_t>(0), in.Read(1, dest).count);
	ASSERT_EQUAL("test_tell_at_size_and_past_size", *size + 6, in.Tell());
	RETURN_TEST("test_tell_at_size_and_past_size", 0);
}

int test_tell_after_span_read_and_seek_zero() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_tell_after_span_read_and_seek_zero", in.Open());
	std::array<std::byte, 3> raw {};
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", static_cast<std::size_t>(3),
		in.Read(std::span<std::byte>(raw)).count);
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", static_cast<std::size_t>(3), in.Tell());
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", ToString(Status::Ok),
		ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", static_cast<std::size_t>(0), in.Tell());
	ASSERT_FALSE("test_tell_after_span_read_and_seek_zero", in.EoF());
	FIFO dest;
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL("test_tell_after_span_read_and_seek_zero", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_tell_after_span_read_and_seek_zero", 0);
}

int test_tell_max_memory_zero_seek_read() {
	BufferedFileReader in(File("seek.bin"), 0, 0);
	ASSERT_TRUE("test_tell_max_memory_zero_seek_read", in.Open());
	ASSERT_EQUAL("test_tell_max_memory_zero_seek_read", static_cast<std::size_t>(0), in.Tell());
	ASSERT_EQUAL("test_tell_max_memory_zero_seek_read", ToString(Status::Ok),
		ToString(in.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL("test_tell_max_memory_zero_seek_read", static_cast<std::size_t>(4), in.Tell());
	FIFO dest;
	ASSERT_EQUAL("test_tell_max_memory_zero_seek_read", static_cast<std::size_t>(2), in.Read(2, dest).count);
	ASSERT_EQUAL("test_tell_max_memory_zero_seek_read", std::string("45"), Text(dest));
	ASSERT_EQUAL("test_tell_max_memory_zero_seek_read", static_cast<std::size_t>(6), in.Tell());
	RETURN_TEST("test_tell_max_memory_zero_seek_read", 0);
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
	ASSERT_EQUAL("test_lines_are_octets", static_cast<std::size_t>(18), in.Tell());
	RETURN_TEST("test_lines_are_octets", 0);
}

int test_no_nl_and_nul() {
	BufferedFileReader text(File("no_nl.txt"));
	ASSERT_TRUE("test_no_nl_and_nul", text.Open());
	FIFO t;
	ASSERT_EQUAL("test_no_nl_and_nul", ToString(Status::Ok), ToString(text.Read(17, t).status));
	ASSERT_EQUAL("test_no_nl_and_nul", std::string("no newline at end"), Text(t));
	ASSERT_EQUAL("test_no_nl_and_nul", static_cast<std::size_t>(17), text.Tell());

	BufferedFileReader raw(File("with_nuls.bin"));
	ASSERT_TRUE("test_no_nl_and_nul", raw.Open());
	FIFO n;
	ASSERT_EQUAL("test_no_nl_and_nul", ToString(Status::Ok), ToString(raw.Read(5, n).status));
	const auto bytes = Bytes(n);
	ASSERT_EQUAL("test_no_nl_and_nul", static_cast<std::size_t>(5), bytes.size());
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{'A'}, bytes[0]);
	ASSERT_EQUAL("test_no_nl_and_nul", std::byte{0}, bytes[1]);
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
	ASSERT_EQUAL("test_pattern_256", static_cast<std::size_t>(256), in.Tell());
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
	ASSERT_EQUAL("test_seek_absolute_and_relative", static_cast<std::size_t>(6), in.Tell());
	ASSERT_EQUAL("test_seek_absolute_and_relative", ToString(Status::Ok), ToString(in.Seek(-2, Position::Relative).status));
	ASSERT_EQUAL("test_seek_absolute_and_relative", static_cast<std::size_t>(4), in.Tell());
	FIFO again;
	ASSERT_EQUAL("test_seek_absolute_and_relative", ToString(Status::Ok), ToString(in.Read(3, again).status));
	ASSERT_EQUAL("test_seek_absolute_and_relative", std::string("456"), Text(again));
	ASSERT_EQUAL("test_seek_absolute_and_relative", static_cast<std::size_t>(7), in.Tell());
	RETURN_TEST("test_seek_absolute_and_relative", 0);
}

int test_seek_negative_absolute_fails() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_negative_absolute_fails", in.Open());
	ASSERT_EQUAL("test_seek_negative_absolute_fails", ToString(Status::Failed), ToString(in.Seek(-1, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_negative_absolute_fails", static_cast<std::size_t>(0), in.Tell());
	RETURN_TEST("test_seek_negative_absolute_fails", 0);
}

int test_seek_relative_before_zero_fails() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_relative_before_zero_fails", in.Open());
	ASSERT_EQUAL("test_seek_relative_before_zero_fails", ToString(Status::Failed),
		ToString(in.Seek(-1, Position::Relative).status));
	ASSERT_EQUAL("test_seek_relative_before_zero_fails", static_cast<std::size_t>(0), in.Tell());
	RETURN_TEST("test_seek_relative_before_zero_fails", 0);
}

int test_seek_without_open_fails() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_EQUAL("test_seek_without_open_fails", ToString(Status::Failed),
		ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_without_open_fails", static_cast<std::size_t>(0), in.Tell());
	RETURN_TEST("test_seek_without_open_fails", 0);
}

int test_seek_zero_on_fresh_open() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_seek_zero_on_fresh_open", in.Open());
	ASSERT_EQUAL("test_seek_zero_on_fresh_open", ToString(Status::Ok),
		ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_zero_on_fresh_open", static_cast<std::size_t>(0), in.Tell());
	ASSERT_FALSE("test_seek_zero_on_fresh_open", in.EoF());
	ASSERT_TRUE("test_seek_zero_on_fresh_open", static_cast<bool>(in));
	ASSERT_EQUAL("test_seek_zero_on_fresh_open", ToString(State::Idle), ToString(in.State()));
	FIFO dest;
	ASSERT_EQUAL("test_seek_zero_on_fresh_open", ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL("test_seek_zero_on_fresh_open", std::string("ABCDE"), Text(dest));
	RETURN_TEST("test_seek_zero_on_fresh_open", 0);
}

int test_seek_zero_rereads() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_zero_rereads", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_seek_zero_rereads", ToString(Status::Ok), ToString(in.Read(4, first).status));
	ASSERT_EQUAL("test_seek_zero_rereads", static_cast<std::size_t>(4), in.Tell());
	ASSERT_EQUAL("test_seek_zero_rereads", ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_zero_rereads", static_cast<std::size_t>(0), in.Tell());
	ASSERT_FALSE("test_seek_zero_rereads", in.EoF());
	ASSERT_TRUE("test_seek_zero_rereads", static_cast<bool>(in));
	FIFO again;
	ASSERT_EQUAL("test_seek_zero_rereads", ToString(Status::Ok), ToString(in.Read(4, again).status));
	ASSERT_EQUAL("test_seek_zero_rereads", Text(first), Text(again));
	ASSERT_EQUAL("test_seek_zero_rereads", static_cast<std::size_t>(4), in.Tell());
	RETURN_TEST("test_seek_zero_rereads", 0);
}

int test_seek_zero_after_end_clears_eof() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_seek_zero_after_end_clears_eof", in.Open());
	FIFO first;
	const auto drain = in.Read(16, first);
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", ToString(Status::End), ToString(drain.status));
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", static_cast<std::size_t>(5), drain.count);
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", std::string("ABCDE"), Text(first));
	ASSERT_TRUE("test_seek_zero_after_end_clears_eof", in.EoF());
	ASSERT_FALSE("test_seek_zero_after_end_clears_eof", static_cast<bool>(in));
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", ToString(State::Idle), ToString(in.State()));

	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", ToString(Status::Ok),
		ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", static_cast<std::size_t>(0), in.Tell());
	ASSERT_FALSE("test_seek_zero_after_end_clears_eof", in.EoF());
	ASSERT_TRUE("test_seek_zero_after_end_clears_eof", static_cast<bool>(in));
	ASSERT_TRUE("test_seek_zero_after_end_clears_eof", in.IsReadable());
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", ToString(State::Idle), ToString(in.State()));

	FIFO second;
	const auto again = in.Read(16, second);
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", ToString(Status::End), ToString(again.status));
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", static_cast<std::size_t>(5), again.count);
	ASSERT_EQUAL("test_seek_zero_after_end_clears_eof", std::string("ABCDE"), Text(second));
	ASSERT_TRUE("test_seek_zero_after_end_clears_eof", in.EoF());
	RETURN_TEST("test_seek_zero_after_end_clears_eof", 0);
}

int test_seek_zero_after_end_twice() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_seek_zero_after_end_twice", in.Open());
	for (int pass = 0; pass < 2; ++pass) {
		FIFO dest;
		const auto got = in.Read(8, dest);
		ASSERT_EQUAL("test_seek_zero_after_end_twice", ToString(Status::End), ToString(got.status));
		ASSERT_EQUAL("test_seek_zero_after_end_twice", static_cast<std::size_t>(5), got.count);
		ASSERT_EQUAL("test_seek_zero_after_end_twice", std::string("ABCDE"), Text(dest));
		ASSERT_TRUE("test_seek_zero_after_end_twice", in.EoF());
		ASSERT_EQUAL("test_seek_zero_after_end_twice", ToString(Status::Ok),
			ToString(in.Seek(0, Position::Absolute).status));
		ASSERT_FALSE("test_seek_zero_after_end_twice", in.EoF());
		ASSERT_EQUAL("test_seek_zero_after_end_twice", static_cast<std::size_t>(0), in.Tell());
	}
	FIFO last;
	ASSERT_EQUAL("test_seek_zero_after_end_twice", ToString(Status::Ok), ToString(in.Read(5, last).status));
	ASSERT_EQUAL("test_seek_zero_after_end_twice", std::string("ABCDE"), Text(last));
	RETURN_TEST("test_seek_zero_after_end_twice", 0);
}

int test_seek_end_via_size() {
	BufferedFileReader in(File("seek.bin"));
	ASSERT_TRUE("test_seek_end_via_size", in.Open());
	ASSERT_TRUE("test_seek_end_via_size", in.IsSized());
	const auto size = in.Size();
	ASSERT_TRUE("test_seek_end_via_size", size.has_value());
	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size), Position::Absolute).status));
	ASSERT_EQUAL("test_seek_end_via_size", *size, in.Tell());
	FIFO dest("KEEP");
	const auto end = in.Read(1, dest);
	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::End), ToString(end.status));
	ASSERT_EQUAL("test_seek_end_via_size", static_cast<std::size_t>(0), end.count);
	ASSERT_EQUAL("test_seek_end_via_size", std::string("KEEP"), Text(dest));
	ASSERT_EQUAL("test_seek_end_via_size", *size, in.Tell());
	ASSERT_TRUE("test_seek_end_via_size", in.EoF());
	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size) - 2, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_end_via_size", *size - 2, in.Tell());
	ASSERT_FALSE("test_seek_end_via_size", in.EoF());
	ASSERT_TRUE("test_seek_end_via_size", static_cast<bool>(in));
	FIFO tail;
	ASSERT_EQUAL("test_seek_end_via_size", ToString(Status::Ok), ToString(in.Read(2, tail).status));
	ASSERT_EQUAL("test_seek_end_via_size", static_cast<std::size_t>(2), tail.AvailableBytes());
	ASSERT_EQUAL("test_seek_end_via_size", *size, in.Tell());
	RETURN_TEST("test_seek_end_via_size", 0);
}

int test_seek_past_size_then_read() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_seek_past_size_then_read", in.Open());
	const auto size = in.Size();
	ASSERT_TRUE("test_seek_past_size_then_read", size.has_value());
	ASSERT_EQUAL("test_seek_past_size_then_read", ToString(Status::Ok),
		ToString(in.Seek(static_cast<std::ptrdiff_t>(*size + 8), Position::Absolute).status));
	ASSERT_EQUAL("test_seek_past_size_then_read", *size + 8, in.Tell());
	FIFO dest("KEEP");
	const auto got = in.Read(4, dest);
	ASSERT_EQUAL("test_seek_past_size_then_read", ToString(Status::End), ToString(got.status));
	ASSERT_EQUAL("test_seek_past_size_then_read", static_cast<std::size_t>(0), got.count);
	ASSERT_EQUAL("test_seek_past_size_then_read", std::string("KEEP"), Text(dest));
	ASSERT_EQUAL("test_seek_past_size_then_read", *size + 8, in.Tell());
	RETURN_TEST("test_seek_past_size_then_read", 0);
}

int test_seek_hits_readahead_then_realigns() {
	BufferedFileReader in(File("ahead.bin"), 16, 64);
	ASSERT_TRUE("test_seek_hits_readahead_then_realigns", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok), ToString(in.Read(2, first).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", static_cast<std::size_t>(2), in.Tell());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok),
		ToString(in.Seek(6, Position::Absolute).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", static_cast<std::size_t>(6), in.Tell());
	ASSERT_FALSE("test_seek_hits_readahead_then_realigns", in.EoF());
	FIFO mid;
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", ToString(Status::Ok), ToString(in.Read(4, mid).status));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", std::string("6789"), Text(mid));
	ASSERT_EQUAL("test_seek_hits_readahead_then_realigns", static_cast<std::size_t>(10), in.Tell());
	RETURN_TEST("test_seek_hits_readahead_then_realigns", 0);
}

int test_peek_window_survives_seek() {
	BufferedFileReader in(File("ahead.bin"), 16, 64);
	ASSERT_TRUE("test_peek_window_survives_seek", in.Open());
	FIFO window;
	ASSERT_EQUAL("test_peek_window_survives_seek", ToString(Status::Ok), ToString(in.Peek(10, window).status));
	ASSERT_EQUAL("test_peek_window_survives_seek", std::string("0123456789"), Text(window));
	ASSERT_EQUAL("test_peek_window_survives_seek", static_cast<std::size_t>(0), in.Tell());

	ASSERT_EQUAL("test_peek_window_survives_seek", ToString(Status::Ok),
		ToString(in.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL("test_peek_window_survives_seek", static_cast<std::size_t>(4), in.Tell());
	ASSERT_FALSE("test_peek_window_survives_seek", in.EoF());
	FIFO mid;
	ASSERT_EQUAL("test_peek_window_survives_seek", ToString(Status::Ok), ToString(in.Peek(4, mid).status));
	ASSERT_EQUAL("test_peek_window_survives_seek", std::string("4567"), Text(mid));
	ASSERT_EQUAL("test_peek_window_survives_seek", static_cast<std::size_t>(4), in.Tell());

	ASSERT_EQUAL("test_peek_window_survives_seek", ToString(Status::Ok),
		ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL("test_peek_window_survives_seek", static_cast<std::size_t>(0), in.Tell());
	ASSERT_FALSE("test_peek_window_survives_seek", in.EoF());
	FIFO again;
	ASSERT_EQUAL("test_peek_window_survives_seek", ToString(Status::Ok), ToString(in.Peek(6, again).status));
	ASSERT_EQUAL("test_peek_window_survives_seek", std::string("012345"), Text(again));
	FIFO consumed;
	ASSERT_EQUAL("test_peek_window_survives_seek", ToString(Status::Ok), ToString(in.Read(6, consumed).status));
	ASSERT_EQUAL("test_peek_window_survives_seek", std::string("012345"), Text(consumed));
	ASSERT_EQUAL("test_peek_window_survives_seek", static_cast<std::size_t>(6), in.Tell());
	RETURN_TEST("test_peek_window_survives_seek", 0);
}

// -------------------
// Policy / volume
// -------------------

int test_readahead_knobs() {
	BufferedFileReader in(File("ahead.bin"), 25, 1024);
	ASSERT_EQUAL("test_readahead_knobs", static_cast<std::size_t>(25), in.ReadAhead());
	ASSERT_EQUAL("test_readahead_knobs", static_cast<std::size_t>(1024), in.MaxMemory());
	RETURN_TEST("test_readahead_knobs", 0);
}

int test_max_memory_zero_still_reads() {
	BufferedFileReader in(File("block_256.bin"), 0, 0);
	ASSERT_TRUE("test_max_memory_zero_still_reads", in.Open());
	FIFO dest;
	const auto read = in.Read(256, dest);
	ASSERT_EQUAL("test_max_memory_zero_still_reads", ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL("test_max_memory_zero_still_reads", static_cast<std::size_t>(256), read.count);
	ASSERT_EQUAL("test_max_memory_zero_still_reads", static_cast<std::size_t>(256), in.Tell());
	RETURN_TEST("test_max_memory_zero_still_reads", 0);
}

int test_max_memory_zero_span_reads() {
	BufferedFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE("test_max_memory_zero_span_reads", in.Open());
	std::array<std::byte, 5> raw {};
	const auto got = in.Read(std::span<std::byte>(raw));
	ASSERT_EQUAL("test_max_memory_zero_span_reads", ToString(Status::Ok), ToString(got.status));
	ASSERT_EQUAL("test_max_memory_zero_span_reads", std::string("ABCDE"),
		std::string(reinterpret_cast<const char*>(raw.data()), 5));
	ASSERT_EQUAL("test_max_memory_zero_span_reads", static_cast<std::size_t>(5), in.Tell());
	RETURN_TEST("test_max_memory_zero_span_reads", 0);
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
		ASSERT_EQUAL("test_block_4k_chunked", total, in.Tell());
		if (read.status == Status::End)
			break;
	}
	ASSERT_EQUAL("test_block_4k_chunked", static_cast<std::size_t>(4096), total);
	RETURN_TEST("test_block_4k_chunked", 0);
}

// -------------------
// Path-only ctor / Setup
// -------------------

int test_path_only_setup_sets_readahead() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_EQUAL("test_path_only_setup_sets_readahead", static_cast<std::size_t>(0), in.ReadAhead());
	ASSERT_TRUE("test_path_only_setup_sets_readahead", in.Open());
	ASSERT_TRUE("test_path_only_setup_sets_readahead", in.ReadAhead() >= 16ull * 1024ull);
	ASSERT_TRUE("test_path_only_setup_sets_readahead", in.ReadAhead() <= 1024ull * 1024ull);
	ASSERT_EQUAL("test_path_only_setup_sets_readahead", static_cast<std::size_t>(1024ull * 1024ull), in.MaxMemory());
	FIFO dest;
	ASSERT_EQUAL("test_path_only_setup_sets_readahead", ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL("test_path_only_setup_sets_readahead", std::string("ABCDE"), Text(dest));
	RETURN_TEST("test_path_only_setup_sets_readahead", 0);
}

int test_explicit_zero_survives_open() {
	BufferedFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE("test_explicit_zero_survives_open", in.Open());
	ASSERT_EQUAL("test_explicit_zero_survives_open", static_cast<std::size_t>(0), in.ReadAhead());
	ASSERT_EQUAL("test_explicit_zero_survives_open", static_cast<std::size_t>(0), in.MaxMemory());
	FIFO dest;
	ASSERT_EQUAL("test_explicit_zero_survives_open", ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL("test_explicit_zero_survives_open", std::string("ABCDE"), Text(dest));
	RETURN_TEST("test_explicit_zero_survives_open", 0);
}

int test_explicit_readahead_survives_setup() {
	BufferedFileReader in(File("ahead.bin"), 25, 1024);
	ASSERT_TRUE("test_explicit_readahead_survives_setup", in.Open());
	ASSERT_EQUAL("test_explicit_readahead_survives_setup", static_cast<std::size_t>(25), in.ReadAhead());
	ASSERT_EQUAL("test_explicit_readahead_survives_setup", static_cast<std::size_t>(1024), in.MaxMemory());
	FIFO dest;
	ASSERT_EQUAL("test_explicit_readahead_survives_setup", ToString(Status::Ok), ToString(in.Read(5, dest).status));
	ASSERT_EQUAL("test_explicit_readahead_survives_setup", std::string("01234"), Text(dest));
	RETURN_TEST("test_explicit_readahead_survives_setup", 0);
}

// -------------------
// Move
// -------------------

int test_move_transfers_session() {
	BufferedFileReader in(File("five.bin"));
	ASSERT_TRUE("test_move_transfers_session", in.Open());
	FIFO first;
	ASSERT_EQUAL("test_move_transfers_session", ToString(Status::Ok), ToString(in.Read(2, first).status));
	ASSERT_EQUAL("test_move_transfers_session", static_cast<std::size_t>(2), in.Tell());
	BufferedFileReader moved(std::move(in));
	ASSERT_TRUE("test_move_transfers_session", moved.IsOpen());
	ASSERT_EQUAL("test_move_transfers_session", static_cast<std::size_t>(2), moved.Tell());
	FIFO rest;
	ASSERT_EQUAL("test_move_transfers_session", ToString(Status::Ok), ToString(moved.Read(3, rest).status));
	ASSERT_EQUAL("test_move_transfers_session", std::string("CDE"), Text(rest));
	ASSERT_EQUAL("test_move_transfers_session", static_cast<std::size_t>(5), moved.Tell());
	RETURN_TEST("test_move_transfers_session", 0);
}

int main() {
	int result = 0;

	// -------------------
	// Session / errors
	// -------------------
	result += test_ctor_unavailable_bool_false();
	result += test_open_missing();
	result += test_open_directory();
	result += test_rewind_without_open_fails();
	result += test_read_before_open_leaves_dest();
	result += test_open_not_idempotent();
	result += test_close_then_reopen();
	result += test_rewind_rereads();

	// -------------------
	// Read / Peek
	// -------------------
	result += test_open_five_read_exact();
	result += test_read_overwrites_dest();
	result += test_read_past_end_is_short();
	result += test_second_read_after_end();
	result += test_empty_file();
	result += test_peek_does_not_consume();
	result += test_read_zero_serves_window();
	result += test_sequential_splits();
	result += test_read_span_exact();
	result += test_peek_span_does_not_consume();
	result += test_read_span_empty_ok();
	result += test_read_span_short_leaves_tail();
	result += test_read_span_before_open_leaves_dest();

	// -------------------
	// Tell
	// -------------------
	result += test_tell_open_is_zero();
	result += test_tell_tracks_each_read();
	result += test_tell_unchanged_on_peek_and_empty_span();
	result += test_tell_matches_seek_and_failed_seek_stays();
	result += test_tell_at_size_and_past_size();
	result += test_tell_after_span_read_and_seek_zero();
	result += test_tell_max_memory_zero_seek_read();

	// -------------------
	// Fixtures
	// -------------------
	result += test_lines_are_octets();
	result += test_no_nl_and_nul();
	result += test_pattern_256();

	// -------------------
	// Seek
	// -------------------
	result += test_seek_absolute_and_relative();
	result += test_seek_negative_absolute_fails();
	result += test_seek_relative_before_zero_fails();
	result += test_seek_without_open_fails();
	result += test_seek_zero_on_fresh_open();
	result += test_seek_zero_rereads();
	result += test_seek_zero_after_end_clears_eof();
	result += test_seek_zero_after_end_twice();
	result += test_seek_end_via_size();
	result += test_seek_past_size_then_read();
	result += test_seek_hits_readahead_then_realigns();
	result += test_peek_window_survives_seek();

	// -------------------
	// Policy / volume
	// -------------------
	result += test_readahead_knobs();
	result += test_max_memory_zero_still_reads();
	result += test_max_memory_zero_span_reads();
	result += test_block_4k_chunked();

	// -------------------
	// Path-only ctor / Setup
	// -------------------
	result += test_path_only_setup_sets_readahead();
	result += test_explicit_zero_survives_open();
	result += test_explicit_readahead_survives_setup();

	// -------------------
	// Move
	// -------------------
	result += test_move_transfers_session();

	if (result == 0)
		std::cout << "BufferedFileReader tests passed!" << std::endl;
	else
		std::cout << result << " BufferedFileReader tests failed." << std::endl;
	return result;
}
