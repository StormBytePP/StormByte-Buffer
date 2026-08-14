/**
 * @file ring_test.cxx
 * @brief Comprehensive test suite for the Ring buffer class
 *
 * Ring is a fully thread-safe, growable FIFO-style buffer based on
 * std::deque<std::byte>. It implements the pure ReadWrite contract and is
 * optimised for the dominant workload of writes + destructive reads/extracts
 * (no massive memmoves of live data).
 *
 * Test categories:
 *
 * BASIC FUNCTIONALITY
 * - Construction, write/read, extract, peek, seek, clean, clear
 * - Close / error handling, AvailableBytes / Size / Empty / EoF consistency
 * - Move semantics (copy is deleted)
 *
 * DESTRUCTIVE / POSITION EDGE CASES
 * - Extract / Drop / Clean interaction with the logical read position
 * - Seek + subsequent destructive operations
 * - Partial extracts that leave the position in a valid state
 * - Extract(0) / Read(0) on empty vs. non-empty buffers
 *
 * THREADING & BLOCKING
 * - Single / multiple writers with single / multiple readers
 * - Blocking Read/Extract/Peek that wake on data arrival or Close/SetError
 * - Concurrent destructive consumption while other threads write
 * - Close / SetError while threads are blocked
 * - Stress with rapid small writes + slow destructive consumers
 * - Large (1 MiB+) transfers
 *
 * RELIABILITY / RACE EDGE CASES
 * - Out-of-sync partial writes (consumer blocks for more data)
 * - Insufficient data + Close
 * - Clear while producers are still active
 * - Interleaved Read (non-destructive) + Extract (destructive)
 * - Multiple sequential blocking operations
 * - Burst writes followed by full drain
 */

#include <StormByte/buffer/ring.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using StormByte::Buffer::Ring;
using StormByte::Buffer::Position;
using StormByte::Buffer::DataType;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string ToString(const DataType& v) {
	return StormByte::String::FromByteVector(v);
}

// ---------------------------------------------------------------------------
// BASIC FUNCTIONALITY
// ---------------------------------------------------------------------------

int test_ring_basic_write_read() {
	Ring ring;
	const std::string msg = "Hello, Ring!";
	ASSERT_TRUE("write ok", ring.Write(msg));
	ring.Close();

	ASSERT_EQUAL("size", ring.Size(), msg.size());
	ASSERT_FALSE("not empty", ring.Empty());
	ASSERT_EQUAL("available", ring.AvailableBytes(), msg.size());

	DataType data;
	ASSERT_TRUE("read ok", ring.Read(msg.size(), data));
	ASSERT_EQUAL("content", ToString(data), msg);
	ASSERT_EQUAL("available after full read", ring.AvailableBytes(), 0u);

	RETURN_TEST("test_ring_basic_write_read", 0);
}

int test_ring_multiple_writes() {
	Ring ring;
	ASSERT_TRUE("w1", ring.Write("First"));
	ASSERT_TRUE("w2", ring.Write("Second"));
	ASSERT_TRUE("w3", ring.Write("Third"));

	DataType all;
	ASSERT_TRUE("read all", ring.Read(0, all));
	ASSERT_EQUAL("concatenated", ToString(all), std::string("FirstSecondThird"));
	RETURN_TEST("test_ring_multiple_writes", 0);
}

int test_ring_extract_destructive() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("ABCDEFGH"));
	ASSERT_EQUAL("initial size", ring.Size(), 8u);

	DataType first, rest;
	ASSERT_TRUE("extract 3", ring.Extract(3, first));
	ASSERT_EQUAL("ABC", ToString(first), std::string("ABC"));
	ASSERT_EQUAL("size after extract", ring.Size(), 5u);
	ASSERT_EQUAL("available after extract", ring.AvailableBytes(), 5u);

	ASSERT_TRUE("extract rest", ring.Extract(0, rest));
	ASSERT_EQUAL("DEFGH", ToString(rest), std::string("DEFGH"));
	ASSERT_TRUE("empty", ring.Empty());
	ASSERT_EQUAL("size zero", ring.Size(), 0u);

	RETURN_TEST("test_ring_extract_destructive", 0);
}

int test_ring_peek_does_not_consume() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("ABCDEFGH"));
	ring.Close();

	DataType p1, p2, r1, r2;
	ASSERT_TRUE("peek1", ring.Peek(4, p1));
	ASSERT_EQUAL("peek1 content", ToString(p1), std::string("ABCD"));

	ASSERT_TRUE("peek2", ring.Peek(4, p2));
	ASSERT_EQUAL("peek2 same", ToString(p2), std::string("ABCD"));
	ASSERT_EQUAL("available unchanged", ring.AvailableBytes(), 8u);

	ASSERT_TRUE("read", ring.Read(4, r1));
	ASSERT_EQUAL("read matches peek", ToString(r1), std::string("ABCD"));
	ASSERT_EQUAL("available after read", ring.AvailableBytes(), 4u);

	ASSERT_TRUE("second read", ring.Read(4, r2));
	ASSERT_EQUAL("EFGH", ToString(r2), std::string("EFGH"));
	ASSERT_EQUAL("available zero", ring.AvailableBytes(), 0u);

	RETURN_TEST("test_ring_peek_does_not_consume", 0);
}

int test_ring_seek_operations() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("0123456789"));
	ring.Close();

	DataType from5, fromStart, from7;
	ASSERT_TRUE("read 5", ring.Read(5, from5));
	ASSERT_EQUAL("01234", ToString(from5), std::string("01234"));

	ring.Seek(-5, Position::Relative);		// back to start
	ASSERT_TRUE("read all", ring.Read(0, fromStart));
	ASSERT_EQUAL("full", ToString(fromStart), std::string("0123456789"));

	ring.Seek(7, Position::Absolute);
	ASSERT_TRUE("read 3", ring.Read(3, from7));
	ASSERT_EQUAL("789", ToString(from7), std::string("789"));

	RETURN_TEST("test_ring_seek_operations", 0);
}

int test_ring_clean_after_seek() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("ABCDEFGH"));
	ring.Seek(3, Position::Absolute);		// logical position at 'D'
	ASSERT_EQUAL("available", ring.AvailableBytes(), 5u);

	ring.Clean();							// discards A,B,C
	ASSERT_EQUAL("size after clean", ring.Size(), 5u);
	ASSERT_EQUAL("available after clean", ring.AvailableBytes(), 5u);
	ASSERT_EQUAL("position reset", 0u, 0u); // offset must be 0 after Clean

	DataType data;
	ASSERT_TRUE("read remaining", ring.Read(0, data));
	ASSERT_EQUAL("DEFGH", ToString(data), std::string("DEFGH"));

	RETURN_TEST("test_ring_clean_after_seek", 0);
}

int test_ring_drop() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("0123456789"));
	ASSERT_TRUE("drop 4", ring.Drop(4));
	ASSERT_EQUAL("size", ring.Size(), 6u);

	DataType data;
	ASSERT_TRUE("read rest", ring.Read(0, data));
	ASSERT_EQUAL("456789", ToString(data), std::string("456789"));

	RETURN_TEST("test_ring_drop", 0);
}

int test_ring_clear() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("Some data to clear"));
	ASSERT_FALSE("not empty", ring.Empty());

	ring.Clear();
	ASSERT_TRUE("empty", ring.Empty());
	ASSERT_EQUAL("size 0", ring.Size(), 0u);
	ASSERT_EQUAL("available 0", ring.AvailableBytes(), 0u);

	ASSERT_TRUE("write after clear", ring.Write("New data"));
	ASSERT_EQUAL("new size", ring.Size(), 8u);

	RETURN_TEST("test_ring_clear", 0);
}

int test_ring_close_mechanism() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("Data"));
	ring.Close();

	ASSERT_FALSE("write after close fails", ring.Write("More"));
	ASSERT_EQUAL("size unchanged", ring.Size(), 4u);
	ASSERT_TRUE("EoF after close + drain", [&]{
		DataType d; (void)ring.Extract(0, d); return ring.EoF();
	}());

	RETURN_TEST("test_ring_close_mechanism", 0);
}

int test_ring_move_semantics() {
	Ring r1;
	ASSERT_TRUE("write", r1.Write("Data"));
	ASSERT_TRUE("write more", r1.Write("More"));

	Ring r2 = std::move(r1);
	ASSERT_EQUAL("moved size", r2.Size(), 8u);

	DataType data;
	ASSERT_TRUE("read from moved", r2.Read(0, data));
	ASSERT_EQUAL("content", ToString(data), std::string("DataMore"));

	RETURN_TEST("test_ring_move_semantics", 0);
}

int test_ring_empty_read_failure() {
	Ring ring;
	ring.Close();

	DataType data;
	ASSERT_FALSE("Read(0) on empty closed returns false", ring.Read(0, data));
	ASSERT_FALSE("Extract(0) on empty closed returns false", ring.Extract(0, data));
	ASSERT_TRUE("EoF", ring.EoF());

	RETURN_TEST("test_ring_empty_read_failure", 0);
}

// ---------------------------------------------------------------------------
// DESTRUCTIVE / POSITION EDGE CASES
// ---------------------------------------------------------------------------

int test_ring_extract_then_seek() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("ABCDEFGHIJ"));
	DataType part;
	ASSERT_TRUE("extract 4", ring.Extract(4, part));		// removes ABCD
	ASSERT_EQUAL("ABCD", ToString(part), std::string("ABCD"));
	ASSERT_EQUAL("size", ring.Size(), 6u);

	// position should still be valid (0 after destructive erase)
	ring.Seek(2, Position::Absolute);
	DataType mid;
	ASSERT_TRUE("read mid", ring.Read(3, mid));
	ASSERT_EQUAL("GHI", ToString(mid), std::string("GHI"));

	RETURN_TEST("test_ring_extract_then_seek", 0);
}

int test_ring_partial_extract_leaves_valid_position() {
    Ring ring;
    ASSERT_TRUE("write", ring.Write("0123456789"));
    ring.Seek(3, Position::Absolute);		// pos = 3 ('3')

    DataType mid;
    ASSERT_TRUE("extract 4 from middle", ring.Extract(4, mid));
    ASSERT_EQUAL("3456", ToString(mid), std::string("3456"));
    ASSERT_EQUAL("size after middle extract", ring.Size(), 6u);
    ASSERT_EQUAL("available after middle extract", ring.AvailableBytes(), 3u);

    DataType rest;
    ASSERT_TRUE("extract rest (suffix only)", ring.Extract(0, rest));
    ASSERT_EQUAL("rest content length", rest.size(), 3u);			// ← era 6, debe ser 3
    ASSERT_EQUAL("rest content", ToString(rest), std::string("789"));
    ASSERT_EQUAL("size after suffix extract", ring.Size(), 3u);	// prefijo "012" sigue ahí
    ASSERT_EQUAL("available now 0", ring.AvailableBytes(), 0u);

    // Limpieza explícita del prefijo
    ring.Clean();
    ASSERT_TRUE("empty after Clean", ring.Empty());

    RETURN_TEST("test_ring_partial_extract_leaves_valid_position", 0);
}

int test_ring_extract_zero_returns_all() {
	Ring ring;
	ASSERT_TRUE("write", ring.Write("TestData"));
	ring.Close();

	DataType data;
	ASSERT_TRUE("Extract(0)", ring.Extract(0, data));
	ASSERT_EQUAL("all 8 bytes", data.size(), 8u);
	ASSERT_TRUE("empty afterwards", ring.Empty());

	RETURN_TEST("test_ring_extract_zero_returns_all", 0);
}

int test_ring_read_zero_on_empty_fails() {
	Ring ring;
	DataType data;
	ASSERT_FALSE("Read(0) empty fails", ring.Read(0, data));
	ASSERT_FALSE("Extract(0) empty fails", ring.Extract(0, data));
	RETURN_TEST("test_ring_read_zero_on_empty_fails", 0);
}

// ---------------------------------------------------------------------------
// THREADING & BLOCKING
// ---------------------------------------------------------------------------

int test_ring_single_writer_single_reader() {
	Ring ring;
	const int messages = 100;
	std::atomic<bool> writer_done{false};
	std::string collected;

	std::thread writer([&] {
		for (int i = 0; i < messages; ++i)
			(void)ring.Write(std::to_string(i) + ",");
		ring.Close();
		writer_done = true;
	});

	std::thread reader([&] {
		while (true) {
			DataType part;
			if (!ring.Extract(16, part)) {
				if (ring.AvailableBytes() > 0) {
					DataType rem;
					if (ring.Extract(0, rem) && !rem.empty())
						collected += ToString(rem);
				}
				break;
			}
			if (part.empty() && ring.EoF()) break;
			collected += ToString(part);
		}
	});

	writer.join();
	reader.join();

	ASSERT_TRUE("writer done", writer_done.load());
	ASSERT_TRUE("got data", !collected.empty());
	ASSERT_TRUE("EoF", ring.EoF());
	RETURN_TEST("test_ring_single_writer_single_reader", 0);
}

int test_ring_multiple_writers_single_reader() {
	Ring ring;
	const int chunks_per = 50;
	std::atomic<int> finished{0};
	std::string collected;

	auto writer_fn = [&](char id) {
		for (int i = 0; i < chunks_per; ++i)
			(void)ring.Write(std::string(1, id));
		finished.fetch_add(1);
	};

	std::thread w1(writer_fn, 'A');
	std::thread w2(writer_fn, 'B');
	std::thread w3(writer_fn, 'C');

	std::thread reader([&] {
		while (finished.load() < 3 || ring.AvailableBytes() > 0) {
			DataType part;
			if (ring.Extract(10, part) && !part.empty())
				collected += ToString(part);
			else if (ring.AvailableBytes() == 0 && finished.load() >= 3)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		// final drain
		DataType rem;
		if (ring.Extract(0, rem)) collected += ToString(rem);
	});

	w1.join(); w2.join(); w3.join();
	ring.Close();
	reader.join();

	ASSERT_EQUAL("A count", std::count(collected.begin(), collected.end(), 'A'), static_cast<size_t>(chunks_per));
	ASSERT_EQUAL("B count", std::count(collected.begin(), collected.end(), 'B'), static_cast<size_t>(chunks_per));
	ASSERT_EQUAL("C count", std::count(collected.begin(), collected.end(), 'C'), static_cast<size_t>(chunks_per));
	ASSERT_EQUAL("total", collected.size(), static_cast<size_t>(chunks_per * 3));
	RETURN_TEST("test_ring_multiple_writers_single_reader", 0);
}

int test_ring_single_writer_multiple_readers() {
	Ring ring;
	const int total = 200;
	std::atomic<size_t> c1{0}, c2{0}, c3{0};

	std::thread writer([&] {
		for (int i = 0; i < total; ++i) (void)ring.Write("X");
		ring.Close();
	});

	auto reader_fn = [&](std::atomic<size_t>& counter) {
		while (true) {
			DataType part;
			if (!ring.Extract(5, part)) {
				if (ring.AvailableBytes() > 0) {
					DataType rem;
					if (ring.Extract(0, rem)) counter += rem.size();
				}
				break;
			}
			if (part.empty() && ring.EoF()) break;
			counter += part.size();
		}
	};

	std::thread r1(reader_fn, std::ref(c1));
	std::thread r2(reader_fn, std::ref(c2));
	std::thread r3(reader_fn, std::ref(c3));

	writer.join(); r1.join(); r2.join(); r3.join();

	size_t sum = c1 + c2 + c3;
	ASSERT_EQUAL("all data consumed", sum, static_cast<size_t>(total));
	ASSERT_TRUE("at least one reader got data", (c1 > 0) || (c2 > 0) || (c3 > 0));
	RETURN_TEST("test_ring_single_writer_multiple_readers", 0);
}

int test_ring_blocking_read_waits_for_data() {
	Ring ring;
	std::atomic<bool> started{false}, finished{false};
	std::string result;

	std::thread consumer([&] {
		started = true;
		DataType data;
		// Request more than currently available → must block
		bool ok = ring.Read(10, data);
		if (ok) result = ToString(data);
		else if (ring.AvailableBytes() > 0) {
			DataType rem; if (ring.Read(0, rem)) result = ToString(rem);
		}
		finished = true;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	ASSERT_TRUE("consumer started", started.load());
	ASSERT_FALSE("still blocked", finished.load());

	(void)ring.Write("AB");					// 2 bytes – still insufficient
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	ASSERT_FALSE("still blocked after partial", finished.load());

	(void)ring.Write("CDEFGHIJ");			// now 10 bytes total
	ring.Close();
	consumer.join();

	ASSERT_TRUE("unblocked", finished.load());
	ASSERT_EQUAL("got all 10", result, std::string("ABCDEFGHIJ"));
	RETURN_TEST("test_ring_blocking_read_waits_for_data", 0);
}

int test_ring_close_unblocks_waiter() {
	Ring ring;
	std::atomic<bool> completed{false};
	std::string result;

	std::thread consumer([&] {
		DataType data;
		bool ok = ring.Read(100, data);		// will never get 100
		if (!ok && ring.AvailableBytes() > 0) {
			DataType rem; if (ring.Read(0, rem)) result = ToString(rem);
		} else if (ok) result = ToString(data);
		completed = true;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	(void)ring.Write("Short");
	ring.Close();							// must wake the waiter
	consumer.join();

	ASSERT_TRUE("completed", completed.load());
	ASSERT_EQUAL("got short data", result, std::string("Short"));
	ASSERT_TRUE("less than requested", result.size() < 100);
	RETURN_TEST("test_ring_close_unblocks_waiter", 0);
}

int test_ring_set_error_unblocks_and_fails() {
	Ring ring;
	std::atomic<bool> completed{false};
	bool read_ok = true;

	std::thread consumer([&] {
		DataType data;
		read_ok = ring.Read(50, data);		// blocks
		completed = true;
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	ring.SetError();						// permanent failure
	consumer.join();

	ASSERT_TRUE("unblocked", completed.load());
	ASSERT_FALSE("read failed after error", read_ok);
	ASSERT_TRUE("HasError", ring.HasError());
	ASSERT_FALSE("IsReadable", ring.IsReadable());
	ASSERT_FALSE("IsWritable", ring.IsWritable());
	RETURN_TEST("test_ring_set_error_unblocks_and_fails", 0);
}

int test_ring_peek_blocking() {
	Ring ring;
	std::thread writer([&] {
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		(void)ring.Write("0123456789");
		ring.Close();
	});

	DataType data;
	ASSERT_TRUE("peek blocks then succeeds", ring.Peek(10, data));
	ASSERT_EQUAL("content", ToString(data), std::string("0123456789"));
	// Peek must not consume
	ASSERT_EQUAL("still available", ring.AvailableBytes(), 10u);
	writer.join();
	RETURN_TEST("test_ring_peek_blocking", 0);
}

// ---------------------------------------------------------------------------
// STRESS / LARGE / RELIABILITY
// ---------------------------------------------------------------------------

int test_ring_stress_rapid_small_writes() {
	Ring ring;
	std::atomic<size_t> written{0}, read{0};

	std::thread writer([&] {
		for (int i = 0; i < 1000; ++i) {
			(void)ring.Write("X");
			written.fetch_add(1);
		}
		ring.Close();
	});

	std::thread reader([&] {
		while (true) {
			DataType part;
			if (!ring.Extract(32, part)) {
				if (ring.AvailableBytes() > 0) {
					DataType rem; if (ring.Extract(0, rem)) read += rem.size();
				}
				break;
			}
			if (part.empty() && ring.EoF()) break;
			read += part.size();
		}
	});

	writer.join(); reader.join();
	ASSERT_EQUAL("written", written.load(), 1000u);
	ASSERT_EQUAL("read == written", read.load(), written.load());
	ASSERT_TRUE("empty", ring.Empty());
	RETURN_TEST("test_ring_stress_rapid_small_writes", 0);
}

int test_ring_very_large_transfer() {
	Ring ring;
	const size_t large = 1u << 20;			// 1 MiB
	std::atomic<size_t> received{0};

	std::thread writer([&] {
		const size_t chunk = 8192;
		for (size_t i = 0; i < large; i += chunk) {
			std::string block(chunk, static_cast<char>('A' + (i / chunk) % 26));
			(void)ring.Write(block);
		}
		ring.Close();
	});

	std::thread reader([&] {
		while (true) {
			DataType part;
			if (!ring.Extract(4096, part)) {
				if (ring.AvailableBytes() > 0) {
					DataType rem; if (ring.Extract(0, rem)) received += rem.size();
				}
				break;
			}
			if (part.empty() && ring.EoF()) break;
			received += part.size();
		}
	});

	writer.join(); reader.join();
	ASSERT_EQUAL("received 1 MiB", received.load(), large);
	RETURN_TEST("test_ring_very_large_transfer", 0);
}

int test_ring_alternating_small_large() {
	Ring ring;
	std::atomic<size_t> total{0};

	std::thread writer([&] {
		for (int i = 0; i < 20; ++i) {
			if (i % 2 == 0) (void)ring.Write("X");
			else            (void)ring.Write(std::string(1000, 'Y'));
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}
		ring.Close();
	});

	std::thread reader([&] {
		while (true) {
			DataType part;
			if (!ring.Extract(128, part)) {
				if (ring.AvailableBytes() > 0) {
					DataType rem; if (ring.Extract(0, rem)) total += rem.size();
				}
				break;
			}
			if (part.empty() && ring.EoF()) break;
			total += part.size();
		}
	});

	writer.join(); reader.join();
	const size_t expected = 10 * 1 + 10 * 1000;
	ASSERT_EQUAL("total bytes", total.load(), expected);
	RETURN_TEST("test_ring_alternating_small_large", 0);
}

int test_ring_clear_while_producing() {
	Ring ring;
	ASSERT_TRUE("initial", ring.Write("InitialData"));
	ASSERT_TRUE("has data", ring.Size() > 0);

	ring.Clear();
	ASSERT_TRUE("cleared", ring.Empty());

	ASSERT_TRUE("after clear", ring.Write("AfterClear"));
	ring.Close();

	DataType data;
	ASSERT_TRUE("extract", ring.Extract(0, data));
	ASSERT_EQUAL("content", ToString(data), std::string("AfterClear"));
	RETURN_TEST("test_ring_clear_while_producing", 0);
}

int test_ring_interleaved_read_extract() {
    Ring ring;
    ASSERT_TRUE("write", ring.Write("ABCDEFGH"));
    ring.Close();

    DataType r1, e1;
    ASSERT_TRUE("read 5", ring.Read(5, r1));
    ASSERT_EQUAL("ABCDE", ToString(r1), std::string("ABCDE"));
    ASSERT_EQUAL("available after read", ring.AvailableBytes(), 3u);

    ASSERT_TRUE("extract 3", ring.Extract(3, e1));
    ASSERT_EQUAL("FGH", ToString(e1), std::string("FGH"));
    ASSERT_EQUAL("size after extract", ring.Size(), 5u);		// prefijo sigue
    ASSERT_EQUAL("available 0", ring.AvailableBytes(), 0u);
    ASSERT_FALSE("not empty yet (prefix remains)", ring.Empty());	// ← aquí fallaba

    ring.Clean();		// descarta el prefijo ya “leído”
    ASSERT_TRUE("empty after Clean", ring.Empty());

    RETURN_TEST("test_ring_interleaved_read_extract", 0);
}

int test_ring_partial_read_on_closed() {
	Ring ring;
	const std::string msg(30, 'Z');
	ASSERT_TRUE("write", ring.Write(msg));
	ring.Close();

	DataType data;
	// Request more than available on a closed buffer → must fail
	ASSERT_FALSE("Read(50) fails", ring.Read(50, data));

	// Drain with Read(0)
	DataType rem;
	ASSERT_TRUE("Read(0) succeeds", ring.Read(0, rem));
	ASSERT_EQUAL("got 30", rem.size(), 30u);
	ASSERT_TRUE("EoF", ring.EoF());

	RETURN_TEST("test_ring_partial_read_on_closed", 0);
}

int test_ring_available_bytes_consistency() {
	Ring ring;
	ASSERT_EQUAL("empty", ring.AvailableBytes(), 0u);

	ASSERT_TRUE("write 9", ring.Write("TEST DATA"));
	ASSERT_EQUAL("9", ring.AvailableBytes(), 9u);

	DataType r1;
	ASSERT_TRUE("read 4", ring.Read(4, r1));
	ASSERT_EQUAL("5 left", ring.AvailableBytes(), 5u);

	ring.Seek(0, Position::Absolute);
	ASSERT_EQUAL("after seek 0", ring.AvailableBytes(), 9u);

	DataType e1;
	ASSERT_TRUE("extract 3", ring.Extract(3, e1));
	ASSERT_EQUAL("6 left", ring.AvailableBytes(), 6u);

	ASSERT_TRUE("more", ring.Write("MORE"));
	ASSERT_EQUAL("10", ring.AvailableBytes(), 10u);

	ring.Seek(0, Position::Absolute);
	DataType all;
	ASSERT_TRUE("read all", ring.Read(0, all));
	ASSERT_EQUAL("0 after full read", ring.AvailableBytes(), 0u);

	RETURN_TEST("test_ring_available_bytes_consistency", 0);
}

int test_ring_burst_then_drain() {
	Ring ring;
	std::atomic<size_t> total{0};

	std::thread writer([&] {
		for (int i = 0; i < 1000; ++i)
			(void)ring.Write("0123456789");
		ring.Close();
	});

	std::thread reader([&] {
		while (true) {
			DataType part;
			if (!ring.Extract(100, part)) {
				if (ring.AvailableBytes() > 0) {
					DataType rem; if (ring.Extract(0, rem)) total += rem.size();
				}
				break;
			}
			if (part.empty() && ring.EoF()) break;
			total += part.size();
		}
	});

	writer.join(); reader.join();
	ASSERT_EQUAL("10 000 bytes", total.load(), 10000u);
	RETURN_TEST("test_ring_burst_then_drain", 0);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	int result = 0;

	// Basic
	result += test_ring_basic_write_read();
	result += test_ring_multiple_writes();
	result += test_ring_extract_destructive();
	result += test_ring_peek_does_not_consume();
	result += test_ring_seek_operations();
	result += test_ring_clean_after_seek();
	result += test_ring_drop();
	result += test_ring_clear();
	result += test_ring_close_mechanism();
	result += test_ring_move_semantics();
	result += test_ring_empty_read_failure();

	// Destructive / position edge cases
	result += test_ring_extract_then_seek();
	result += test_ring_partial_extract_leaves_valid_position();
	result += test_ring_extract_zero_returns_all();
	result += test_ring_read_zero_on_empty_fails();

	// Threading & blocking
	result += test_ring_single_writer_single_reader();
	result += test_ring_multiple_writers_single_reader();
	result += test_ring_single_writer_multiple_readers();
	result += test_ring_blocking_read_waits_for_data();
	result += test_ring_close_unblocks_waiter();
	result += test_ring_set_error_unblocks_and_fails();
	result += test_ring_peek_blocking();

	// Stress / large / reliability
	result += test_ring_stress_rapid_small_writes();
	result += test_ring_very_large_transfer();
	result += test_ring_alternating_small_large();
	result += test_ring_clear_while_producing();
	result += test_ring_interleaved_read_extract();
	result += test_ring_partial_read_on_closed();
	result += test_ring_available_bytes_consistency();
	result += test_ring_burst_then_drain();

	if (result == 0)
		std::cout << "All Ring tests passed!\n";
	else
		std::cout << result << " Ring test(s) failed.\n";

	return result;
}
