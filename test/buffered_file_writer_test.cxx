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

#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/system.hxx>
#include <StormByte/test_handlers.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using StormByte::Buffer::DataType;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::IO::BufferedFileWriter;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::IO::ToString;
using StormByte::System::TempFileName;

static std::filesystem::path Scratch(const char* tag) {
	return std::filesystem::path(TempFileName(std::string("sbw_") + tag));
}

static std::string Slurp(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::in | std::ios::binary);
	if (!in)
		return {};
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static FIFO FromText(const std::string& text) {
	FIFO fifo;
	DataType data(text.size());
	for (std::size_t i = 0; i < text.size(); ++i)
		data[i] = static_cast<std::byte>(text[i]);
	static_cast<void>(fifo.Write(data.size(), std::move(data)));
	return fifo;
}

static bool WaitDirtyZero(BufferedFileWriter& out) {
	for (int i = 0; i < 80; ++i) {
		if (out.Dirty() == 0)
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return out.Dirty() == 0;
}

// -------------------
// Session / errors
// -------------------

int test_ctor_unavailable() {
	const auto path = Scratch("ctor");
	BufferedFileWriter out(path);
	ASSERT_EQUAL("test_ctor_unavailable", ToString(State::Unavailable), ToString(out.State()));
	ASSERT_FALSE("test_ctor_unavailable", static_cast<bool>(out));
	ASSERT_FALSE("test_ctor_unavailable", out.IsOpen());
	ASSERT_EQUAL("test_ctor_unavailable", static_cast<std::size_t>(0), out.Tell());
	ASSERT_EQUAL("test_ctor_unavailable", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_ctor_unavailable", static_cast<std::size_t>(0), out.WriteChunk());
	ASSERT_EQUAL("test_ctor_unavailable", static_cast<std::size_t>(0), out.BackPressure());
	ASSERT_EQUAL("test_ctor_unavailable", path, out.Path());
	RETURN_TEST("test_ctor_unavailable", 0);
}

int test_open_missing_parent() {
	BufferedFileWriter out(std::filesystem::path("no_such_sbw_dir") / "x.bin");
	ASSERT_FALSE("test_open_missing_parent", out.Open());
	ASSERT_EQUAL("test_open_missing_parent", ToString(State::Missing), ToString(out.State()));
	ASSERT_FALSE("test_open_missing_parent", static_cast<bool>(out));
	RETURN_TEST("test_open_missing_parent", 0);
}

int test_open_directory() {
	BufferedFileWriter out(CurrentFileDirectory / "files");
	ASSERT_FALSE("test_open_directory", out.Open());
	ASSERT_EQUAL("test_open_directory", ToString(State::Directory), ToString(out.State()));
	RETURN_TEST("test_open_directory", 0);
}

int test_open_creates_and_not_idempotent() {
	const auto path = Scratch("create");
	std::filesystem::remove(path);
	BufferedFileWriter out(path);
	ASSERT_TRUE("test_open_creates_and_not_idempotent", out.Open());
	ASSERT_EQUAL("test_open_creates_and_not_idempotent", ToString(State::Idle), ToString(out.State()));
	ASSERT_TRUE("test_open_creates_and_not_idempotent", static_cast<bool>(out));
	ASSERT_TRUE("test_open_creates_and_not_idempotent", std::filesystem::exists(path));
	ASSERT_EQUAL("test_open_creates_and_not_idempotent", static_cast<std::size_t>(0), out.Tell());
	ASSERT_FALSE("test_open_creates_and_not_idempotent", out.Open());
	ASSERT_EQUAL("test_open_creates_and_not_idempotent", ToString(State::Idle), ToString(out.State()));
	ASSERT_TRUE("test_open_creates_and_not_idempotent", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_open_creates_and_not_idempotent", 0);
}

int test_write_before_open_leaves_src() {
	BufferedFileWriter out(Scratch("before"));
	FIFO src = FromText("KEEP");
	const auto written = out.Write(src);
	ASSERT_EQUAL("test_write_before_open_leaves_src", ToString(Status::Failed), ToString(written.status));
	ASSERT_EQUAL("test_write_before_open_leaves_src", static_cast<std::size_t>(0), written.count);
	ASSERT_EQUAL("test_write_before_open_leaves_src", static_cast<std::size_t>(4), src.AvailableBytes());
	ASSERT_EQUAL("test_write_before_open_leaves_src", static_cast<std::size_t>(0), out.Tell());
	RETURN_TEST("test_write_before_open_leaves_src", 0);
}

int test_rewind_without_open_fails() {
	BufferedFileWriter out(Scratch("rew"));
	ASSERT_FALSE("test_rewind_without_open_fails", out.Rewind());
	ASSERT_EQUAL("test_rewind_without_open_fails", ToString(State::Unavailable), ToString(out.State()));
	RETURN_TEST("test_rewind_without_open_fails", 0);
}

int test_close_then_reopen_appends() {
	const auto path = Scratch("reopen");
	std::filesystem::remove(path);
	{
		BufferedFileWriter out(path, 0, 0);
		ASSERT_TRUE("test_close_then_reopen_appends", out.Open());
		FIFO a = FromText("AB");
		ASSERT_EQUAL("test_close_then_reopen_appends", ToString(Status::Ok), ToString(out.Write(a).status));
		ASSERT_EQUAL("test_close_then_reopen_appends", static_cast<std::size_t>(2), out.Tell());
		ASSERT_TRUE("test_close_then_reopen_appends", out.Close());
		ASSERT_EQUAL("test_close_then_reopen_appends", ToString(State::Unavailable), ToString(out.State()));
		ASSERT_EQUAL("test_close_then_reopen_appends", static_cast<std::size_t>(0), out.Dirty());
		ASSERT_TRUE("test_close_then_reopen_appends", out.Close());
		ASSERT_TRUE("test_close_then_reopen_appends", out.Open());
		ASSERT_EQUAL("test_close_then_reopen_appends", static_cast<std::size_t>(0), out.Tell());
		FIFO b = FromText("CD");
		ASSERT_EQUAL("test_close_then_reopen_appends", ToString(Status::Ok), ToString(out.Write(b).status));
		ASSERT_EQUAL("test_close_then_reopen_appends", static_cast<std::size_t>(2), out.Tell());
		ASSERT_TRUE("test_close_then_reopen_appends", out.Close());
	}
	ASSERT_EQUAL("test_close_then_reopen_appends", std::string("ABCD"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST("test_close_then_reopen_appends", 0);
}

// -------------------
// Direct write (explicit 0, 0)
// -------------------

int test_direct_write_hits_disk() {
	const auto path = Scratch("direct");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE("test_direct_write_hits_disk", out.Open());
	ASSERT_EQUAL("test_direct_write_hits_disk", static_cast<std::size_t>(0), out.WriteChunk());
	ASSERT_EQUAL("test_direct_write_hits_disk", static_cast<std::size_t>(0), out.BackPressure());
	FIFO src = FromText("HELLO");
	const auto written = out.Write(src);
	ASSERT_EQUAL("test_direct_write_hits_disk", ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL("test_direct_write_hits_disk", static_cast<std::size_t>(5), written.count);
	ASSERT_EQUAL("test_direct_write_hits_disk", static_cast<std::size_t>(0), src.AvailableBytes());
	ASSERT_EQUAL("test_direct_write_hits_disk", static_cast<std::size_t>(5), out.Tell());
	ASSERT_EQUAL("test_direct_write_hits_disk", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_direct_write_hits_disk", std::string("HELLO"), Slurp(path));
	ASSERT_TRUE("test_direct_write_hits_disk", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_direct_write_hits_disk", 0);
}

int test_direct_span_write() {
	const auto path = Scratch("span");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE("test_direct_span_write", out.Open());
	const char raw[] = { 'Z', 'Y', 'X' };
	const auto written = out.Write(std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(raw), 3));
	ASSERT_EQUAL("test_direct_span_write", ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL("test_direct_span_write", static_cast<std::size_t>(3), out.Tell());
	ASSERT_EQUAL("test_direct_span_write", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_direct_span_write", std::string("ZYX"), Slurp(path));
	ASSERT_TRUE("test_direct_span_write", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_direct_span_write", 0);
}

int test_empty_write_ok() {
	const auto path = Scratch("empty");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE("test_empty_write_ok", out.Open());
	FIFO empty;
	const auto written = out.Write(empty);
	ASSERT_EQUAL("test_empty_write_ok", ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL("test_empty_write_ok", static_cast<std::size_t>(0), written.count);
	ASSERT_EQUAL("test_empty_write_ok", static_cast<std::size_t>(0), out.Tell());
	ASSERT_EQUAL("test_empty_write_ok", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_TRUE("test_empty_write_ok", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_empty_write_ok", 0);
}

int test_explicit_zero_survives_open() {
	const auto path = Scratch("zero");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE("test_explicit_zero_survives_open", out.Open());
	ASSERT_EQUAL("test_explicit_zero_survives_open", static_cast<std::size_t>(0), out.WriteChunk());
	ASSERT_EQUAL("test_explicit_zero_survives_open", static_cast<std::size_t>(0), out.BackPressure());
	ASSERT_TRUE("test_explicit_zero_survives_open", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_explicit_zero_survives_open", 0);
}

// -------------------
// Path-only ctor / Setup
// -------------------

int test_path_only_setup_sets_device_knobs() {
	const auto path = Scratch("probe");
	std::filesystem::remove(path);
	BufferedFileWriter out(path);
	ASSERT_EQUAL("test_path_only_setup_sets_device_knobs", static_cast<std::size_t>(0), out.WriteChunk());
	ASSERT_EQUAL("test_path_only_setup_sets_device_knobs", static_cast<std::size_t>(0), out.BackPressure());
	ASSERT_TRUE("test_path_only_setup_sets_device_knobs", out.Open());
	ASSERT_TRUE("test_path_only_setup_sets_device_knobs", out.WriteChunk() >= 16ull * 1024ull);
	ASSERT_TRUE("test_path_only_setup_sets_device_knobs", out.WriteChunk() <= 1024ull * 1024ull);
	ASSERT_EQUAL("test_path_only_setup_sets_device_knobs", static_cast<std::size_t>(4), out.BackPressure());
	ASSERT_TRUE("test_path_only_setup_sets_device_knobs", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_path_only_setup_sets_device_knobs", 0);
}

int test_path_only_short_write_holds_until_flush() {
	const auto path = Scratch("phold");
	std::filesystem::remove(path);
	BufferedFileWriter out(path);
	ASSERT_TRUE("test_path_only_short_write_holds_until_flush", out.Open());
	FIFO src = FromText("ZYX");
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", ToString(Status::Ok),
		ToString(out.Write(src).status));
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", static_cast<std::size_t>(3), out.Tell());
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", static_cast<std::size_t>(3), out.Dirty());
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", std::string(""), Slurp(path));
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", ToString(Status::Ok),
		ToString(out.Flush().status));
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_path_only_short_write_holds_until_flush", std::string("ZYX"), Slurp(path));
	ASSERT_TRUE("test_path_only_short_write_holds_until_flush", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_path_only_short_write_holds_until_flush", 0);
}

int test_explicit_chunk_survives_setup() {
	const auto path = Scratch("keep");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 2);
	ASSERT_TRUE("test_explicit_chunk_survives_setup", out.Open());
	ASSERT_EQUAL("test_explicit_chunk_survives_setup", static_cast<std::size_t>(8), out.WriteChunk());
	ASSERT_EQUAL("test_explicit_chunk_survives_setup", static_cast<std::size_t>(2), out.BackPressure());
	ASSERT_TRUE("test_explicit_chunk_survives_setup", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_explicit_chunk_survives_setup", 0);
}

// -------------------
// Ring / Flush / BackPressure
// -------------------

int test_chunk_holds_until_flush() {
	const auto path = Scratch("hold");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 4);
	ASSERT_TRUE("test_chunk_holds_until_flush", out.Open());
	FIFO src = FromText("ABC");
	ASSERT_EQUAL("test_chunk_holds_until_flush", ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL("test_chunk_holds_until_flush", static_cast<std::size_t>(3), out.Dirty());
	ASSERT_EQUAL("test_chunk_holds_until_flush", static_cast<std::size_t>(3), out.Tell());
	ASSERT_EQUAL("test_chunk_holds_until_flush", std::string(""), Slurp(path));
	ASSERT_EQUAL("test_chunk_holds_until_flush", ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL("test_chunk_holds_until_flush", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_chunk_holds_until_flush", static_cast<std::size_t>(3), out.Tell());
	ASSERT_EQUAL("test_chunk_holds_until_flush", std::string("ABC"), Slurp(path));
	ASSERT_TRUE("test_chunk_holds_until_flush", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_chunk_holds_until_flush", 0);
}

int test_full_chunk_clears_dirty() {
	const auto path = Scratch("drain");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 4, 4);
	ASSERT_TRUE("test_full_chunk_clears_dirty", out.Open());
	FIFO src = FromText("ABCD");
	ASSERT_EQUAL("test_full_chunk_clears_dirty", ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL("test_full_chunk_clears_dirty", static_cast<std::size_t>(4), out.Tell());
	ASSERT_TRUE("test_full_chunk_clears_dirty", WaitDirtyZero(out));
	ASSERT_EQUAL("test_full_chunk_clears_dirty", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_full_chunk_clears_dirty", ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL("test_full_chunk_clears_dirty", std::string("ABCD"), Slurp(path));
	ASSERT_TRUE("test_full_chunk_clears_dirty", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_full_chunk_clears_dirty", 0);
}

int test_two_chunks_then_flush() {
	const auto path = Scratch("two");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 2, 4);
	ASSERT_TRUE("test_two_chunks_then_flush", out.Open());
	FIFO a = FromText("AB");
	FIFO b = FromText("CD");
	ASSERT_EQUAL("test_two_chunks_then_flush", ToString(Status::Ok), ToString(out.Write(a).status));
	ASSERT_EQUAL("test_two_chunks_then_flush", static_cast<std::size_t>(2), out.Tell());
	ASSERT_TRUE("test_two_chunks_then_flush", WaitDirtyZero(out));
	ASSERT_EQUAL("test_two_chunks_then_flush", ToString(Status::Ok), ToString(out.Write(b).status));
	ASSERT_EQUAL("test_two_chunks_then_flush", static_cast<std::size_t>(4), out.Tell());
	ASSERT_TRUE("test_two_chunks_then_flush", WaitDirtyZero(out));
	ASSERT_EQUAL("test_two_chunks_then_flush", ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL("test_two_chunks_then_flush", std::string("ABCD"), Slurp(path));
	ASSERT_TRUE("test_two_chunks_then_flush", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_two_chunks_then_flush", 0);
}

int test_backpressure_tryagain_atomic() {
	const auto path = Scratch("bp");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 4, 1);
	ASSERT_TRUE("test_backpressure_tryagain_atomic", out.Open());
	FIFO first = FromText("AB");
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", ToString(Status::Ok), ToString(out.Write(first).status));
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", static_cast<std::size_t>(2), out.Tell());
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", static_cast<std::size_t>(2), out.Dirty());
	FIFO second = FromText("CDEFGH");
	const auto blocked = out.Write(second);
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", ToString(Status::TryAgain), ToString(blocked.status));
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", static_cast<std::size_t>(0), blocked.count);
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", static_cast<std::size_t>(6), second.AvailableBytes());
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", static_cast<std::size_t>(2), out.Tell());
	ASSERT_EQUAL("test_backpressure_tryagain_atomic", static_cast<std::size_t>(2), out.Dirty());
	ASSERT_TRUE("test_backpressure_tryagain_atomic", out.Close());
	std::filesystem::remove(path);
	RETURN_TEST("test_backpressure_tryagain_atomic", 0);
}

int test_close_flushes_dirty() {
	const auto path = Scratch("cflush");
	std::filesystem::remove(path);
	{
		BufferedFileWriter out(path, 8, 4);
		ASSERT_TRUE("test_close_flushes_dirty", out.Open());
		FIFO src = FromText("XY");
		ASSERT_EQUAL("test_close_flushes_dirty", ToString(Status::Ok), ToString(out.Write(src).status));
		ASSERT_EQUAL("test_close_flushes_dirty", static_cast<std::size_t>(2), out.Dirty());
		ASSERT_TRUE("test_close_flushes_dirty", out.Close());
	}
	ASSERT_EQUAL("test_close_flushes_dirty", std::string("XY"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST("test_close_flushes_dirty", 0);
}

int test_truncate_zeros_file_and_tell() {
	const auto path = Scratch("trunc");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE("test_truncate_zeros_file_and_tell", out.Open());
	FIFO src = FromText("XYZ");
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", static_cast<std::size_t>(3), out.Tell());
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", ToString(Status::Ok), ToString(out.Truncate().status));
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", static_cast<std::size_t>(0), out.Tell());
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", std::string(""), Slurp(path));
	FIFO again = FromText("OK");
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", ToString(Status::Ok), ToString(out.Write(again).status));
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", static_cast<std::size_t>(2), out.Tell());
	ASSERT_TRUE("test_truncate_zeros_file_and_tell", out.Close());
	ASSERT_EQUAL("test_truncate_zeros_file_and_tell", std::string("OK"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST("test_truncate_zeros_file_and_tell", 0);
}

int test_truncate_drops_dirty() {
	const auto path = Scratch("tdrop");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 4);
	ASSERT_TRUE("test_truncate_drops_dirty", out.Open());
	FIFO src = FromText("NOPE");
	ASSERT_EQUAL("test_truncate_drops_dirty", ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL("test_truncate_drops_dirty", static_cast<std::size_t>(4), out.Dirty());
	ASSERT_EQUAL("test_truncate_drops_dirty", ToString(Status::Ok), ToString(out.Truncate().status));
	ASSERT_EQUAL("test_truncate_drops_dirty", static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL("test_truncate_drops_dirty", static_cast<std::size_t>(0), out.Tell());
	ASSERT_TRUE("test_truncate_drops_dirty", out.Close());
	ASSERT_EQUAL("test_truncate_drops_dirty", std::string(""), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST("test_truncate_drops_dirty", 0);
}

// -------------------
// Move
// -------------------

int test_move_transfers_session() {
	const auto path = Scratch("move");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE("test_move_transfers_session", out.Open());
	FIFO first = FromText("AB");
	ASSERT_EQUAL("test_move_transfers_session", ToString(Status::Ok), ToString(out.Write(first).status));
	BufferedFileWriter moved(std::move(out));
	ASSERT_FALSE("test_move_transfers_session", static_cast<bool>(out));
	ASSERT_EQUAL("test_move_transfers_session", ToString(State::Unavailable), ToString(out.State()));
	ASSERT_EQUAL("test_move_transfers_session", static_cast<std::size_t>(2), moved.Tell());
	FIFO rest = FromText("C");
	ASSERT_EQUAL("test_move_transfers_session", ToString(Status::Ok), ToString(moved.Write(rest).status));
	ASSERT_EQUAL("test_move_transfers_session", static_cast<std::size_t>(3), moved.Tell());
	ASSERT_TRUE("test_move_transfers_session", moved.Close());
	ASSERT_EQUAL("test_move_transfers_session", std::string("ABC"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST("test_move_transfers_session", 0);
}

int main() {
	int result = 0;

	// -------------------
	// Session / errors
	// -------------------
	result += test_ctor_unavailable();
	result += test_open_missing_parent();
	result += test_open_directory();
	result += test_open_creates_and_not_idempotent();
	result += test_write_before_open_leaves_src();
	result += test_rewind_without_open_fails();
	result += test_close_then_reopen_appends();

	// -------------------
	// Direct write
	// -------------------
	result += test_direct_write_hits_disk();
	result += test_direct_span_write();
	result += test_empty_write_ok();
	result += test_explicit_zero_survives_open();

	// -------------------
	// Path-only ctor / Setup
	// -------------------
	result += test_path_only_setup_sets_device_knobs();
	result += test_path_only_short_write_holds_until_flush();
	result += test_explicit_chunk_survives_setup();

	// -------------------
	// Ring / Flush / BackPressure
	// -------------------
	result += test_chunk_holds_until_flush();
	result += test_full_chunk_clears_dirty();
	result += test_two_chunks_then_flush();
	result += test_backpressure_tryagain_atomic();
	result += test_close_flushes_dirty();
	result += test_truncate_zeros_file_and_tell();
	result += test_truncate_drops_dirty();

	// -------------------
	// Move
	// -------------------
	result += test_move_transfers_session();

	if (result == 0)
		std::cout << "BufferedFileWriter tests passed!" << std::endl;
	else
		std::cout << result << " BufferedFileWriter tests failed." << std::endl;
	return result;
}
