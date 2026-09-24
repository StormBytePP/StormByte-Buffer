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

#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/system/file.hxx>
#include <StormByte/test_handlers.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using StormByte::Buffer::Data;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::IO::BufferedFileWriter;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::IO::ToString;

namespace {
	std::filesystem::path Scratch(const char* tag) {
		StormByte::String::String path;
		if (!StormByte::System::File::Temporary(path, tag))
			return {};
		return std::filesystem::path(std::string(path));
	}

	std::string Slurp(const std::filesystem::path& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in)
			return {};
		return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	FIFO FromText(const std::string& text) {
		FIFO fifo;
		Data data(StormByte::Size{text.size()});
		for (std::size_t i = 0; i < text.size(); ++i)
			data[i] = static_cast<std::byte>(text[i]);
		static_cast<void>(fifo.Write(data.size(), std::move(data)));
		return fifo;
	}

	bool WaitDirtyZero(BufferedFileWriter& out) {
		for (int i = 0; i < 80; ++i) {
			if (out.Dirty() == StormByte::Size{0})
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return out.Dirty() == StormByte::Size{0};
	}
}

// -------------------
// Direct write
// -------------------

int test_direct_span_write() {
	const std::string fn = "test_direct_span_write";
	const auto path = Scratch("span");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	const char raw[] = { 'Z', 'Y', 'X' };
	const auto written = out.Write(std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(raw), 3));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, std::string("ZYX"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_direct_write_hits_disk() {
	const std::string fn = "test_direct_write_hits_disk";
	const auto path = Scratch("direct");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.WriteChunk());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), out.BackPressure());
	FIFO src = FromText("HELLO");
	const auto written = out.Write(src);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, written.count);
	ASSERT_EQUAL(fn, StormByte::Size{0}, src.AvailableBytes());
	ASSERT_EQUAL(fn, StormByte::Size{5}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, std::string("HELLO"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_empty_write_ok() {
	const std::string fn = "test_empty_write_ok";
	const auto path = Scratch("empty");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO empty;
	const auto written = out.Write(empty);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, written.count);
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_explicit_zero_survives_open() {
	const std::string fn = "test_explicit_zero_survives_open";
	const auto path = Scratch("zero");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.WriteChunk());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), out.BackPressure());
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

// -------------------
// Move
// -------------------

int test_move_transfers_session() {
	const std::string fn = "test_move_transfers_session";
	const auto path = Scratch("move");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO first = FromText("AB");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(first).status));
	BufferedFileWriter moved(std::move(out));
	ASSERT_FALSE(fn, static_cast<bool>(out));
	ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(out.State()));
	ASSERT_EQUAL(fn, StormByte::Size{2}, moved.Tell());
	FIFO rest = FromText("C");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(moved.Write(rest).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, moved.Tell());
	ASSERT_TRUE(fn, moved.Close());
	ASSERT_EQUAL(fn, std::string("ABC"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

// -------------------
// Path-only / Setup
// -------------------

int test_explicit_chunk_survives_setup() {
	const std::string fn = "test_explicit_chunk_survives_setup";
	const auto path = Scratch("keep");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 2);
	ASSERT_TRUE(fn, out.Open());
	ASSERT_EQUAL(fn, StormByte::Size{8}, out.WriteChunk());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(2), out.BackPressure());
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_path_only_setup_sets_device_knobs() {
	const std::string fn = "test_path_only_setup_sets_device_knobs";
	const auto path = Scratch("probe");
	std::filesystem::remove(path);
	BufferedFileWriter out(path);
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.WriteChunk());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), out.BackPressure());
	ASSERT_TRUE(fn, out.Open());
	ASSERT_TRUE(fn, out.WriteChunk() >= StormByte::Size{16ull * 1024ull});
	ASSERT_TRUE(fn, out.WriteChunk() <= StormByte::Size{1024ull * 1024ull});
	ASSERT_EQUAL(fn, static_cast<std::size_t>(4), out.BackPressure());
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_path_only_short_write_holds_until_flush() {
	const std::string fn = "test_path_only_short_write_holds_until_flush";
	const auto path = Scratch("phold");
	std::filesystem::remove(path);
	BufferedFileWriter out(path);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("ZYX");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Dirty());
	ASSERT_EQUAL(fn, std::string(""), Slurp(path));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, std::string("ZYX"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

// -------------------
// Ring / Flush
// -------------------

int test_backpressure_tryagain_atomic() {
	const std::string fn = "test_backpressure_tryagain_atomic";
	const auto path = Scratch("bp");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 4, 1);
	ASSERT_TRUE(fn, out.Open());
	FIFO first = FromText("AB");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(first).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Dirty());
	FIFO second = FromText("CDEFGH");
	const auto blocked = out.Write(second);
	ASSERT_EQUAL(fn, ToString(Status::TryAgain), ToString(blocked.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, blocked.count);
	ASSERT_EQUAL(fn, StormByte::Size{6}, second.AvailableBytes());
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Dirty());
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_chunk_holds_until_flush() {
	const std::string fn = "test_chunk_holds_until_flush";
	const auto path = Scratch("hold");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 4);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("ABC");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, std::string(""), Slurp(path));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, std::string("ABC"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_close_flushes_dirty() {
	const std::string fn = "test_close_flushes_dirty";
	const auto path = Scratch("cflush");
	std::filesystem::remove(path);
	{
		BufferedFileWriter out(path, 8, 4);
		ASSERT_TRUE(fn, out.Open());
		FIFO src = FromText("XY");
		ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
		ASSERT_EQUAL(fn, StormByte::Size{2}, out.Dirty());
		ASSERT_TRUE(fn, out.Close());
	}
	ASSERT_EQUAL(fn, std::string("XY"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_full_chunk_clears_dirty() {
	const std::string fn = "test_full_chunk_clears_dirty";
	const auto path = Scratch("drain");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 4, 4);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("ABCD");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Tell());
	ASSERT_TRUE(fn, WaitDirtyZero(out));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL(fn, std::string("ABCD"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_truncate_drops_dirty() {
	const std::string fn = "test_truncate_drops_dirty";
	const auto path = Scratch("tdrop");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 4);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("NOPE");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Dirty());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Truncate().status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string(""), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_truncate_zeros_file_and_tell() {
	const std::string fn = "test_truncate_zeros_file_and_tell";
	const auto path = Scratch("trunc");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("XYZ");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Truncate().status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, std::string(""), Slurp(path));
	FIFO again = FromText("OK");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(again).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("OK"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_two_chunks_then_flush() {
	const std::string fn = "test_two_chunks_then_flush";
	const auto path = Scratch("two");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 2, 4);
	ASSERT_TRUE(fn, out.Open());
	FIFO a = FromText("AB");
	FIFO b = FromText("CD");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(a).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
	ASSERT_TRUE(fn, WaitDirtyZero(out));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(b).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Tell());
	ASSERT_TRUE(fn, WaitDirtyZero(out));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL(fn, std::string("ABCD"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

// -------------------
// Seek / Size
// -------------------

int test_seek_before_start_fails() {
	const std::string fn = "test_seek_before_start_fails";
	const auto path = Scratch("neg");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("HI");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(out.Seek(-1, Position::Absolute).status));
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(out.Seek(-3, Position::Relative).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("HI"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_seek_flushes_dirty_then_patches() {
	const std::string fn = "test_seek_flushes_dirty_then_patches";
	const auto path = Scratch("sdirty");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 4);
	ASSERT_TRUE(fn, out.Open());
	FIFO fill = FromText("XXXX");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(fill).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Size());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Seek(1, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{1}, out.Tell());
	ASSERT_EQUAL(fn, std::string("XXXX"), Slurp(path));
	FIFO mid = FromText("YZ");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(mid).status));
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("XYZX"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_seek_patch_direct() {
	const std::string fn = "test_seek_patch_direct";
	const auto path = Scratch("patch");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO fill = FromText("XXXXYYYY");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(fill).status));
	ASSERT_EQUAL(fn, StormByte::Size{8}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{8}, out.Size());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	FIFO ab = FromText("AB");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(ab).status));
	ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Seek(4, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Tell());
	FIFO cd = FromText("CD");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(cd).status));
	ASSERT_EQUAL(fn, StormByte::Size{6}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{8}, out.Size());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("ABXXCDYY"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_seek_relative() {
	const std::string fn = "test_seek_relative";
	const auto path = Scratch("rel");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO fill = FromText("01234567");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(fill).status));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Seek(-4, Position::Relative).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.Tell());
	FIFO mid = FromText("AB");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(mid).status));
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("0123AB67"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_seek_then_extend() {
	const std::string fn = "test_seek_then_extend";
	const auto path = Scratch("ext");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO head = FromText("AB");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(head).status));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Seek(2, Position::Absolute).status));
	FIFO tail = FromText("CDEF");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(tail).status));
	ASSERT_EQUAL(fn, StormByte::Size{6}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{6}, out.Size());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("ABCDEF"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_seek_without_open_fails() {
	const std::string fn = "test_seek_without_open_fails";
	BufferedFileWriter out(Scratch("closed"), 0, 0);
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(out.Seek(0, Position::Absolute).status));
	RETURN_TEST(fn, 0);
}

int test_size_counts_dirty() {
	const std::string fn = "test_size_counts_dirty";
	const auto path = Scratch("szdirty");
	std::filesystem::remove(path);
	BufferedFileWriter out(path, 8, 4);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("ABC");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Size());
	ASSERT_EQUAL(fn, std::string(""), Slurp(path));
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.Size());
	ASSERT_EQUAL(fn, std::string("ABC"), Slurp(path));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

// -------------------
// Session / errors
// -------------------

int test_close_then_reopen_appends() {
	const std::string fn = "test_close_then_reopen_appends";
	const auto path = Scratch("reopen");
	std::filesystem::remove(path);
	{
		BufferedFileWriter out(path, 0, 0);
		ASSERT_TRUE(fn, out.Open());
		FIFO a = FromText("AB");
		ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(a).status));
		ASSERT_EQUAL(fn, StormByte::Size{2}, out.Tell());
		ASSERT_TRUE(fn, out.Close());
		ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(out.State()));
		ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
		ASSERT_TRUE(fn, out.Close());
		ASSERT_TRUE(fn, out.Open());
		ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
		ASSERT_EQUAL(fn, ToString(Status::Ok),
			ToString(out.Seek(static_cast<std::ptrdiff_t>(out.Size()), Position::Absolute).status));
		FIFO b = FromText("CD");
		ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(b).status));
		ASSERT_EQUAL(fn, StormByte::Size{4}, out.Tell());
		ASSERT_TRUE(fn, out.Close());
	}
	ASSERT_EQUAL(fn, std::string("ABCD"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_ctor_unavailable() {
	const std::string fn = "test_ctor_unavailable";
	const auto path = Scratch("ctor");
	BufferedFileWriter out(path);
	ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(out.State()));
	ASSERT_FALSE(fn, static_cast<bool>(out));
	ASSERT_FALSE(fn, out.IsOpen());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Dirty());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.WriteChunk());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), out.BackPressure());
	ASSERT_EQUAL(fn, path, out.Path());
	RETURN_TEST(fn, 0);
}

int test_open_creates_and_not_idempotent() {
	const std::string fn = "test_open_creates_and_not_idempotent";
	const auto path = Scratch("create");
	std::filesystem::remove(path);
	BufferedFileWriter out(path);
	ASSERT_TRUE(fn, out.Open());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(out.State()));
	ASSERT_TRUE(fn, static_cast<bool>(out));
	ASSERT_TRUE(fn, std::filesystem::exists(path));
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	ASSERT_FALSE(fn, out.Open());
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(out.State()));
	ASSERT_TRUE(fn, out.Close());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_open_directory() {
	const std::string fn = "test_open_directory";
	BufferedFileWriter out(CurrentFileDirectory / "files");
	ASSERT_FALSE(fn, out.Open());
	ASSERT_EQUAL(fn, ToString(State::Directory), ToString(out.State()));
	RETURN_TEST(fn, 0);
}

int test_open_missing_parent() {
	const std::string fn = "test_open_missing_parent";
	BufferedFileWriter out(std::filesystem::path("no_such_sbw_dir") / "x.bin");
	ASSERT_FALSE(fn, out.Open());
	ASSERT_EQUAL(fn, ToString(State::Missing), ToString(out.State()));
	ASSERT_FALSE(fn, static_cast<bool>(out));
	RETURN_TEST(fn, 0);
}

int test_rewind_without_open_fails() {
	const std::string fn = "test_rewind_without_open_fails";
	BufferedFileWriter out(Scratch("rew"));
	ASSERT_FALSE(fn, out.Rewind());
	ASSERT_EQUAL(fn, ToString(State::Unavailable), ToString(out.State()));
	RETURN_TEST(fn, 0);
}

int test_write_before_open_leaves_src() {
	const std::string fn = "test_write_before_open_leaves_src";
	BufferedFileWriter out(Scratch("before"));
	FIFO src = FromText("KEEP");
	const auto written = out.Write(src);
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(written.status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, written.count);
	ASSERT_EQUAL(fn, StormByte::Size{4}, src.AvailableBytes());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.Tell());
	RETURN_TEST(fn, 0);
}

int main() {
	int result = 0;

	// -------------------
	// Direct write
	// -------------------
	result += test_direct_span_write();
	result += test_direct_write_hits_disk();
	result += test_empty_write_ok();
	result += test_explicit_zero_survives_open();

	// -------------------
	// Move
	// -------------------
	result += test_move_transfers_session();

	// -------------------
	// Path-only / Setup
	// -------------------
	result += test_explicit_chunk_survives_setup();
	result += test_path_only_setup_sets_device_knobs();
	result += test_path_only_short_write_holds_until_flush();

	// -------------------
	// Ring / Flush
	// -------------------
	result += test_backpressure_tryagain_atomic();
	result += test_chunk_holds_until_flush();
	result += test_close_flushes_dirty();
	result += test_full_chunk_clears_dirty();
	result += test_truncate_drops_dirty();
	result += test_truncate_zeros_file_and_tell();
	result += test_two_chunks_then_flush();

	// -------------------
	// Seek / Size
	// -------------------
	result += test_seek_before_start_fails();
	result += test_seek_flushes_dirty_then_patches();
	result += test_seek_patch_direct();
	result += test_seek_relative();
	result += test_seek_then_extend();
	result += test_seek_without_open_fails();
	result += test_size_counts_dirty();

	// -------------------
	// Session / errors
	// -------------------
	result += test_close_then_reopen_appends();
	result += test_ctor_unavailable();
	result += test_open_creates_and_not_idempotent();
	result += test_open_directory();
	result += test_open_missing_parent();
	result += test_rewind_without_open_fails();
	result += test_write_before_open_leaves_src();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
