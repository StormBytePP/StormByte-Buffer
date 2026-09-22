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
#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>
#include <StormByte/system.hxx>
#include <StormByte/test_handlers.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using StormByte::Buffer::Bridge;
using StormByte::Buffer::DataType;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::SharedFIFO;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::BufferedFileWriter;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::ToString;
using StormByte::Buffer::IO::Drainer::ToString;
using StormByte::Buffer::IO::Drainer::Operation;
using StormByte::Buffer::IO::Drainer::Status;
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

static bool WaitFifoSize(const FIFO& fifo, const std::size_t n) {
	for (int i = 0; i < 200; ++i) {
		if (fifo.Size() >= n)
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return fifo.Size() >= n;
}

static bool WaitFile(const std::filesystem::path& path, const std::size_t n) {
	for (int i = 0; i < 200; ++i) {
		if (Slurp(path).size() >= n)
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return Slurp(path).size() >= n;
}

static bool WaitDirtyZero(BufferedFileWriter& out) {
	for (int i = 0; i < 80; ++i) {
		if (out.Dirty() == 0)
			return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return out.Dirty() == 0;
}

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

int test_ext_drains_all() {
	const std::string fn = "test_ext_drains_all";
	const std::string text = "The quick brown fox jumps over the lazy dog.";
	FIFO src = FromText(text);
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 64);
	ASSERT_EQUAL(fn, ToString(Status::Started), ToString(bridge.Drainer()));
	ASSERT_TRUE(fn, WaitFifoSize(dst, text.size()));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, text, FifoText(dst));
	ASSERT_TRUE(fn, bridge.EoF());
	RETURN_TEST(fn, 0);
}

int test_ext_high_water_zero_starts_paused() {
	const std::string fn = "test_ext_high_water_zero_starts_paused";
	FIFO src = FromText("ABCDE");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 0);
	ASSERT_EQUAL(fn, ToString(Status::Paused), ToString(bridge.Drainer()));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	RETURN_TEST(fn, 0);
}

int test_ext_flush_and_close() {
	const std::string fn = "test_ext_flush_and_close";
	FIFO src = FromText("CLOSEME");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 16);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 7));
	ASSERT_TRUE(fn, bridge.FlushAndClose());
	ASSERT_FALSE(fn, out.IsWritable());
	RETURN_TEST(fn, 0);
}

int test_ext_set_error() {
	const std::string fn = "test_ext_set_error";
	FIFO src = FromText("ERR");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 16);
	bridge.SetError();
	ASSERT_FALSE(fn, bridge.IsWritable());
	RETURN_TEST(fn, 0);
}

int test_ext_eof_after_close_source() {
	const std::string fn = "test_ext_eof_after_close_source";
	FIFO src = FromText("X");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 8);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 1));
	ASSERT_TRUE(fn, bridge.EoF());
	RETURN_TEST(fn, 0);
}

int test_ext_writer_failure() {
	const std::string fn = "test_ext_writer_failure";
	FIFO src = FromText("NOPE");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	FailingWriter out(dst, 0);
	Bridge bridge(in, out, 16);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_FALSE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	RETURN_TEST(fn, 0);
}

int test_ext_move_bridge() {
	const std::string fn = "test_ext_move_bridge";
	FIFO src = FromText("MOVEOK");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge first(in, out, 16);
	Bridge second(std::move(first));
	ASSERT_EQUAL(fn, ToString(Status::Stopped), ToString(first.Drainer()));
	ASSERT_FALSE(fn, first.Drainer(Operation::Toggle));
	ASSERT_TRUE(fn, WaitFifoSize(dst, 6));
	ASSERT_EQUAL(fn, std::string("MOVEOK"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_ext_move_assign_bridge() {
	const std::string fn = "test_ext_move_assign_bridge";
	FIFO src_a = FromText("AAAA");
	src_a.Close();
	FIFO dst_a;
	ExternalBufferReader in_a(src_a);
	ExternalBufferWriter out_a(dst_a);
	Bridge a(in_a, out_a, 16);

	FIFO src_b = FromText("BBBB");
	src_b.Close();
	FIFO dst_b;
	ExternalBufferReader in_b(src_b);
	ExternalBufferWriter out_b(dst_b);
	Bridge b(in_b, out_b, 16);
	b = std::move(a);
	ASSERT_EQUAL(fn, ToString(Status::Stopped), ToString(a.Drainer()));
	ASSERT_TRUE(fn, WaitFifoSize(dst_a, 4));
	ASSERT_EQUAL(fn, std::string("AAAA"), FifoText(dst_a));
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
	Bridge bridge(in, out, 16);
	ASSERT_TRUE(fn, WaitFile(out_path, 5));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("ABCDE"), Slurp(out_path));
	ASSERT_TRUE(fn, in.EoF());
	in.Close();
	out.Close();
	std::filesystem::remove(out_path);
	RETURN_TEST(fn, 0);
}

int test_io_unopened_does_not_write() {
	const std::string fn = "test_io_unopened_does_not_write";
	const auto out_path = Scratch("unopen");
	std::filesystem::remove(out_path);
	BufferedFileReader in(File("five.bin"));
	BufferedFileWriter out(out_path, 0, 0);
	Bridge bridge(in, out, 16);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_FALSE(fn, bridge.IsReadable());
	ASSERT_FALSE(fn, bridge.IsWritable());
	ASSERT_EQUAL(fn, std::string(""), Slurp(out_path));
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
	Bridge bridge(in, out, 16);
	ASSERT_TRUE(fn, WaitFile(out_path, 5));
	ASSERT_TRUE(fn, bridge.FlushAndClose());
	ASSERT_TRUE(fn, static_cast<bool>(out));
	ASSERT_EQUAL(fn, ToString(State::Idle), ToString(out.State()));
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
	Bridge bridge(in, out, 16);
	bridge.SetError();
	ASSERT_TRUE(fn, WaitFile(out_path, 5));
	ASSERT_TRUE(fn, bridge.IsWritable());
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
	Bridge bridge(in, out, 16);
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
	Bridge bridge(in, out, 512);
	ASSERT_TRUE(fn, WaitFile(out_path, 256));
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, Slurp(File("pattern_256.bin")), Slurp(out_path));
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
	src.Close();
	ExternalBufferReader in(src);
	BufferedFileWriter out(out_path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	Bridge bridge(in, out, 16);
	ASSERT_TRUE(fn, WaitFile(out_path, 5));
	ASSERT_TRUE(fn, WaitDirtyZero(out));
	ASSERT_EQUAL(fn, std::string("HELLO"), Slurp(out_path));
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
	Bridge bridge(in, out, 16);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
	Bridge bridge(in, out, 16);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 5));
	ASSERT_EQUAL(fn, std::string("ABCDE"), FifoText(dst));
	ASSERT_TRUE(fn, in.EoF());
	in.Close();
	RETURN_TEST(fn, 0);
}

int test_io_to_buf_reader_not_open() {
	const std::string fn = "test_io_to_buf_reader_not_open";
	FIFO dst;
	BufferedFileReader in(File("five.bin"));
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 16);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	RETURN_TEST(fn, 0);
}

int test_io_to_buf_nul_and_binary() {
	const std::string fn = "test_io_to_buf_nul_and_binary";
	FIFO dst;
	BufferedFileReader in(File("nul.bin"));
	ExternalBufferWriter out(dst);
	ASSERT_TRUE(fn, in.Open());
	Bridge bridge(in, out, 16);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 5));
	ASSERT_EQUAL(fn, Slurp(File("nul.bin")), FifoText(dst));
	in.Close();
	RETURN_TEST(fn, 0);
}

int test_io_to_buf_pattern() {
	const std::string fn = "test_io_to_buf_pattern";
	FIFO dst;
	BufferedFileReader in(File("pattern_256.bin"));
	ExternalBufferWriter out(dst);
	ASSERT_TRUE(fn, in.Open());
	Bridge bridge(in, out, 512);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 256));
	ASSERT_EQUAL(fn, Slurp(File("pattern_256.bin")), FifoText(dst));
	in.Close();
	RETURN_TEST(fn, 0);
}

// -------------------
// Drainer Toggle / Flush
// -------------------

int test_toggle_pauses_and_resumes_buffer() {
	const std::string fn = "test_toggle_pauses_and_resumes_buffer";
	FIFO src = FromText("HELLO");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 64);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 5));
	ASSERT_EQUAL(fn, std::string("HELLO"), FifoText(dst));

	ASSERT_TRUE(fn, bridge.Drainer(Operation::Toggle));
	ASSERT_EQUAL(fn, ToString(Status::Paused), ToString(bridge.Drainer()));
	ASSERT_TRUE(fn, src.Write("WORLD"));
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	ASSERT_EQUAL(fn, std::string("HELLO"), FifoText(dst));

	ASSERT_TRUE(fn, bridge.Drainer(Operation::Toggle));
	ASSERT_EQUAL(fn, ToString(Status::Started), ToString(bridge.Drainer()));
	ASSERT_TRUE(fn, WaitFifoSize(dst, 10));
	ASSERT_EQUAL(fn, std::string("HELLOWORLD"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_toggle_from_ctor_paused() {
	const std::string fn = "test_toggle_from_ctor_paused";
	FIFO src = FromText("ABC");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 0);
	ASSERT_EQUAL(fn, ToString(Status::Paused), ToString(bridge.Drainer()));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	bridge.HighWater(16);
	ASSERT_EQUAL(fn, ToString(Status::Paused), ToString(bridge.Drainer()));
	ASSERT_TRUE(fn, bridge.Drainer(Operation::Toggle));
	ASSERT_TRUE(fn, WaitFifoSize(dst, 3));
	ASSERT_EQUAL(fn, std::string("ABC"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_high_water_setter_does_not_toggle() {
	const std::string fn = "test_high_water_setter_does_not_toggle";
	FIFO src = FromText("XYZ");
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 0);
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), bridge.HighWater());
	bridge.HighWater(32);
	ASSERT_EQUAL(fn, static_cast<std::size_t>(32), bridge.HighWater());
	ASSERT_EQUAL(fn, ToString(Status::Paused), ToString(bridge.Drainer()));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), dst.Size());
	RETURN_TEST(fn, 0);
}

int test_drainer_flush_hurry_on_buffer() {
	const std::string fn = "test_drainer_flush_hurry_on_buffer";
	FIFO src = FromText("ABCDEFGH");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 64);
	ASSERT_TRUE(fn, WaitFifoSize(dst, 8));
	ASSERT_TRUE(fn, bridge.Drainer(Operation::Flush));
	ASSERT_EQUAL(fn, std::string("ABCDEFGH"), FifoText(dst));
	ASSERT_EQUAL(fn, ToString(Status::Started), ToString(bridge.Drainer()));
	RETURN_TEST(fn, 0);
}

int test_bridge_flush_is_barrier() {
	const std::string fn = "test_bridge_flush_is_barrier";
	FIFO src = FromText("FLUSHME");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 32);
	ASSERT_TRUE(fn, bridge.Flush());
	ASSERT_EQUAL(fn, std::string("FLUSHME"), FifoText(dst));
	RETURN_TEST(fn, 0);
}

int test_drainer_ops_on_moved_from() {
	const std::string fn = "test_drainer_ops_on_moved_from";
	FIFO src = FromText("Z");
	src.Close();
	FIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge first(in, out, 8);
	Bridge second(std::move(first));
	ASSERT_EQUAL(fn, ToString(Status::Stopped), ToString(first.Drainer()));
	ASSERT_FALSE(fn, first.Drainer(Operation::Toggle));
	ASSERT_FALSE(fn, first.Drainer(Operation::Flush));
	ASSERT_FALSE(fn, first.Flush());
	RETURN_TEST(fn, 0);
}

int test_backpressure_shared_fifo() {
	const std::string fn = "test_backpressure_shared_fifo";
	const std::string text = "ABCDEFGHIJKLMNOP";
	FIFO src = FromText(text);
	src.Close();
	SharedFIFO dst;
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out, 4);

	std::atomic<bool> stop {false};
	DataType collected;
	std::thread consumer([&] {
		while (!stop.load() || dst.AvailableBytes() > 0) {
			if (dst.AvailableBytes() == 0) {
				std::this_thread::yield();
				continue;
			}
			DataType chunk;
			const std::size_t n = std::min<std::size_t>(2, dst.AvailableBytes());
			if (dst.Extract(n, chunk))
				collected.insert(collected.end(), chunk.begin(), chunk.end());
		}
	});

	for (int i = 0; i < 200 && collected.size() < text.size(); ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	stop.store(true);
	consumer.join();
	ASSERT_EQUAL(fn, text, StormByte::String::FromByteVector(collected));
	RETURN_TEST(fn, 0);
}

int main() {
	int result = 0;

	// -------------------
	// Buffer → buffer
	// -------------------
	result += test_ext_drains_all();
	result += test_ext_high_water_zero_starts_paused();
	result += test_ext_flush_and_close();
	result += test_ext_set_error();
	result += test_ext_eof_after_close_source();
	result += test_ext_writer_failure();
	result += test_ext_move_bridge();
	result += test_ext_move_assign_bridge();

	// -------------------
	// IO → IO
	// -------------------
	result += test_io_file_to_file();
	result += test_io_unopened_does_not_write();
	result += test_io_flush_and_close_does_not_close_file();
	result += test_io_set_error_noop();
	result += test_io_empty_file();
	result += test_io_pattern_256();

	// -------------------
	// Buffer → IO
	// -------------------
	result += test_buf_to_io();
	result += test_buf_to_io_writer_not_open();

	// -------------------
	// IO → buffer
	// -------------------
	result += test_io_to_buf();
	result += test_io_to_buf_reader_not_open();
	result += test_io_to_buf_nul_and_binary();
	result += test_io_to_buf_pattern();

	// -------------------
	// Drainer Toggle / Flush
	// -------------------
	result += test_toggle_pauses_and_resumes_buffer();
	result += test_toggle_from_ctor_paused();
	result += test_high_water_setter_does_not_toggle();
	result += test_drainer_flush_hurry_on_buffer();
	result += test_bridge_flush_is_barrier();
	result += test_drainer_ops_on_moved_from();
	result += test_backpressure_shared_fifo();

	if (result == 0)
		std::cout << "Bridge tests passed!" << std::endl;
	else
		std::cout << result << " Bridge tests failed." << std::endl;
	return result;
}
