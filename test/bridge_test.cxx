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
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>
#include <iostream>
#include <memory>
#include <string>
using StormByte::Buffer::Bridge;
using StormByte::Buffer::DataType;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
// ---------------------------------------------------------------------------
// Test helpers – full ExternalReader / ExternalWriter implementations
// ---------------------------------------------------------------------------
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

/**
 * FailingWriter: accepts a limited number of successful Write calls.
 */
class FailingWriter final : public ExternalWriter {
public:
	FailingWriter(FIFO& to, std::size_t succeed_calls) noexcept
		: m_target(to), m_succeed(succeed_calls), m_calls(0), m_closed(false), m_error(false) {}

	bool IsWritable() const noexcept override {
		return !m_closed && !m_error;
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

/**
 * FailingWriterOnce: only the first Write succeeds.
 */
class FailingWriterOnce final : public ExternalWriter {
public:
	explicit FailingWriterOnce(FIFO& to) noexcept
		: m_target(to), m_called(false), m_closed(false), m_error(false) {}

	bool IsWritable() const noexcept override {
		return !m_closed && !m_error;
	}

	bool Write(const DataType& data) noexcept override {
		DataType copy = data;
		return Write(std::move(copy));
	}

	bool Write(DataType&& in) noexcept override {
		if (m_closed || m_error)
			return false;
		if (!m_called) {
			m_called = true;
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
		return MakePointer<FailingWriterOnce>(m_target);
	}
	PointerType Move() noexcept override {
		return MakePointer<FailingWriterOnce>(m_target);
	}

private:
	FIFO& m_target;
	bool m_called;
	bool m_closed;
	bool m_error;
};

// ---------------------------------------------------------------------------
// Original tests (same semantics)
// ---------------------------------------------------------------------------

int test_simple_bridge_passthrough() {
	const std::string fn_name = "test_simple_bridge_passthrough";
	const std::string test_data = "The quick brown fox jumps over the lazy dog.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 16);

	std::size_t total_bytes = source_fifo.Size();
	bool passthrough_success = bridge.Passthrough(total_bytes);
	ASSERT_TRUE(fn_name, passthrough_success);

	bool flush_success = bridge.Flush();
	ASSERT_TRUE(fn_name, flush_success);

	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_little_data_and_flush() {
	const std::string fn_name = "test_little_data_and_flush";
	const std::string test_data = "The quick brown fox jumps over the lazy dog.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer); // default chunk_size 4096

	std::size_t total_bytes = source_fifo.Size();
	bool passthrough_success = bridge.Passthrough(total_bytes);
	ASSERT_TRUE(fn_name, passthrough_success);

	// Data not flushed yet (chunk larger than payload)
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), target_fifo.Size());

	bool flush_success = bridge.Flush();
	ASSERT_TRUE(fn_name, flush_success);

	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_flush_on_destruct() {
	const std::string fn_name = "test_flush_on_destruct";
	const std::string test_data = "The quick brown fox jumps over the lazy dog.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	{
		std::unique_ptr<Bridge> bridge = std::make_unique<Bridge>(reader, writer);

		std::size_t total_bytes = source_fifo.Size();
		bool passthrough_success = bridge->Passthrough(total_bytes);
		ASSERT_TRUE(fn_name, passthrough_success);
	}

	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_reader_false_prevents_write_then_recover() {
	const std::string fn_name = "test_reader_false_prevents_write_then_recover";
	const std::string test_data = "Pack my box with five dozen liquor jugs.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	FaultyReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 16);

	bool first = bridge.Passthrough(8);
	ASSERT_TRUE(fn_name, !first);
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), target_fifo.Size());

	bool second = bridge.Passthrough(source_fifo.AvailableBytes());
	ASSERT_TRUE(fn_name, second);
	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_writer_failure_stops_passthrough() {
	const std::string fn_name = "test_writer_failure_stops_passthrough";
	const std::string test_data = "Sphinx of black quartz, judge my vow.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	FailingWriter writer(target_fifo, 1);
	ExternalBufferReader reader(source_fifo);

	Bridge bridge(reader, writer, 16);
	bool ok = bridge.Passthrough(source_fifo.Size());
	if (ok) {
		bool flushed = bridge.Flush();
		ASSERT_TRUE(fn_name, !flushed);
	} else {
		ASSERT_TRUE(fn_name, !ok);
	}

	ASSERT_TRUE(fn_name, target_fifo.Size() <= test_data.size());

	RETURN_TEST(fn_name, 0);
}

int test_multiple_passthrough_calls() {
	const std::string fn_name = "test_multiple_passthrough_calls";
	const std::string test_data = "How vexingly quick daft zebras jump!";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 16);

	bool ok1 = bridge.Passthrough(10);
	bool ok2 = bridge.Passthrough(10);
	bool ok3 = bridge.Passthrough(source_fifo.AvailableBytes());
	ASSERT_TRUE(fn_name, ok1);
	ASSERT_TRUE(fn_name, ok2);
	ASSERT_TRUE(fn_name, ok3);

	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_passthrough_zero_reads_all() {
	const std::string fn_name = "test_passthrough_zero_reads_all";
	const std::string test_data = "Mr. Jock, TV quiz PhD, bags few lynx.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 32);
	// Original semantics: request all currently available bytes
	bool ok = bridge.Passthrough(source_fifo.AvailableBytes());
	ASSERT_TRUE(fn_name, ok);
	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_destruction_flush_with_failing_writer() {
	const std::string fn_name = "test_destruction_flush_with_failing_writer";
	const std::string test_data = "Waltz, bad nymph, for quick jigs vex.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	FailingWriterOnce writer(target_fifo);

	{
		Bridge bridge(reader, writer, 64);
		bool ok = bridge.Passthrough(10);
		ASSERT_TRUE(fn_name, ok);
		// destructor calls Flush()
	}

	ASSERT_TRUE(fn_name, target_fifo.Size() <= test_data.size());

	RETURN_TEST(fn_name, 0);
}

int test_large_transfer_stress() {
	const std::string fn_name = "test_large_transfer_stress";
	std::string test_data;
	test_data.reserve(200 * 1024);
	for (size_t i = 0; i < 200 * 1024; ++i)
		test_data.push_back(static_cast<char>('A' + (i % 26)));

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 4096);
	// Original test used Passthrough(0) meaning "all available" via reader
	bool ok = bridge.Passthrough(0);
	ASSERT_TRUE(fn_name, ok);
	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_chunk_size_zero_passthrough_no_flush() {
	const std::string fn_name = "test_chunk_size_zero_passthrough_no_flush";
	const std::string test_data = "Chunkless passthrough test data: 0123456789";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 0);

	std::size_t total_bytes = source_fifo.Size();
	bool ok = bridge.Passthrough(total_bytes);
	ASSERT_TRUE(fn_name, ok);

	ASSERT_EQUAL(fn_name, test_data.size(), static_cast<std::size_t>(target_fifo.Size()));
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_const_bridge_passthrough_non_destructive() {
	const std::string fn_name = "test_const_bridge_passthrough_non_destructive";
	const std::string test_data = "Const bridge passthrough test.";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 16);

	std::size_t size_before = source_fifo.Size();

	const Bridge& cbridge = bridge;
	bool ok = cbridge.Passthrough(source_fifo.AvailableBytes());
	ASSERT_TRUE(fn_name, ok);

	// Storage size unchanged; logical available bytes consumed by Read
	ASSERT_EQUAL(fn_name, size_before, source_fifo.Size());
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), source_fifo.AvailableBytes());

	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

// ---------------------------------------------------------------------------
// New tests for the enriched API
// ---------------------------------------------------------------------------

int test_flush_and_close() {
	const std::string fn_name = "test_flush_and_close";
	const std::string test_data = "Flush and close me";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 4096);
	ASSERT_TRUE(fn_name, bridge.Passthrough(test_data.size()));
	ASSERT_TRUE(fn_name, bridge.IsWritable());

	ASSERT_TRUE(fn_name, bridge.FlushAndClose());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));
	ASSERT_TRUE(fn_name, !bridge.IsWritable());

	// Further writes through the writer should fail
	ASSERT_TRUE(fn_name, !target_fifo.IsWritable());

	RETURN_TEST(fn_name, 0);
}

int test_bridge_set_error_propagation() {
	const std::string fn_name = "test_bridge_set_error_propagation";
	const std::string test_data = "error path";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 16);
	ASSERT_TRUE(fn_name, bridge.IsWritable());
	ASSERT_TRUE(fn_name, bridge.IsReadable());

	bridge.SetError();
	ASSERT_TRUE(fn_name, !bridge.IsWritable());

	RETURN_TEST(fn_name, 0);
}

int test_bridge_eof_delegation() {
	const std::string fn_name = "test_bridge_eof_delegation";
	const std::string test_data = "eof";

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);
	source_fifo.Close();

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 8);

	// Drain everything
	ASSERT_TRUE(fn_name, bridge.Passthrough(0));
	ASSERT_TRUE(fn_name, bridge.Flush());

	// Source closed + empty → EoF
	ASSERT_TRUE(fn_name, bridge.EoF());

	RETURN_TEST(fn_name, 0);
}

int test_pending_bytes_invariant() {
	const std::string fn_name = "test_pending_bytes_invariant";
	const std::string test_data = "0123456789ABCDEFGHIJ"; // 20 bytes

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	const std::size_t chunk = 8;
	Bridge bridge(reader, writer, chunk);

	ASSERT_TRUE(fn_name, bridge.Passthrough(5));
	ASSERT_TRUE(fn_name, bridge.PendingBytes() < chunk);
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(5), bridge.PendingBytes());

	ASSERT_TRUE(fn_name, bridge.Passthrough(10));
	ASSERT_TRUE(fn_name, bridge.PendingBytes() < chunk);

	// Drenar el resto del source
	ASSERT_TRUE(fn_name, bridge.Passthrough(source_fifo.AvailableBytes()));
	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), bridge.PendingBytes());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_chunk_size_accessor() {
	const std::string fn_name = "test_chunk_size_accessor";

	FIFO source_fifo;
	FIFO target_fifo;
	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 1234);
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(1234), bridge.ChunkSize());

	Bridge bridge0(reader, writer, 0);
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), bridge0.ChunkSize());

	RETURN_TEST(fn_name, 0);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	int result = 0;

	result += test_simple_bridge_passthrough();
	result += test_little_data_and_flush();
	result += test_flush_on_destruct();
	result += test_reader_false_prevents_write_then_recover();
	result += test_writer_failure_stops_passthrough();
	result += test_multiple_passthrough_calls();
	result += test_passthrough_zero_reads_all();
	result += test_destruction_flush_with_failing_writer();
	result += test_large_transfer_stress();
	result += test_chunk_size_zero_passthrough_no_flush();
	result += test_const_bridge_passthrough_non_destructive();

	// New coverage
	result += test_flush_and_close();
	result += test_bridge_set_error_propagation();
	result += test_bridge_eof_delegation();
	result += test_pending_bytes_invariant();
	result += test_chunk_size_accessor();

	if (result == 0) {
		std::cout << "Bridge tests passed!" << std::endl;
	} else {
		std::cout << result << " Bridge tests failed." << std::endl;
	}
	return result;
}
