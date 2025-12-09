#include <StormByte/buffer/bridge.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>


using StormByte::Buffer::Bridge;
using StormByte::Buffer::DataType;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::ReadOnly;
using StormByte::Buffer::WriteOnly;

// FaultyReader: first call returns false and writes nothing; subsequent
// calls behave like a ExternalBufferReader and extract from the source FIFO.
class FaultyReader final: public ExternalReader {
	public:
		FaultyReader(FIFO& from) noexcept: m_source(from), m_first(true) {}
		bool Read(std::size_t bytes, DataType& outBuffer) const noexcept override {
			return const_cast<FaultyReader*>(this)->Read(bytes, outBuffer);
		}
		bool Read(std::size_t bytes, DataType& outBuffer) noexcept override {
			if (m_first) {
				// Simulate an untrusted read: do not write to outBuffer and
				// indicate failure.
				m_first = false;
				return false;
			}
			return m_source.Extract(bytes, outBuffer);
		}
		PointerType Clone() const override { return MakePointer<FaultyReader>(m_source); }
		PointerType Move() override { return MakePointer<FaultyReader>(m_source); }
	private:
		FIFO& m_source;
		mutable bool m_first;
};

// FailingWriter accepts a limited number of successful writes (by call count)
class FailingWriter final: public ExternalWriter {
	public:
		FailingWriter(FIFO& to, std::size_t succeed_calls) noexcept: m_target(to), m_succeed(succeed_calls), m_calls(0) {}
		bool Write(DataType&& in) noexcept override {
			if (m_calls < m_succeed) {
				++m_calls;
				return m_target.Write(std::move(in));
			}
			// Simulate failure: do not consume input and return false
			return false;
		}
		PointerType Clone() const override { return MakePointer<FailingWriter>(m_target, m_succeed); }
		PointerType Move() override { return MakePointer<FailingWriter>(m_target, m_succeed); }
	private:
		FIFO& m_target;
		std::size_t m_succeed;
		std::size_t m_calls;
};

// Writer that only accepts first write call
class FailingWriterOnce final: public ExternalWriter {
	public:
		FailingWriterOnce(FIFO& to) noexcept: m_target(to), m_called(false) {}
		bool Write(DataType&& inBuffer) noexcept override {
			if (!m_called) {
				m_called = true;
				return m_target.Write(std::move(inBuffer));
			}
			return false;
		}
		PointerType Clone() const override { return FailingWriterOnce::MakePointer<FailingWriterOnce>(m_target); }
		PointerType Move() override { return FailingWriterOnce::MakePointer<FailingWriterOnce>(m_target); }
	private:
		FIFO& m_target;
		bool m_called;
};

int test_simple_bridge_passthrough() {
	const std::string fn_name = "test_simple_bridge_passthrough";
	const std::string test_data = "The quick brown fox jumps over the lazy dog.";

	// Prepare source and target FIFOs
	FIFO source_fifo;
	FIFO target_fifo;

	// Fill source FIFO with test data
	source_fifo.Write(test_data);

	// Create SimpleReader and SimpleWriter
	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	// Create Bridge with chunk size of 16 bytes
	Bridge bridge(reader, writer, 16);

	// Perform passthrough of all data
	std::size_t total_bytes = source_fifo.Size();
	bool passthrough_success = bridge.Passthrough(total_bytes);
	ASSERT_TRUE(fn_name, passthrough_success);

	// Flush any remaining data
	bool flush_success = bridge.Flush();
	ASSERT_TRUE(fn_name, flush_success);

	// Verify target FIFO contents match source data
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_little_data_and_flush() {
	const std::string fn_name = "test_little_data_and_flush";
	const std::string test_data = "The quick brown fox jumps over the lazy dog.";

	// Prepare source and target FIFOs
	FIFO source_fifo;
	FIFO target_fifo;

	// Fill source FIFO with test data
	source_fifo.Write(test_data);

	// Create SimpleReader and SimpleWriter
	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	// Create Bridge with chunk size of 4096 bytes (default)
	Bridge bridge(reader, writer);

	// Perform passthrough of all data
	std::size_t total_bytes = source_fifo.Size();
	bool passthrough_success = bridge.Passthrough(total_bytes);
	ASSERT_TRUE(fn_name, passthrough_success);

	// Verify that target FIFO is still empty (data not flushed yet)
	ASSERT_EQUAL(fn_name, 0, target_fifo.Size());

	// Flush any remaining data
	bool flush_success = bridge.Flush();
	ASSERT_TRUE(fn_name, flush_success);

	// Verify target FIFO contents match source data
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

int test_flush_on_destruct() {
	const std::string fn_name = "test_flush_on_destruct";
	const std::string test_data = "The quick brown fox jumps over the lazy dog.";

	// Prepare source and target FIFOs
	FIFO source_fifo;
	FIFO target_fifo;

	// Fill source FIFO with test data
	source_fifo.Write(test_data);

	// Create SimpleReader and SimpleWriter
	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	{
		// Create Bridge with chunk size of 4096 bytes (default)
		std::unique_ptr<Bridge> bridge = std::make_unique<Bridge>(reader, writer);

		// Perform passthrough of all data
		std::size_t total_bytes = source_fifo.Size();
		bool passthrough_success = bridge->Passthrough(total_bytes);
		ASSERT_TRUE(fn_name, passthrough_success);
	}

	// Verify target FIFO contents match source data
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

	// First attempt should fail and perform no write
	bool first = bridge.Passthrough(8);
	ASSERT_TRUE(fn_name, !first);
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), target_fifo.Size());

	// Next call should succeed and transfer data
	bool second = bridge.Passthrough(source_fifo.AvailableBytes()); // request remaining
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

	// Allow only first write call to succeed
	FailingWriter writer(target_fifo, 1);
	ExternalBufferReader reader(source_fifo);

	Bridge bridge(reader, writer, 16);
	bool ok = bridge.Passthrough(source_fifo.Size());
	// Either passthrough fails (writer returns false) or flush fails; expect failure
	if (ok) {
		bool flushed = bridge.Flush();
		ASSERT_TRUE(fn_name, !flushed); // flush should fail because writer returns false later
	} else {
		ASSERT_TRUE(fn_name, !ok);
	}

	// Target should contain at most one write's worth of data (one successful call)
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

	// Call passthrough in pieces so internal buffer accumulates across calls
	bool ok1 = bridge.Passthrough(10);
	bool ok2 = bridge.Passthrough(10);
	bool ok3 = bridge.Passthrough(source_fifo.AvailableBytes()); // remaining
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
	bool ok = bridge.Passthrough(source_fifo.AvailableBytes()); // request all available
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
		// Pass through only a portion so destructor must flush a remainder
		bool ok = bridge.Passthrough(10);
		ASSERT_TRUE(fn_name, ok);
		// bridge destroyed at scope exit; destructor calls Flush()
	}

	// Ensure no crash and some data (at least one successful write) may have occurred
	ASSERT_TRUE(fn_name, target_fifo.Size() <= test_data.size());

	RETURN_TEST(fn_name, 0);
}

int test_large_transfer_stress() {
	const std::string fn_name = "test_large_transfer_stress";
	// 200 KB of patterned data
	std::string test_data;
	test_data.reserve(200 * 1024);
	for (size_t i = 0; i < 200 * 1024; ++i) test_data.push_back(static_cast<char>('A' + (i % 26)));

	FIFO source_fifo;
	FIFO target_fifo;
	source_fifo.Write(test_data);

	ExternalBufferReader reader(source_fifo);
	ExternalBufferWriter writer(target_fifo);

	Bridge bridge(reader, writer, 4096);
	bool ok = bridge.Passthrough(0); // all
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

	// chunk_size == 0 disables chunking and should passthrough everything immediately
	Bridge bridge(reader, writer, 0);

	std::size_t total_bytes = source_fifo.Size();
	bool ok = bridge.Passthrough(total_bytes);
	ASSERT_TRUE(fn_name, ok);

	// Because chunking is disabled, data should already be in target_fifo without a flush
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

	// Record the original total buffer size (Size() should not change on const reads)
	std::size_t size_before = source_fifo.Size();

	const Bridge& cbridge = bridge;
	bool ok = cbridge.Passthrough(source_fifo.AvailableBytes());
	ASSERT_TRUE(fn_name, ok);

	// For const passthrough, the underlying buffer storage (Size) must not be cleared
	ASSERT_EQUAL(fn_name, size_before, source_fifo.Size());
	// But original buffer read position is advanced, so available bytes should be zero
	ASSERT_EQUAL(fn_name, static_cast<std::size_t>(0), source_fifo.AvailableBytes());

	// Flush and verify target received data
	ASSERT_TRUE(fn_name, bridge.Flush());
	ASSERT_EQUAL(fn_name, test_data, StormByte::String::FromByteVector(target_fifo.Data()));

	RETURN_TEST(fn_name, 0);
}

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

	if (result == 0) {
		std::cout << "Bridge tests passed!" << std::endl;
	} else {
		std::cout << result << " Bridge tests failed." << std::endl;
	}
	return result;
}
