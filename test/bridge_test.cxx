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

#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/string.hxx>
#include <StormByte/system.hxx>
#include <StormByte/test_handlers.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using StormByte::Buffer::Bridge;
using StormByte::Buffer::DataType;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::BufferedFileWriter;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::IO::ToString;
using StormByte::System::TempFileName;

// -------------------
// Helpers
// -------------------

static std::filesystem::path File(const char* name) {
	return CurrentFileDirectory / "files" / name;
}

static std::filesystem::path Scratch(const char* tag) {
	return std::filesystem::path(TempFileName(std::string("sbr_") + tag));
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

static std::string FifoText(const FIFO& fifo) {
	return StormByte::String::FromByteVector(fifo.Data());
}

static bool WaitDirtyZero(BufferedFileWriter& out) {
	for (int i = 0; i < 80; ++i) {
		if (out.Dirty() == 0)
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return out.Dirty() == 0;
}

class FaultyReader final : public ExternalReader {
	public:
		explicit FaultyReader(FIFO& from) noexcept
			: m_source(from), m_fail_extract(true), m_fail_read(true) {}

		std::size_t AvailableBytes() const noexcept override {
			return m_source.AvailableBytes();
		}

		bool Empty() const noexcept override { return m_source.Empty(); }
		bool EoF() const noexcept override { return m_source.EoF(); }
		bool IsReadable() const noexcept override { return m_source.IsReadable(); }

		bool Read(std::size_t bytes, DataType& out) const noexcept override {
			if (m_fail_read) {
				m_fail_read = false;
				return false;
			}
			return m_source.Extract(bytes, out);
		}

		bool Extract(std::size_t bytes, DataType& out) noexcept override {
			if (m_fail_extract) {
				m_fail_extract = false;
				return false;
			}
			return m_source.Extract(bytes, out);
		}

		bool Peek(std::size_t bytes, DataType& out) const noexcept override {
			return m_source.Peek(bytes, out);
		}

		void ReadUntilEoF(DataType& out) const noexcept override {
			m_source.ReadUntilEoF(out);
		}

		void ExtractUntilEoF(DataType& out) noexcept override {
			m_source.ExtractUntilEoF(out);
		}

		void Seek(std::ptrdiff_t offset, Position mode) const noexcept override {
			m_source.Seek(offset, mode);
		}

		void Clean() noexcept override { m_source.Clean(); }

		PointerType Clone() const noexcept override {
			return MakePointer<FaultyReader>(m_source);
		}

		PointerType Move() noexcept override {
			return MakePointer<FaultyReader>(m_source);
		}

	private:
		FIFO& m_source;
		mutable bool m_fail_extract;
		mutable bool m_fail_read;
};

class FailingWriter final : public ExternalWriter {
	public:
		FailingWriter(FIFO& to, std::size_t succeed_calls) noexcept
			: m_target(to), m_succeed(succeed_calls), m_calls(0), m_closed(false), m_error(false) {}

		bool IsWritable() const noexcept override {
			return !m_closed && !m_error;
		}

		std::size_t Occupied() const noexcept override {
			return m_target.Size();
		}

		bool Write(const DataType& data) noexcept override {
			DataType copy = data;
			return Write(std::move(copy));
		}

		bool Write(DataType&& in) noexcept override {
			if (m_closed || m_error)
				return false;
			if (m_calls < m_succeed) {
				++m_calls;
				return m_target.Write(std::move(in));
			}
			return false;
		}

		bool Write(std::size_t count, const DataType& data) noexcept override {
			if (count == 0)
				return Write(data);
			DataType tmp(data.begin(),
				data.begin() + static_cast<std::ptrdiff_t>(std::min(count, data.size())));
			return Write(std::move(tmp));
		}

		bool Write(std::size_t count, DataType&& data) noexcept override {
			if (count == 0)
				return Write(std::move(data));
			if (count < data.size())
				data.resize(count);
			return Write(std::move(data));
		}

		void Close() noexcept override { m_closed = true; }
		void SetError() noexcept override { m_error = true; }

		PointerType Clone() const noexcept override {
			return MakePointer<FailingWriter>(m_target, m_succeed);
		}

		PointerType Move() noexcept override {
			return MakePointer<FailingWriter>(m_target, m_succeed);
		}

	private:
		FIFO& m_target;
		std::size_t m_succeed;
		std::size_t m_calls;
		bool m_closed;
		bool m_error;
};

// -------------------
// Buffer → buffer
// -------------------

int test_ext_passthrough_all() {
	const std::string fn = "test_ext_passthrough_all";
	const std::string text = "The quick brown fox jumps over the lazy dog.";
	FIFO src = FromText(text);
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.IsReadable());
	ASSERT_TRUE(fn, bridge.IsWritable());
	ASSERT_TRUE(fn, bridge.Passthrough(src.AvailableBytes()));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, text, FifoText(dst));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), src.AvailableBytes());
	RETURN_TEST(fn, 0);
}

int test_ext_passthrough_zero_available_now() {
	const std::string fn = "test_ext_passthrough_zero_available_now";
	const std::string text = "Mr. Jock, TV quiz PhD, bags few lynx.";
	FIFO src = FromText(text);
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(0));
	ASSERT_EQUAL(fn, text, FifoText(dst));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), src.AvailableBytes());
	RETURN_TEST(fn, 0);
}

int test_ext_passthrough_zero_when_empty() {
	const std::string fn = "test_ext_passthrough_zero_when_empty";
	FIFO src;
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(0));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	RETURN_TEST(fn, 0);
}

int test_ext_passthrough_consumes_source() {
	const std::string fn = "test_ext_passthrough_consumes_source";
	FIFO src = FromText("ABCDEFGH");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(3));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), src.AvailableBytes());
	ASSERT_EQUAL(fn, std::string("ABC"), FifoText(dst));
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), src.AvailableBytes());
	ASSERT_EQUAL(fn, std::string("ABCDEFGH"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_multiple_passthrough() {
	const std::string fn = "test_ext_multiple_passthrough";
	const std::string text = "How vexingly quick daft zebras jump!";
	FIFO src = FromText(text);
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(10));
	ASSERT_TRUE(fn, bridge.Passthrough(10));
	ASSERT_TRUE(fn, bridge.Passthrough(src.AvailableBytes()));
	ASSERT_EQUAL(fn, text, FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_flush_is_noop() {
	const std::string fn = "test_ext_flush_is_noop";
	FIFO src = FromText("HELLO");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_EQUAL(fn, std::string("HELLO"), FifoText(dst));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("HELLO"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_dtor_flush_noop_data_already_there() {
	const std::string fn = "test_ext_dtor_flush_noop_data_already_there";
	const std::string text = "The quick brown fox jumps over the lazy dog.";
	FIFO src = FromText(text);
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	{
		Bridge bridge(in, out);
		ASSERT_TRUE(fn, bridge.Passthrough(src.AvailableBytes()));
	}
	ASSERT_EQUAL(fn, text, FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_flush_and_close() {
	const std::string fn = "test_ext_flush_and_close";
	FIFO src = FromText("CLOSEME");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(7));
	ASSERT_TRUE(fn, bridge.FlushAndClose());
	ASSERT_FALSE(fn, bridge.IsWritable());
	ASSERT_FALSE(fn, out.IsWritable());
	RETURN_TEST(fn, 0);
}

int test_ext_set_error() {
	const std::string fn = "test_ext_set_error";
	FIFO src = FromText("ERR");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.IsWritable());
	bridge.SetError();
	ASSERT_FALSE(fn, bridge.IsWritable());
	ASSERT_TRUE(fn, bridge.IsReadable());
	ASSERT_FALSE(fn, bridge.Passthrough(3));
	RETURN_TEST(fn, 0);
}

int test_ext_eof_after_close_source() {
	const std::string fn = "test_ext_eof_after_close_source";
	FIFO src = FromText("X");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(1));
	src.Close();
	ASSERT_TRUE(fn, bridge.EoF());
	RETURN_TEST(fn, 0);
}

int test_ext_reader_fail_then_recover() {
	const std::string fn = "test_ext_reader_fail_then_recover";
	const std::string text = "Pack my box with five dozen liquor jugs.";
	FIFO src = FromText(text);
	FIFO dst;
	FaultyReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_FALSE(fn, bridge.Passthrough(8));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	ASSERT_TRUE(fn, bridge.Passthrough(src.AvailableBytes()));
	ASSERT_EQUAL(fn, text, FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_writer_failure() {
	const std::string fn = "test_ext_writer_failure";
	const std::string text = "Sphinx of black quartz, judge my vow.";
	FIFO src = FromText(text);
	FIFO dst;
	ExternalBufferReader in(src);
	FailingWriter out(dst, 0);
	Bridge bridge(in, out);
	ASSERT_FALSE(fn, bridge.Passthrough(src.AvailableBytes()));
	RETURN_TEST(fn, 0);
}

int test_ext_large_transfer() {
	const std::string fn = "test_ext_large_transfer";
	std::string text;
	text.reserve(200 * 1024);
	for (std::size_t i = 0; i < 200 * 1024; ++i)
		text.push_back(static_cast<char>('A' + (i % 26)));
	FIFO src = FromText(text);
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(src.AvailableBytes()));
	ASSERT_EQUAL(fn, text, FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_move_bridge() {
	const std::string fn = "test_ext_move_bridge";
	FIFO src = FromText("MOVEOK");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge first(in, out);
	Bridge second(std::move(first));
	ASSERT_FALSE(fn, first.Passthrough(6));
	ASSERT_TRUE(fn, second.Passthrough(6));
	ASSERT_EQUAL(fn, std::string("MOVEOK"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_move_assign_bridge() {
	const std::string fn = "test_ext_move_assign_bridge";
	FIFO src_a = FromText("AAAA");
	FIFO dst_a;
	ExternalBufferReader in_a(src_a);
	ExternalBufferWriter out_a(dst_a);
	Bridge a(in_a, out_a);

	FIFO src_b = FromText("BBBB");
	FIFO dst_b;
	ExternalBufferReader in_b(src_b);
	ExternalBufferWriter out_b(dst_b);
	Bridge b(in_b, out_b);
	b = std::move(a);
	ASSERT_TRUE(fn, b.Passthrough(4));
	ASSERT_EQUAL(fn, std::string("AAAA"), FifoText(dst_a));
	ASSERT_FALSE(fn, a.Passthrough(4));
	RETURN_TEST(fn, 0);
}

// -------------------
// IO → IO
// -------------------

int test_io_file_to_file() {
	const std::string fn = "test_io_file_to_file";
	const auto out_path = Scratch("io2io");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.IsReadable());
	ASSERT_TRUE(fn, bridge.IsWritable());
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), out.Tell());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), out.Dirty());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), in.Tell());
	ASSERT_TRUE(fn, bridge.Passthrough(1));
	ASSERT_TRUE(fn, in.EoF());
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_file_to_file_short_end() {
	const std::string fn = "test_io_file_to_file_short_end";
	const auto out_path = Scratch("short");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(64));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	ASSERT_TRUE(fn, in.EoF());
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_passthrough_zero_is_window_only() {
	const std::string fn = "test_io_passthrough_zero_is_window_only";
	const auto out_path = Scratch("win0");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"), 0, 0);
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(0));
	ASSERT_EQUAL(fn, std::string(""), Slurp(out_path));
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_split_then_rest() {
	const std::string fn = "test_io_split_then_rest";
	const auto out_path = Scratch("split");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(2));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(2), in.Tell());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(2), out.Tell());
	ASSERT_TRUE(fn, bridge.Passthrough(3));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_unopened_passthrough_fails() {
	const std::string fn = "test_io_unopened_passthrough_fails";
	const auto out_path = Scratch("unopen");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	Bridge bridge(in, out);
	ASSERT_FALSE(fn, bridge.IsReadable());
	ASSERT_FALSE(fn, bridge.IsWritable());
	ASSERT_FALSE(fn, bridge.Passthrough(5));
	ASSERT_EQUAL(fn, std::string(""), Slurp(out_path));
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_chunked_writer_flush() {
	const std::string fn = "test_io_chunked_writer_flush";
	const auto out_path = Scratch("chunk");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 8, 4);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), out.Tell());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), out.Dirty());
	ASSERT_EQUAL(fn, std::string(""), Slurp(out_path));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_TRUE(fn, WaitDirtyZero(out));
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_flush_and_close_does_not_close_file() {
	const std::string fn = "test_io_flush_and_close_does_not_close_file";
	const auto out_path = Scratch("noclose");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(2));
	ASSERT_TRUE(fn, bridge.FlushAndClose());
	ASSERT_TRUE(fn, static_cast<bool>(out));
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(out.State()));
	ASSERT_TRUE(fn, bridge.Passthrough(3));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_set_error_noop() {
	const std::string fn = "test_io_set_error_noop";
	const auto out_path = Scratch("seterr");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	bridge.SetError();
	ASSERT_TRUE(fn, bridge.IsWritable());
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_empty_file() {
	const std::string fn = "test_io_empty_file";
	const auto out_path = Scratch("empty");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("empty.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(16));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string(""), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_pattern_256() {
	const std::string fn = "test_io_pattern_256";
	const auto out_path = Scratch("pat");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("pattern_256.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(256));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, Slurp(File("pattern_256.bin")), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_block_4k_loop() {
	const std::string fn = "test_io_block_4k_loop";
	const auto out_path = Scratch("4k");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("block_4k.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	while (!in.EoF() && static_cast<bool>(in))
		ASSERT_TRUE(fn, bridge.Passthrough(512));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, Slurp(File("block_4k.bin")), Slurp(out_path));
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

// -------------------
// Buffer → IO
// -------------------

int test_buf_to_io() {
	const std::string fn = "test_buf_to_io";
	const auto out_path = Scratch("b2io");
	std::filesystem::remove(out_path);
	FIFO src = FromText("HELLO");
	ExternalBufferReader in(src);
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("HELLO"), Slurp(out_path));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), src.AvailableBytes());
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_buf_to_io_zero() {
	const std::string fn = "test_buf_to_io_zero";
	const auto out_path = Scratch("b2io0");
	std::filesystem::remove(out_path);
	FIFO src = FromText("ZYX");
	ExternalBufferReader in(src);
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(0));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ZYX"), Slurp(out_path));
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_buf_to_io_splits() {
	const std::string fn = "test_buf_to_io_splits";
	const auto out_path = Scratch("b2ios");
	std::filesystem::remove(out_path);
	FIFO src = FromText("ABCDEF");
	ExternalBufferReader in(src);
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(2));
	ASSERT_TRUE(fn, bridge.Passthrough(2));
	ASSERT_TRUE(fn, bridge.Passthrough(2));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDEF"), Slurp(out_path));
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_buf_to_io_writer_not_open() {
	const std::string fn = "test_buf_to_io_writer_not_open";
	const auto out_path = Scratch("b2ion");
	std::filesystem::remove(out_path);
	FIFO src = FromText("NOPE");
	ExternalBufferReader in(src);
	BufferedFileWriter out(out_path, 0, 0);
	Bridge bridge(in, out);
	ASSERT_FALSE(fn, bridge.Passthrough(4));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(4), src.AvailableBytes());
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

// -------------------
// IO → buffer
// -------------------

int test_io_to_buf() {
	const std::string fn = "test_io_to_buf";
	FIFO dst;
	BufferedFileReader in(File("five.bin"));
	ExternalBufferWriter out(dst);
	ASSERT_TRUE(fn, in.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(5));
	ASSERT_EQUAL(fn, std::string("ABCDE"), FifoText(dst));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), in.Tell());
	ASSERT_TRUE(fn, bridge.Passthrough(1));
	ASSERT_TRUE(fn, in.EoF());
	in.Close();
	RETURN_TEST(fn, 0);
}

int test_io_to_buf_splits() {
	const std::string fn = "test_io_to_buf_splits";
	FIFO dst;
	BufferedFileReader in(File("five.bin"));
	ExternalBufferWriter out(dst);
	ASSERT_TRUE(fn, in.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(1));
	ASSERT_TRUE(fn, bridge.Passthrough(4));
	ASSERT_EQUAL(fn, std::string("ABCDE"), FifoText(dst));
	in.Close();
	RETURN_TEST(fn, 0);
}

int test_io_to_buf_reader_not_open() {
	const std::string fn = "test_io_to_buf_reader_not_open";
	FIFO dst;
	BufferedFileReader in(File("five.bin"));
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	ASSERT_FALSE(fn, bridge.Passthrough(5));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	RETURN_TEST(fn, 0);
}

int test_io_to_buf_nul_and_binary() {
	const std::string fn = "test_io_to_buf_nul_and_binary";
	FIFO dst;
	BufferedFileReader in(File("nul.bin"));
	ExternalBufferWriter out(dst);
	ASSERT_TRUE(fn, in.Open());
	Bridge bridge(in, out);
	ASSERT_TRUE(fn, bridge.Passthrough(16));
	ASSERT_EQUAL(fn, Slurp(File("nul.bin")), FifoText(dst));
	in.Close();
	RETURN_TEST(fn, 0);
}

// -------------------
// main
// -------------------

int main() {
	int result = 0;

	// -------------------
	// Buffer → buffer
	// -------------------
	result += test_ext_passthrough_all();
	result += test_ext_passthrough_zero_available_now();
	result += test_ext_passthrough_zero_when_empty();
	result += test_ext_passthrough_consumes_source();
	result += test_ext_multiple_passthrough();
	result += test_ext_flush_is_noop();
	result += test_ext_dtor_flush_noop_data_already_there();
	result += test_ext_flush_and_close();
	result += test_ext_set_error();
	result += test_ext_eof_after_close_source();
	result += test_ext_reader_fail_then_recover();
	result += test_ext_writer_failure();
	result += test_ext_large_transfer();
	result += test_ext_move_bridge();
	result += test_ext_move_assign_bridge();

	// -------------------
	// IO → IO
	// -------------------
	result += test_io_file_to_file();
	result += test_io_file_to_file_short_end();
	result += test_io_passthrough_zero_is_window_only();
	result += test_io_split_then_rest();
	result += test_io_unopened_passthrough_fails();
	result += test_io_chunked_writer_flush();
	result += test_io_flush_and_close_does_not_close_file();
	result += test_io_set_error_noop();
	result += test_io_empty_file();
	result += test_io_pattern_256();
	result += test_io_block_4k_loop();

	// -------------------
	// Buffer → IO
	// -------------------
	result += test_buf_to_io();
	result += test_buf_to_io_zero();
	result += test_buf_to_io_splits();
	result += test_buf_to_io_writer_not_open();

	// -------------------
	// IO → buffer
	// -------------------
	result += test_io_to_buf();
	result += test_io_to_buf_splits();
	result += test_io_to_buf_reader_not_open();
	result += test_io_to_buf_nul_and_binary();

	if (result == 0)
		std::cout << "Bridge tests passed!" << std::endl;
	else
		std::cout << result << " Bridge tests failed." << std::endl;
	return result;
}
