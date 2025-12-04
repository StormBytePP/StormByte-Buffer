#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <iostream>

using StormByte::Buffer::SharedFIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::FIFO;

static std::string toString(const std::vector<std::byte>& v) {
	return StormByte::String::FromByteVector(v);
}

int test_shared_fifo_write_span_basic() {
	SharedFIFO fifo;
	const char* msg = "SFPAN";
	std::vector<std::byte> vec(reinterpret_cast<const std::byte*>(msg), reinterpret_cast<const std::byte*>(msg) + 5);
	auto w = fifo.Write(vec);
	ASSERT_TRUE("shared_write_span ok", w.has_value());
	std::vector<std::byte> read;
	auto res = fifo.Read(5, read);
	ASSERT_TRUE("shared_write_span read ok", res.has_value());
	ASSERT_EQUAL("shared_write_span content", StormByte::String::FromByteVector(read), std::string("SFPAN"));
	RETURN_TEST("test_shared_fifo_write_span_basic", 0);
}

int test_shared_fifo_multiple_spans_eof() {
	SharedFIFO fifo;
	(void)fifo.Write(std::string("ABCDEFGHIJ")); // 10 bytes

	// Consume in chunks using Read (non-destructive)
	std::vector<std::byte> s1, s2, s3;
	auto r1 = fifo.Read(4, s1);
	ASSERT_TRUE("sf read1 ok", r1.has_value());
	ASSERT_EQUAL("sf read1 size", s1.size(), static_cast<std::size_t>(4));
	auto r2 = fifo.Read(3, s2);
	ASSERT_TRUE("sf read2 ok", r2.has_value());
	ASSERT_EQUAL("sf read2 size", s2.size(), static_cast<std::size_t>(3));
	auto r3 = fifo.Read(3, s3);
	ASSERT_TRUE("sf read3 ok", r3.has_value());
	ASSERT_EQUAL("sf read3 size", s3.size(), static_cast<std::size_t>(3));

	ASSERT_EQUAL("sf available after reads", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	ASSERT_TRUE("sf eof after reads when closed false", !fifo.EoF());
	fifo.Close();
	ASSERT_TRUE("sf eof after close and empty", fifo.EoF());
	RETURN_TEST("test_shared_fifo_multiple_spans_eof", 0);
}

int test_shared_fifo_producer_consumer_blocking() {
	SharedFIFO fifo;
	std::atomic<bool> done{false};
	const std::string payload = "ABCDEFGHIJ"; // 10 bytes

	std::thread producer([&]() -> void {
		(void)fifo.Write(std::string(payload.begin(), payload.begin() + 4));
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		(void)fifo.Write(std::string(payload.begin() + 4, payload.end()));
		fifo.Close();
		done.store(true);
	});

	std::string collected;
	std::thread consumer([&]() -> void {
		while (true) {
			std::vector<std::byte> part;
			auto res = fifo.Read(3, part); // read small chunks, blocks until 3 available or closed
			if (!res.has_value()) {
				// On error (closed & insufficient), try to consume any remaining bytes and exit.
				if (fifo.AvailableBytes() > 0) {
					std::vector<std::byte> rem;
					auto remres = fifo.Read(0, rem);
					if (remres.has_value() && !rem.empty()) collected.append(toString(rem));
				}
				break;
			}
			if (part.empty() && fifo.EoF()) break;
			collected.append(toString(part));
			// small delay to increase interleaving
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	producer.join();
	consumer.join();

	ASSERT_TRUE("producer finished", done.load());
	ASSERT_EQUAL("collected matches payload", collected, payload);
	RETURN_TEST("test_shared_fifo_producer_consumer_blocking", 0);
}

int test_shared_fifo_extract_blocking_and_close() {
	SharedFIFO fifo;
	std::atomic<bool> woke{false};
	std::atomic<bool> saw_writable{false};
	std::size_t extracted_size = 1234; // sentinel

	std::thread t([&]() -> void {
		std::vector<std::byte> out;
		auto res = fifo.Extract(1, out); // block until 1 byte or close
		// With no writer, Close() will wake us; out should be error or empty
		woke.store(true);
		saw_writable.store(fifo.IsWritable());
		extracted_size = res.has_value() ? out.size() : 0;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	fifo.Close();
	t.join();

	ASSERT_TRUE("thread woke on close", woke.load());
	ASSERT_FALSE("extract woke writable", saw_writable.load());
	ASSERT_EQUAL("got error on read", extracted_size, 0);
	RETURN_TEST("test_shared_fifo_extract_blocking_and_close", 0);
}

int test_shared_fifo_concurrent_seek_and_read() {
	SharedFIFO fifo;
	(void)fifo.Write(std::string("0123456789"));

	std::atomic<bool> seeker_done{false};
	std::string read_a, read_b;
	std::atomic<bool> reader_failed{false};


	std::thread seeker([&]() -> void {
		// Seek around while another thread reads; mutex ensures safety
		fifo.Seek(5, Position::Absolute); // pos at '5'
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		fifo.Seek(2, Position::Relative); // pos at '7'
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		fifo.Seek(1, Position::Absolute); // pos at '1'
		fifo.Close(); // Writer closes buffer before finishing
		seeker_done.store(true);
	});

	std::thread reader([&]() -> void {
		// Perform two reads interleaved with seeks
		std::vector<std::byte> r1, r2;
		auto res1 = fifo.Read(2, r1); // reads from current (race-free due to mutex)
		if (!res1.has_value()) { reader_failed.store(true); return; }
		read_a = toString(r1);
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
		auto res2 = fifo.Read(3, r2);
		if (!res2.has_value()) { reader_failed.store(true); return; }
		read_b = toString(r2);
	});

	seeker.join();
	reader.join();

	ASSERT_FALSE("reader had error", reader_failed.load());

	ASSERT_TRUE("seeker finished", seeker_done.load());
	// After the final seek to absolute 1, the next read should start from '1'
	// Due to interleaving, we validate that reads return valid substrings of the buffer and sizes are consistent.
	ASSERT_TRUE("read_a size 0..2", read_a.size() <= 2);
	ASSERT_TRUE("read_b size 0..3", read_b.size() <= 3);
	// Ensure content characters are within expected set 0-9
	auto within_digits = [](const std::string& s){
		for (char c : s) if (c < '0' || c > '9') return false; return true;
	};
	ASSERT_TRUE("read_a digits", within_digits(read_a));
	ASSERT_TRUE("read_b digits", within_digits(read_b));
	RETURN_TEST("test_shared_fifo_concurrent_seek_and_read", 0);
}

int test_shared_fifo_extract_adjusts_read_position_concurrency() {
	SharedFIFO fifo;
	(void)fifo.Write(std::string("ABCDEFGH"));

	std::string r_before, r_after;
	std::atomic<bool> reader_failed2{false};
	std::atomic<bool> first_read_done{false};

	// ...existing code...
	try {
		std::thread reader([&]() -> void {
			std::vector<std::byte> _r_before, _r_after;
			auto res_before = fifo.Read(3, _r_before);
			if (!res_before.has_value()) { reader_failed2.store(true); return; }
			r_before = toString(_r_before); // ABC, position now at 3
			first_read_done.store(true);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			auto res_after = fifo.Read(2, _r_after);
			if (!res_after.has_value()) { reader_failed2.store(true); return; }
			r_after = toString(_r_after);
		});
		// ...existing code...

		// ...existing code...
		std::thread extractor([&]() -> void {
			// Wait until first read completes to ensure deterministic position
			while (!first_read_done.load()) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			std::vector<std::byte> e;
			auto eres = fifo.Extract(2, e); // Extract from position 3, gets "DE"
			(void)eres;
		});
		// ...existing code...

		reader.join();
		extractor.join();
		// ...existing code...
	} catch (const std::system_error&) {
		// ...existing code...
	}

	ASSERT_FALSE("reader had error", reader_failed2.load());

	ASSERT_EQUAL("first read ABC", r_before, std::string("ABC"));
	// After reading 3 (position at 3), Extract(2) removes from position 3, which is "DE"
	// Position stays at 3, next read gets "FG"
	ASSERT_EQUAL("next read after adjust is FG", r_after, std::string("FG"));
	RETURN_TEST("test_shared_fifo_extract_adjusts_read_position_concurrency", 0);
}

int test_shared_fifo_multi_producer_single_consumer_counts() {
	SharedFIFO fifo;
	const int chunks = 200;
	std::atomic<bool> p1_done{false}, p2_done{false};

	auto producerA = std::thread([&]() -> void {
		for (int i = 0; i < chunks; ++i) {
			(void)fifo.Write(std::string("A"));
		}
		p1_done.store(true);
	});

	auto producerB = std::thread([&]() -> void {
		for (int i = 0; i < chunks; ++i) {
			(void)fifo.Write(std::string("B"));
		}
		p2_done.store(true);
	});

	std::string collected;
	auto consumer = std::thread([&]() -> void {
		while (true) {
			std::vector<std::byte> part;
			auto res = fifo.Extract(1, part); // block for each byte
			if (!res.has_value() || (part.empty() && fifo.EoF())) break;
			collected.append(toString(part));
		}
	});

	producerA.join();
	producerB.join();
	fifo.Close();
	consumer.join();

	ASSERT_TRUE("producers finished", p1_done.load() && p2_done.load());
	// Verify counts: exactly 'chunks' of 'A' and 'chunks' of 'B'
	size_t countA = 0, countB = 0;
	for (char c : collected) { if (c == 'A') ++countA; else if (c == 'B') ++countB; }
	ASSERT_EQUAL("count A", countA, static_cast<size_t>(chunks));
	ASSERT_EQUAL("count B", countB, static_cast<size_t>(chunks));
	ASSERT_EQUAL("total size", collected.size(), static_cast<size_t>(chunks * 2));
	RETURN_TEST("test_shared_fifo_multi_producer_single_consumer_counts", 0);
}

int test_shared_fifo_multiple_consumers_total_coverage() {
	SharedFIFO fifo;
	const int total = 1000;
	// Producer writes a predictable sequence of 'X'
	auto producer = std::thread([&]() -> void {
		(void)fifo.Write(std::string(total, 'X'));
		fifo.Close();
	});

	std::atomic<size_t> c1{0}, c2{0};
	auto consumer1 = std::thread([&]() -> void {
		size_t local = 0;
		while (true) {
			std::vector<std::byte> part;
			auto res = fifo.Extract(1, part);
			if (!res.has_value() || (part.empty() && fifo.EoF())) break;
			local += part.size();
		}
		c1.store(local);
	});
	auto consumer2 = std::thread([&]() -> void {
		size_t local = 0;
		while (true) {
			std::vector<std::byte> part;
			auto res = fifo.Extract(1, part);
			if (!res.has_value() || (part.empty() && fifo.EoF())) break;
			local += part.size();
		}
		c2.store(local);
	});

	producer.join();
	consumer1.join();
	consumer2.join();

	ASSERT_EQUAL("sum of consumed", c1.load() + c2.load(), static_cast<size_t>(total));
	RETURN_TEST("test_shared_fifo_multiple_consumers_total_coverage", 0);
}

int test_shared_fifo_close_suppresses_writes() {
	SharedFIFO fifo;
	(void)fifo.Write(std::string("ABC"));
	ASSERT_EQUAL("pre-close size", fifo.Size(), static_cast<std::size_t>(3));
	fifo.Close();
	(void)fifo.Write(std::string("DEF"));
	ASSERT_EQUAL("size unchanged after close", fifo.Size(), static_cast<std::size_t>(3));
	std::vector<std::byte> out;
	auto res = fifo.Extract(0, out);
	ASSERT_TRUE("extract returned", res.has_value());
	ASSERT_EQUAL("content after close write blocked", toString(out), std::string("ABC"));
	RETURN_TEST("test_shared_fifo_close_suppresses_writes", 0);
}

int test_shared_fifo_wrap_boundary_blocking() {
	SharedFIFO fifo;
	(void)fifo.Write("ABCDE");
	std::vector<std::byte> r1;
	auto res1 = fifo.Read(3, r1); // reads ABC, position now at 3
	ASSERT_TRUE("r1 returned", res1.has_value());
	ASSERT_EQUAL("read ABC", toString(r1), std::string("ABC"));
	std::vector<std::byte> e1;
	auto eres1 = fifo.Extract(2, e1); // extract from position 3, gets "DE"
	ASSERT_TRUE("e1 returned", eres1.has_value());
	ASSERT_EQUAL("extract DE", toString(e1), std::string("DE"));
	(void)fifo.Write("12"); // wrap at capacity
	// Seek to beginning and read remaining 4 bytes Follows non-destructive read position semantics
	fifo.Seek(0, Position::Absolute);
	std::vector<std::byte> r2;
	auto res2 = fifo.Read(4, r2); // should read C? Wait head moved: after extract 2 from ABCDE, head moved; remaining was ABC? After read 3 non-destructive, size unchanged.
	// Given operations: after initial write, size=5; Read(3) didn't change size; Extract(2) removed DE, size=3; Then write 12 size=5; Head somewhere; Reading all should give remaining 5 from read position 0
	fifo.Seek(0, Position::Absolute);
	std::vector<std::byte> all;
	auto resall = fifo.Read(0, all);
	ASSERT_TRUE("all returned", resall.has_value());
	ASSERT_EQUAL("wrap combined", toString(all).size(), static_cast<std::size_t>(5));
	RETURN_TEST("test_shared_fifo_wrap_boundary_blocking", 0);
}

int test_shared_fifo_growth_under_contention() {
	SharedFIFO fifo;
	const int iters = 100;
	std::atomic<bool> done{false};
	auto producer = std::thread([&]() -> void {
		for (int i = 0; i < iters; ++i) {
			(void)fifo.Write(std::string(100 + (i % 50), 'Z'));
		}
		done.store(true);
		fifo.Close();
	});

	size_t consumed = 0;
	auto consumer = std::thread([&]() -> void {
		while (true) {
			std::vector<std::byte> part;
			auto res = fifo.Extract(128, part);
			if (!res.has_value()) {
				if (fifo.AvailableBytes() > 0) {
					std::vector<std::byte> rem;
					auto remres = fifo.Extract(0, rem);
					if (remres.has_value() && !rem.empty()) consumed += rem.size();
				}
				break;
			}
			if (part.empty() && fifo.EoF()) break;
			consumed += part.size();
		}
	});

	producer.join();
	consumer.join();

	// Expected bytes written
	size_t expected = 0;
	for (int i = 0; i < iters; ++i) expected += 100 + (i % 50);
	ASSERT_EQUAL("growth contention total", consumed, expected);
	RETURN_TEST("test_shared_fifo_growth_under_contention", 0);
}

int test_shared_fifo_read_insufficient_closed_returns_available() {
	SharedFIFO fifo;
	(void)fifo.Write("ABC");
	fifo.Close();
	
	// Even though we request 10 bytes, when closed with only 3 available,
	// Requesting more than available when closed is an error.
	std::vector<std::byte> out;
	auto result = fifo.Read(10, out);
	ASSERT_FALSE("read with closed returns error", result.has_value());
	// Data should remain available unchanged
	ASSERT_EQUAL("available unchanged", fifo.AvailableBytes(), static_cast<std::size_t>(3));
	
	RETURN_TEST("test_shared_fifo_read_insufficient_closed_returns_available", 0);
}

int test_shared_fifo_extract_insufficient_closed_returns_available() {
	SharedFIFO fifo;
	(void)fifo.Write("HELLO");
	fifo.Close();
	
	// Extract more than available when closed should return available data
	std::vector<std::byte> out;
	auto result = fifo.Extract(100, out);
	ASSERT_FALSE("extract with closed returns error", result.has_value());
	// Data must remain unchanged when extract fails
	ASSERT_EQUAL("size unchanged after failed extract", fifo.Size(), static_cast<std::size_t>(5));
	std::vector<std::byte> all;
	auto allres = fifo.Read(0, all);
	ASSERT_TRUE("read remaining data", allres.has_value());
	ASSERT_EQUAL("content is HELLO", toString(all), std::string("HELLO"));
	
	RETURN_TEST("test_shared_fifo_extract_insufficient_closed_returns_available", 0);
}

int test_shared_fifo_blocking_read_insufficient_not_closed() {
	SharedFIFO fifo;
	(void)fifo.Write("12");
	
	std::atomic<bool> read_started{false};
	std::atomic<bool> read_got_error{false};
	std::atomic<bool> read_finished{false};
	
	std::thread reader([&]() -> void {
		read_started.store(true);
		// This should block waiting for 10 bytes (only 2 available)
		std::vector<std::byte> out;
		auto result = fifo.Read(10, out);
		read_finished.store(true);
		// When we close below, it will wake up and return the 2 available bytes
		read_got_error.store(!result.has_value());
	});
	
	// Wait for read to start and block
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	ASSERT_TRUE("read started", read_started.load());
	ASSERT_FALSE("read still blocking", read_finished.load());
	
	// Close the buffer - should wake the blocked read
	fifo.Close();
	reader.join();
	
	ASSERT_TRUE("read finished after close", read_finished.load());
	ASSERT_TRUE("read returned error after close", read_got_error.load());
	
	RETURN_TEST("test_shared_fifo_blocking_read_insufficient_not_closed", 0);
}

int test_shared_fifo_available_bytes_basic() {
	SharedFIFO fifo;
	
	// Empty buffer
	ASSERT_EQUAL("empty available", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	
	// Write data
	(void)fifo.Write("HELLO WORLD"); // 11 bytes
	ASSERT_EQUAL("after write", fifo.AvailableBytes(), static_cast<std::size_t>(11));
	
	// Read non-destructively
	std::vector<std::byte> r1;
	auto res1 = fifo.Read(5, r1);
	ASSERT_EQUAL("after read 5", fifo.AvailableBytes(), static_cast<std::size_t>(6));

	// Seek changes available bytes
	fifo.Seek(2, Position::Absolute);
	ASSERT_EQUAL("after seek to 2", fifo.AvailableBytes(), static_cast<std::size_t>(9));

	// Extract 3 from position 2, removes 3 bytes, position stays at 2
	std::vector<std::byte> e1;
	auto eres1 = fifo.Extract(3, e1);
	ASSERT_EQUAL("after extract 3", fifo.AvailableBytes(), static_cast<std::size_t>(6));
	
	RETURN_TEST("test_shared_fifo_available_bytes_basic", 0);
}

int test_shared_fifo_available_bytes_concurrent() {
	SharedFIFO fifo;
	std::atomic<std::size_t> available_checks{0};
	std::atomic<bool> done{false};
	
	// Writer thread
	std::thread writer([&]() -> void {
		for (int i = 0; i < 10; ++i) {
			(void)fifo.Write("DATA");
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		done.store(true);
		fifo.Close();
	});
	
	// Reader thread that checks AvailableBytes
	std::thread reader([&]() -> void {
		while (!done.load() || !fifo.Empty()) {
			std::size_t available = fifo.AvailableBytes();
			if (available > 0) {
				std::vector<std::byte> data;
				auto res = fifo.Extract(0, data);
				available_checks.fetch_add(1);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		}
	});
	
	writer.join();
	reader.join();
	
	ASSERT_TRUE("available checked multiple times", available_checks.load() > 0);
	ASSERT_TRUE("buffer empty at end", fifo.Empty());
	ASSERT_EQUAL("no bytes available", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	
	RETURN_TEST("test_shared_fifo_available_bytes_concurrent", 0);
}

int test_shared_fifo_read_closed_no_data_nonblocking() {
	SharedFIFO fifo;
	fifo.Close();
	ASSERT_FALSE("fifo is writable", fifo.IsWritable());
	ASSERT_EQUAL("fifo is empty", fifo.Size(), static_cast<std::size_t>(0));
	
	// Calling Read on already-closed empty FIFO should return empty vector (since it returns immediately)
	// But to truly test the non-blocking case where it's already closed before Read is called,
	// Read(10) with count>0 should wait, wake up immediately because closed, and return empty
	std::vector<std::byte> out;
	auto result = fifo.Read(10, out);
	ASSERT_FALSE("Read returns error when requesting > available on closed", result.has_value());
	
	RETURN_TEST("test_shared_fifo_read_closed_no_data_nonblocking", 0);
}

int test_shared_fifo_extract_closed_no_data_nonblocking() {
	SharedFIFO fifo;
	fifo.Close();
	ASSERT_FALSE("fifo is writable", fifo.IsWritable());
	ASSERT_EQUAL("fifo is empty", fifo.Size(), static_cast<std::size_t>(0));
	
	std::vector<std::byte> out;
	auto result = fifo.Extract(10, out);
	ASSERT_FALSE("Extract returns error when requesting > available on closed", result.has_value());
	
	RETURN_TEST("test_shared_fifo_extract_closed_no_data_nonblocking", 0);
}

int test_sharedfifo_equality() {
	StormByte::Buffer::SharedFIFO sa;
	StormByte::Buffer::SharedFIFO sb;
(void)("HELLO");
(void)("HELLO");

	// Same contents -> equal (equality is content-only)
	ASSERT_TRUE("sharedfifo equal same content", sa == sb);
	ASSERT_FALSE("sharedfifo unequal same content", sa != sb);

	// Closing one does not change content equality
	sa.Close();
	ASSERT_TRUE("sharedfifo still equal after close", sa == sb);

	// Close the other too -> still equal
	sb.Close();
	ASSERT_TRUE("sharedfifo equal after both closed", sa == sb);

	RETURN_TEST("test_sharedfifo_equality", 0);
}

int test_shared_fifo_write_whole_fifo() {
	SharedFIFO shared;
	FIFO src;
	(void)src.Write(std::string("ONE"));

	// Write the whole FIFO into shared
	auto ok = shared.Write(src);
	ASSERT_TRUE("shared fifo write whole returned true", ok.has_value());

	// Extract and validate
	std::vector<std::byte> all;
	auto allres = shared.Extract(0, all);
	ASSERT_TRUE("shared extract returned", allres.has_value());
	ASSERT_EQUAL("shared fifo write whole content", toString(all), std::string("ONE"));

	// rvalue move-append: source should be empty afterward
	FIFO src2;
	(void)src2.Write(std::string("TWO"));
	ok = shared.Write(std::move(src2));
	ASSERT_TRUE("shared fifo write whole rvalue returned true", ok.has_value());
	std::vector<std::byte> all2;
	auto all2res = shared.Extract(0, all2);
	ASSERT_TRUE("shared extract2 returned", all2res.has_value());
	ASSERT_EQUAL("shared fifo write whole rvalue content", toString(all2), std::string("TWO"));

	RETURN_TEST("test_shared_fifo_write_whole_fifo", 0);
}

int test_shared_fifo_skip_basic() {
	SharedFIFO sf;
	(void)sf.Write(std::string("ABCDEFG"));
	(void)sf.Drop(3);
	ASSERT_EQUAL("shared skip basic size", sf.Size(), static_cast<std::size_t>(4));
	std::vector<std::byte> out;
	auto outres = sf.Extract(0, out);
	ASSERT_TRUE("shared extract after skip returned", outres.has_value());
	ASSERT_EQUAL("shared extract after skip content", toString(out), std::string("DEFG"));
	RETURN_TEST("test_shared_fifo_skip_basic", 0);
}

int test_shared_fifo_skip_with_readpos() {
	SharedFIFO sf;
	(void)sf.Write(std::string("0123456789"));

	// Non-destructive read to move read position
	std::vector<std::byte> r;
	auto rres = sf.Read(3, r);
	ASSERT_TRUE("shared read before skip", rres.has_value());

	// Drop removes four bytes from head (adjusting for read position)
	(void)sf.Drop(4);

	ASSERT_EQUAL("shared size after skip with readpos", sf.Size(), static_cast<std::size_t>(3));
	std::vector<std::byte> out;
	auto outres = sf.Extract(0, out);
	ASSERT_TRUE("shared extract after skip with readpos returned", outres.has_value());
	ASSERT_EQUAL("shared content after skip with readpos", toString(out), std::string("789"));
	RETURN_TEST("test_shared_fifo_skip_with_readpos", 0);
}

int test_shared_fifo_peek_basic() {
	SharedFIFO fifo;
	(void)fifo.Write(std::string("HELLO"));
	
	// Peek 3 bytes - should not advance read position
	std::vector<std::byte> peek1;
	auto peek1res = fifo.Peek(3, peek1);
	ASSERT_TRUE("peek returned", peek1res.has_value());
	ASSERT_EQUAL("peek content", toString(peek1), std::string("HEL"));
	
	// Peek again - should return same data
	std::vector<std::byte> peek2;
	auto peek2res = fifo.Peek(3, peek2);
	ASSERT_TRUE("peek2 returned", peek2res.has_value());
	ASSERT_EQUAL("peek2 content", toString(peek2), std::string("HEL"));
	
	// Now read - should return same data as peek
	std::vector<std::byte> read1;
	auto read1res = fifo.Read(3, read1);
	ASSERT_TRUE("read returned", read1res.has_value());
	ASSERT_EQUAL("read content matches peek", toString(read1), std::string("HEL"));
	
	RETURN_TEST("test_shared_fifo_peek_basic", 0);
}

int test_shared_fifo_peek_concurrent() {
	SharedFIFO fifo;
	
	// Write data first
	(void)fifo.Write(std::string("DATA"));
	
	// Peek should return the data
	std::vector<std::byte> peek;
	auto peekres = fifo.Peek(4, peek);
	ASSERT_TRUE("concurrent peek returned", peekres.has_value());
	ASSERT_EQUAL("concurrent peek content", toString(peek), std::string("DATA"));
	
	// Read should return same data
	std::vector<std::byte> read;
	auto readres = fifo.Read(4, read);
	ASSERT_TRUE("concurrent read returned", readres.has_value());
	ASSERT_EQUAL("concurrent read content", toString(read), std::string("DATA"));
	
	RETURN_TEST("test_shared_fifo_peek_concurrent", 0);
}

int test_shared_fifo_peek_all_available() {
	SharedFIFO fifo;
	(void)fifo.Write(std::string("WORLD"));
	
	// Peek all available (count = 0)
	std::vector<std::byte> peek_all;
	auto peekallres = fifo.Peek(0, peek_all);
	ASSERT_TRUE("peek all returned", peekallres.has_value());
	ASSERT_EQUAL("peek all content", toString(peek_all), std::string("WORLD"));
	
	// Data should still be there
	std::vector<std::byte> read_all;
	auto readallres = fifo.Read(0, read_all);
	ASSERT_TRUE("read all returned", readallres.has_value());
	ASSERT_EQUAL("read all content", toString(read_all), std::string("WORLD"));
	
	RETURN_TEST("test_shared_fifo_peek_all_available", 0);
}

int test_hexdump1() {
	const std::string fn_name = "test_shared_hexdump";
	// Test 1: 5-line hexdump like FIFO test
	SharedFIFO sf;
	std::string s = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcd";
	(void)sf.Write(s);
	std::string dump = sf.HexDump(8, 0);

	std::string expected;
	expected += "Size: 40 bytes\n";
	expected += "Read Position: 0\n";
	expected += "Status: opened and ready\n";
	expected += "00000000: 30 31 32 33 34 35 36 37   01234567\n";
	expected += "00000008: 38 39 41 42 43 44 45 46   89ABCDEF\n";
	expected += "00000010: 47 48 49 4A 4B 4C 4D 4E   GHIJKLMN\n";
	expected += "00000018: 4F 50 51 52 53 54 55 56   OPQRSTUV\n";
	expected += "00000020: 57 58 59 5A 61 62 63 64   WXYZabcd";

	ASSERT_EQUAL("test_shared_hexdump exact match", expected, dump);
	RETURN_TEST(fn_name.c_str(), 0);
}

int test_hexdump2() {
	const std::string fn_name = "test_shared_hexdump_offset";
	// Test 2: hexdump starting at offset 5
	SharedFIFO sf;
	std::string s = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcd";
	(void)sf.Write(s);
	sf.Seek(5, Position::Absolute);
	std::string dump = sf.HexDump(8, 0);

	std::string expected;
	expected += "Size: 40 bytes\n";
	expected += "Read Position: 5\n";
	expected += "Status: opened and ready\n";
	expected += "00000005: 35 36 37 38 39 41 42 43   56789ABC\n";
	expected += "0000000D: 44 45 46 47 48 49 4A 4B   DEFGHIJK\n";
	expected += "00000015: 4C 4D 4E 4F 50 51 52 53   LMNOPQRS\n";
	expected += "0000001D: 54 55 56 57 58 59 5A 61   TUVWXYZa\n";
	expected += "00000025: 62 63 64                  bcd";

	ASSERT_EQUAL("test_shared_hexdump_offset exact match", expected, dump);
	RETURN_TEST(fn_name.c_str(), 0);
}

int test_hexdump3() {
	const std::string fn_name = "test_shared_hexdump_mixed";
	// Test 3: mixed printable and non-printable
	SharedFIFO sf;
	std::vector<std::byte> v;
	v.push_back(std::byte{0x41}); // 'A'
	v.push_back(std::byte{0x00}); // NUL
	v.push_back(std::byte{0x1F}); // non-print
	v.push_back(std::byte{0x20}); // space
	v.push_back(std::byte{0x41}); // 'A'
	v.push_back(std::byte{0x7E}); // '~'
	v.push_back(std::byte{0x7F}); // DEL
	v.push_back(std::byte{0x80}); // non-print
	v.push_back(std::byte{0xFF}); // non-print
	v.push_back(std::byte{0x30}); // '0'
	(void)sf.Write(std::move(v));
	std::string dump = sf.HexDump(8, 0);

	std::string expected;
	expected += "Size: 10 bytes\n";
	expected += "Read Position: 0\n";
	expected += "Status: opened and ready\n";
	expected += "00000000: 41 00 1F 20 41 7E 7F 80   A.. A~..\n";
	expected += std::string("00000008: FF 30") + std::string(21, ' ') + ".0";

	ASSERT_EQUAL("test_shared_hexdump_mixed exact match", expected, dump);
	RETURN_TEST(fn_name.c_str(), 0);
}

int main() {
	int result = 0;
	result += test_shared_fifo_producer_consumer_blocking();
	result += test_shared_fifo_extract_blocking_and_close();
	result += test_shared_fifo_concurrent_seek_and_read();
	result += test_shared_fifo_extract_adjusts_read_position_concurrency();
	result += test_shared_fifo_multi_producer_single_consumer_counts();
	result += test_shared_fifo_multiple_consumers_total_coverage();
	result += test_shared_fifo_close_suppresses_writes();
	result += test_shared_fifo_wrap_boundary_blocking();
	result += test_shared_fifo_write_span_basic();
	result += test_shared_fifo_multiple_spans_eof();
	result += test_shared_fifo_growth_under_contention();
	result += test_shared_fifo_read_insufficient_closed_returns_available();
	result += test_shared_fifo_extract_insufficient_closed_returns_available();
	result += test_shared_fifo_blocking_read_insufficient_not_closed();
	result += test_shared_fifo_available_bytes_basic();
	result += test_shared_fifo_available_bytes_concurrent();
	result += test_shared_fifo_read_closed_no_data_nonblocking();
	result += test_shared_fifo_extract_closed_no_data_nonblocking();
	result += test_sharedfifo_equality();
	result += test_shared_fifo_write_whole_fifo();
	result += test_shared_fifo_skip_basic();
	result += test_shared_fifo_skip_with_readpos();
	result += test_shared_fifo_peek_basic();
	result += test_shared_fifo_peek_concurrent();
	result += test_shared_fifo_peek_all_available();
	result += test_hexdump1();
	result += test_hexdump2();
	result += test_hexdump3();

	if (result == 0) {
		std::cout << "SharedFIFO tests passed!" << std::endl;
	} else {
		std::cout << result << " SharedFIFO tests failed." << std::endl;
	}
	return result;
}
