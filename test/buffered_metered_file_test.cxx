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
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/test_handlers.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>

using StormByte::Buffer::DataType;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::BufferedFileWriter;
using StormByte::Buffer::IO::Result;
using StormByte::Buffer::IO::State;
using StormByte::Buffer::IO::Status;
using StormByte::Buffer::IO::ToString;

/**
 * @class BufferedMeteredFileReader
 * @brief Test-only File derivation. Selective override example.
 *
 * Keeps the local file transport: @ref OriginPull delegates to
 * @ref BufferedFileReader::OriginPull and counts octets that the
 * origin actually served (@c Ok / @c End). Prefetch therefore
 * increments the counter. @ref OriginCanSeek is left alone.
 *
 * Extra ctor arguments belong on the derived type (here none).
 * Call @ref Close in the derived destructor.
 */
class BufferedMeteredFileReader: public BufferedFileReader {
	public:
		/**
		 * @brief Same knobs as File. Does not open.
		 * @param path Filesystem path.
		 * @param read_ahead Initial ReadAhead.
		 * @param max_memory Initial MaxMemory.
		 */
		explicit BufferedMeteredFileReader(std::filesystem::path path,
			StormByte::Size read_ahead = 0, StormByte::Size max_memory = 0):
			BufferedFileReader(std::move(path), read_ahead, max_memory) {}

		BufferedMeteredFileReader(const BufferedMeteredFileReader&) = delete;
		BufferedMeteredFileReader& operator=(const BufferedMeteredFileReader&) = delete;

		/**
		 * @brief Close while this vtable is live.
		 */
		~BufferedMeteredFileReader() noexcept override {
			static_cast<void>(Close());
		}

		/**
		 * @brief Origin octets pulled since construction.
		 * @return Cumulative @ref OriginPull count. Not reset on Close.
		 */
		StormByte::Size BytesRead() const noexcept {
			return m_bytes;
		}

	protected:
		/**
		 * @brief Pull from the file, then count a successful serve.
		 * @param n Maximum bytes.
		 * @param dest Base-owned FIFO.
		 * @return Parent result.
		 */
		Result OriginPull(StormByte::Size n, FIFO& dest) override {
			const Result pulled = BufferedFileReader::OriginPull(n, dest);
			if (pulled.status == Status::Ok || pulled.status == Status::End)
				m_bytes += pulled.count;
			return pulled;
		}

	private:
		StormByte::Size m_bytes {0};	///< Cumulative origin octets.
};

/**
 * @class BufferedMeteredFileWriter
 * @brief Test-only File derivation. Selective override example.
 *
 * @ref OriginPush delegates to File and counts octets accepted
 * by the stream. The counter follows the hook, not Dirty.
 */
class BufferedMeteredFileWriter: public BufferedFileWriter {
	public:
		/**
		 * @brief Same knobs as File. Does not open.
		 * @param path Filesystem path.
		 * @param write_chunk Initial WriteChunk.
		 * @param back_pressure Initial BackPressure.
		 */
		explicit BufferedMeteredFileWriter(std::filesystem::path path,
			StormByte::Size write_chunk = 0, std::size_t back_pressure = 0):
			BufferedFileWriter(std::move(path), write_chunk, back_pressure) {}

		BufferedMeteredFileWriter(const BufferedMeteredFileWriter&) = delete;
		BufferedMeteredFileWriter& operator=(const BufferedMeteredFileWriter&) = delete;

		/**
		 * @brief Close while this vtable is live.
		 */
		~BufferedMeteredFileWriter() noexcept override {
			static_cast<void>(Close());
		}

		/**
		 * @brief Origin octets pushed since construction.
		 * @return Cumulative @ref OriginPush count. Not reset on Close.
		 */
		StormByte::Size BytesWritten() const noexcept {
			return m_bytes;
		}

	protected:
		/**
		 * @brief Push to the file, then count a successful serve.
		 * @param data Contiguous octets.
		 * @return Parent result.
		 */
		Result OriginPush(const std::span<const std::byte> data) override {
			const Result pushed = BufferedFileWriter::OriginPush(data);
			if (pushed.status == Status::Ok)
				m_bytes += pushed.count;
			return pushed;
		}

	private:
		StormByte::Size m_bytes {0};	///< Cumulative origin octets.
};

namespace {
	std::filesystem::path File(const char* name) {
		return CurrentFileDirectory / "files" / name;
	}

	std::filesystem::path Scratch(const char* tag) {
		return std::filesystem::temp_directory_path() / (std::string("sbm_") + tag + ".bin");
	}

	std::string Slurp(const std::filesystem::path& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in)
			return {};
		return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	std::string Text(FIFO& fifo) {
		DataType data;
		static_cast<void>(fifo.Peek(0, data));
		return std::string(reinterpret_cast<const char*>(data.data()), data.size());
	}

	FIFO FromText(const std::string& text) {
		FIFO fifo;
		DataType data(text.size());
		for (std::size_t i = 0; i < text.size(); ++i)
			data[i] = static_cast<std::byte>(text[i]);
		static_cast<void>(fifo.Write(data.size(), std::move(data)));
		return fifo;
	}
}

// -------------------
// Reader
// -------------------

int test_metered_reader_before_open() {
	const std::string fn = "test_metered_reader_before_open";
	BufferedMeteredFileReader in(File("five.bin"), 0, 0);
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.BytesRead());
	ASSERT_FALSE(fn, static_cast<bool>(in));
	FIFO dest("KEEP");
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(in.Read(1, dest).status));
	ASSERT_EQUAL(fn, std::string("KEEP"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.BytesRead());
	RETURN_TEST(fn, 0);
}

int test_metered_reader_counts_origin_pull() {
	const std::string fn = "test_metered_reader_counts_origin_pull";
	BufferedMeteredFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	ASSERT_TRUE(fn, in.IsSeekable());
	ASSERT_TRUE(fn, in.IsSized());
	ASSERT_EQUAL(fn, File("five.bin"), in.Path());
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.BytesRead());
	FIFO dest;
	const auto read = in.Read(5, dest);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, read.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.BytesRead());
	RETURN_TEST(fn, 0);
}

int test_metered_reader_prefetch_at_least_requested() {
	const std::string fn = "test_metered_reader_prefetch_at_least_requested";
	BufferedMeteredFileReader in(File("five.bin"), 8, 64);
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	ASSERT_EQUAL(fn, StormByte::Size{1}, in.Read(1, dest).count);
	ASSERT_EQUAL(fn, std::string("A"), Text(dest));
	ASSERT_TRUE(fn, in.BytesRead() >= 1);
	ASSERT_TRUE(fn, in.BytesRead() <= 5);
	ASSERT_EQUAL(fn, StormByte::Size{1}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_metered_reader_seek_and_second_read() {
	const std::string fn = "test_metered_reader_seek_and_second_read";
	BufferedMeteredFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	FIFO first;
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.Read(2, first).count);
	ASSERT_EQUAL(fn, std::string("AB"), Text(first));
	ASSERT_EQUAL(fn, StormByte::Size{2}, in.BytesRead());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(in.Seek(0, Position::Absolute).status));
	ASSERT_EQUAL(fn, StormByte::Size{0}, in.Tell());
	FIFO second;
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Read(3, second).count);
	ASSERT_EQUAL(fn, std::string("ABC"), Text(second));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.BytesRead());
	ASSERT_EQUAL(fn, StormByte::Size{3}, in.Tell());
	RETURN_TEST(fn, 0);
}

int test_metered_reader_short_end() {
	const std::string fn = "test_metered_reader_short_end";
	BufferedMeteredFileReader in(File("five.bin"), 0, 0);
	ASSERT_TRUE(fn, in.Open());
	FIFO dest;
	const auto read = in.Read(8, dest);
	ASSERT_EQUAL(fn, ToString(Status::End), ToString(read.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, read.count);
	ASSERT_EQUAL(fn, std::string("ABCDE"), Text(dest));
	ASSERT_EQUAL(fn, StormByte::Size{5}, in.BytesRead());
	RETURN_TEST(fn, 0);
}

// -------------------
// Writer
// -------------------

int test_metered_writer_append_accumulates() {
	const std::string fn = "test_metered_writer_append_accumulates";
	const auto path = Scratch("app");
	std::filesystem::remove(path);
	BufferedMeteredFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO a = FromText("AB");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(a).status));
	FIFO b = FromText("CD");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(b).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, out.BytesWritten());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("ABCD"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_metered_writer_before_open() {
	const std::string fn = "test_metered_writer_before_open";
	BufferedMeteredFileWriter out(Scratch("pre"), 0, 0);
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.BytesWritten());
	FIFO src = FromText("KEEP");
	ASSERT_EQUAL(fn, ToString(Status::Failed), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{4}, src.AvailableBytes());
	ASSERT_EQUAL(fn, StormByte::Size{0}, out.BytesWritten());
	RETURN_TEST(fn, 0);
}

int test_metered_writer_direct_counts_and_hits_disk() {
	const std::string fn = "test_metered_writer_direct_counts_and_hits_disk";
	const auto path = Scratch("direct");
	std::filesystem::remove(path);
	BufferedMeteredFileWriter out(path, 0, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("HELLO");
	const auto written = out.Write(src);
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(written.status));
	ASSERT_EQUAL(fn, StormByte::Size{5}, written.count);
	ASSERT_EQUAL(fn, StormByte::Size{5}, out.Tell());
	ASSERT_EQUAL(fn, StormByte::Size{5}, out.BytesWritten());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("HELLO"), Slurp(path));
	ASSERT_EQUAL(fn, StormByte::Size{5}, out.BytesWritten());
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int test_metered_writer_flush_does_not_double_count() {
	const std::string fn = "test_metered_writer_flush_does_not_double_count";
	const auto path = Scratch("chunk");
	std::filesystem::remove(path);
	BufferedMeteredFileWriter out(path, 16, 0);
	ASSERT_TRUE(fn, out.Open());
	FIFO src = FromText("XYZ");
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Write(src).status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.BytesWritten());
	ASSERT_EQUAL(fn, ToString(Status::Ok), ToString(out.Flush().status));
	ASSERT_EQUAL(fn, StormByte::Size{3}, out.BytesWritten());
	ASSERT_TRUE(fn, out.Close());
	ASSERT_EQUAL(fn, std::string("XYZ"), Slurp(path));
	std::filesystem::remove(path);
	RETURN_TEST(fn, 0);
}

int main() {
	int result = 0;

	// -------------------
	// Reader
	// -------------------
	result += test_metered_reader_before_open();
	result += test_metered_reader_counts_origin_pull();
	result += test_metered_reader_prefetch_at_least_requested();
	result += test_metered_reader_seek_and_second_read();
	result += test_metered_reader_short_end();

	// -------------------
	// Writer
	// -------------------
	result += test_metered_writer_append_accumulates();
	result += test_metered_writer_before_open();
	result += test_metered_writer_direct_counts_and_hits_disk();
	result += test_metered_writer_flush_does_not_double_count();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
