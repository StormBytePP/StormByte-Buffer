#include <StormByte/buffer/fifo.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>
#include <random>

using StormByte::Buffer::DataType;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

int test_fifo_write_read_vector() {
	FIFO fifo;
	std::string s = "Hello";
	(void)fifo.Write(s);
	DataType data;
	auto res = fifo.Extract(s.size(), data);
	std::string result = StormByte::String::FromByteVector(data);
	ASSERT_EQUAL("test_fifo_write_read_vector", s, result);
	ASSERT_TRUE("test_fifo_write_read_vector", fifo.Empty());
	RETURN_TEST("test_fifo_write_read_vector", 0);
}

int test_fifo_wrap_around() {
	FIFO fifo;
	(void)fifo.Write("ABCDE");
	DataType r1, all;
	auto res1 = fifo.Extract(2, r1); // now head moves, tail at end
	ASSERT_EQUAL("test_fifo_wrap_around r1", std::string("AB"), StormByte::String::FromByteVector(r1));
	(void)fifo.Write("1234"); // this will wrap
	auto res2 = fifo.Extract(7, all);
	ASSERT_EQUAL("test_fifo_wrap_around size", static_cast<std::size_t>(7), all.size());
	ASSERT_EQUAL("test_fifo_wrap_around content", std::string("CDE1234"), StormByte::String::FromByteVector(all));
	ASSERT_TRUE("test_fifo_wrap_around empty", fifo.Empty());
	RETURN_TEST("test_fifo_wrap_around", 0);
}

static std::string makePattern(std::size_t n) {
	std::string s; s.reserve(n);
	for (std::size_t i = 0; i < n; ++i) s.push_back(static_cast<char>('A' + (i % 26)));
	return s;
}

int test_fifo_buffer_stress() {
	FIFO fifo;
	std::mt19937_64 rng(12345);
	std::uniform_int_distribution<int> small(1, 256);
	std::uniform_int_distribution<int> large(512, 4096);

	std::string expected; expected.reserve(200000);

	for (int i = 0; i < 1000; ++i) {
		int len = small(rng);
		std::string chunk = makePattern(len);
		(void)fifo.Write(chunk);
		expected.append(chunk);
		if (i % 10 == 0) {
			DataType out;
			auto res = fifo.Extract(len / 2, out);
			std::string got = StormByte::String::FromByteVector(out);
			std::string exp = expected.substr(0, out.size());
			ASSERT_EQUAL("stress phase1", exp, got);
			expected.erase(0, out.size());
		}
	}

	for (int i = 0; i < 200; ++i) {
		int len = large(rng);
		std::string chunk = makePattern(len);
		(void)fifo.Write(chunk);
		expected.append(chunk);
		if (i % 5 == 0) {
			DataType out;
			auto res = fifo.Extract(len, out);
			std::string got = StormByte::String::FromByteVector(out);
			std::string exp = expected.substr(0, out.size());
			ASSERT_EQUAL("stress phase2", exp, got);
			expected.erase(0, out.size());
		}
	}

	DataType out;
	auto res = fifo.Extract(0, out);
	std::string got = StormByte::String::FromByteVector(out);
	ASSERT_EQUAL("stress final drain", expected, got);
	ASSERT_TRUE("stress empty", fifo.Empty());
	RETURN_TEST("test_fifo_buffer_stress", 0);
}

int test_fifo_default_ctor() {
	FIFO fifo;
	ASSERT_TRUE("default ctor empty", fifo.Empty());
	ASSERT_EQUAL("default ctor size", static_cast<std::size_t>(0), fifo.Size());
	RETURN_TEST("test_fifo_default_ctor", 0);
}

int test_fifo_write_basic() {
	FIFO fifo;
	(void)fifo.Write(std::string("1234"));
	ASSERT_EQUAL("write size", static_cast<std::size_t>(4), fifo.Size());
	RETURN_TEST("test_fifo_write_basic", 0);
}

int test_fifo_write_partial_count() {
	FIFO fifo;
	auto data = StormByte::String::ToByteVector("PARTIAL");
	auto res = fifo.Write(static_cast<std::size_t>(3), data);
	ASSERT_TRUE("partial write ok", res);
	ASSERT_EQUAL("partial write size", static_cast<std::size_t>(3), fifo.Size());
	DataType out;
	(void)fifo.Extract(0, out);
	ASSERT_EQUAL("partial write content", std::string("PAR"), StormByte::String::FromByteVector(out));
	RETURN_TEST("test_fifo_write_partial_count", 0);
}

int test_fifo_copy_ctor_assign() {
	FIFO a;
	(void)a.Write(std::string("AB"));
	FIFO b(a); // copy ctor
	ASSERT_EQUAL("copy ctor size", a.Size(), b.Size());
	DataType out1, out2;
	auto res1 = b.Extract(2, out1);
	ASSERT_EQUAL("copy ctor content", std::string("AB"), StormByte::String::FromByteVector(out1));
	FIFO c;
	c = a; // copy assign
	ASSERT_EQUAL("copy assign size", a.Size(), c.Size());
	auto res2 = c.Extract(2, out2);
	ASSERT_EQUAL("copy assign content", std::string("AB"), StormByte::String::FromByteVector(out2));
	RETURN_TEST("test_fifo_copy_ctor_assign", 0);
}

int test_fifo_move_ctor_assign() {
	FIFO a; (void)a.Write(std::string("XY"));
	FIFO b(std::move(a));
	ASSERT_EQUAL("move ctor size", static_cast<std::size_t>(2), b.Size());
	ASSERT_TRUE("move ctor a empty", a.Empty());
	FIFO c; c = std::move(b);
	ASSERT_EQUAL("move assign size", static_cast<std::size_t>(2), c.Size());
	ASSERT_TRUE("move assign b empty", b.Empty());
	RETURN_TEST("test_fifo_move_ctor_assign", 0);
}

int test_fifo_clear() {
	FIFO fifo;
	(void)fifo.Write(std::string(100, 'A'));
	fifo.Clear();
	ASSERT_TRUE("clear empty", fifo.Empty());
	ASSERT_EQUAL("clear size", static_cast<std::size_t>(0), fifo.Size());
	RETURN_TEST("test_fifo_clear", 0);
}

int test_fifo_write_multiple() {
	FIFO fifo;
	(void)fifo.Write(std::string(10, 'Z'));
	ASSERT_EQUAL("write size", static_cast<std::size_t>(10), fifo.Size());
	(void)fifo.Write(std::string(5, 'Y'));
	ASSERT_EQUAL("size after second write", static_cast<std::size_t>(15), fifo.Size());
	RETURN_TEST("test_fifo_write_multiple", 0);
}

int test_fifo_write_vector_and_rvalue() {
	FIFO fifo;
	std::vector<std::byte> v;
	v.resize(3);
	v[0] = std::byte{'A'}; v[1] = std::byte{'B'}; v[2] = std::byte{'C'};
	(void)fifo.Write(v);
	std::vector<std::byte> w;
	w.resize(3);
	w[0] = std::byte{'D'}; w[1] = std::byte{'E'}; w[2] = std::byte{'F'};
	(void)fifo.Write(std::move(w));
	DataType out;
	auto res = fifo.Extract(6, out);
	ASSERT_EQUAL("write vector+rvalue", std::string("ABCDEF"), StormByte::String::FromByteVector(out));
	RETURN_TEST("test_fifo_write_vector_and_rvalue", 0);
}

int test_fifo_read_default_all() {
	FIFO fifo;
	(void)fifo.Write(std::string("DATA"));
	DataType out;
	auto res = fifo.Extract(0, out); // default parameter returns all
	ASSERT_EQUAL("read default all", StormByte::String::FromByteVector(out), std::string("DATA"));
	ASSERT_TRUE("read default all empty", fifo.Empty());
	RETURN_TEST("test_fifo_read_default_all", 0);
}

int test_fifo_adopt_storage_move_write() {
	FIFO fifo;
	auto v = StormByte::String::ToByteVector("MOVE");
	(void)fifo.Write(std::move(v)); // adopt when empty
	ASSERT_EQUAL("test_fifo_adopt_storage_move_write size", fifo.Size(), static_cast<std::size_t>(4));
	DataType out;
	auto res = fifo.Extract(4, out);
	ASSERT_EQUAL("test_fifo_adopt_storage_move_write content", StormByte::String::FromByteVector(out), std::string("MOVE"));
	ASSERT_TRUE("test_fifo_adopt_storage_move_write empty", fifo.Empty());
	RETURN_TEST("test_fifo_adopt_storage_move_write", 0);
}

int test_fifo_clear_with_data() {
	FIFO fifo;
	(void)fifo.Write(StormByte::String::ToByteVector("X"));
	ASSERT_FALSE("has data before clear", fifo.Empty());
	fifo.Clear();
	ASSERT_TRUE("empty after clear", fifo.Empty());
	ASSERT_EQUAL("size is zero", fifo.Size(), static_cast<std::size_t>(0));
	RETURN_TEST("test_fifo_clear_with_data", 0);
}

int test_fifo_read_nondestructive() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABCDEF"));
	
	// First read - should get data without removing it
	DataType out1;
	(void)fifo.Read(3, out1);
	ASSERT_EQUAL("first read content", StormByte::String::FromByteVector(out1), std::string("ABC"));
	ASSERT_EQUAL("size unchanged after read", fifo.Size(), static_cast<std::size_t>(6));
	
	// Second read - should continue from where we left off
	DataType out2;
	(void)fifo.Read(3, out2);
	ASSERT_EQUAL("second read content", StormByte::String::FromByteVector(out2), std::string("DEF"));
	ASSERT_EQUAL("size still unchanged", fifo.Size(), static_cast<std::size_t>(6));
	
	// Third read - should return error (no more data)
	DataType out3;
	auto res3 = fifo.Read(0, out3);
	ASSERT_FALSE("third read error", res3);
	
	RETURN_TEST("test_fifo_read_nondestructive", 0);
}

int test_fifo_read_vs_extract() {
	FIFO fifo;
	(void)fifo.Write(std::string("123456"));
	
	// Read shouldn't remove data
	DataType r1;
	(void)fifo.Read(2, r1);
	ASSERT_EQUAL("read content", StormByte::String::FromByteVector(r1), std::string("12"));
	ASSERT_EQUAL("size after read", fifo.Size(), static_cast<std::size_t>(6));
	
	// Extract should remove data from current position (which is now at 2 after Read)
	DataType e1;
	(void)fifo.Extract(2, e1);
	ASSERT_EQUAL("extract content", StormByte::String::FromByteVector(e1), std::string("34"));
	ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(4));
	
	// Read should continue from current position (still at 2 in the new buffer)
	DataType r2;
	(void)fifo.Read(2, r2);
	ASSERT_EQUAL("read after extract", StormByte::String::FromByteVector(r2), std::string("56"));
	
	RETURN_TEST("test_fifo_read_vs_extract", 0);
}

int test_fifo_read_all_nondestructive() {
	FIFO fifo;
	(void)fifo.Write(std::string("HELLO"));
	
	// Read all with default parameter
	DataType out1;
	(void)fifo.Read(0, out1);
	ASSERT_EQUAL("read all content", StormByte::String::FromByteVector(out1), std::string("HELLO"));
	ASSERT_EQUAL("size unchanged", fifo.Size(), static_cast<std::size_t>(5));
	ASSERT_FALSE("not empty after read", fifo.Empty());
	
	// Read again should return error (no more data)
	DataType tmp;
	auto out2 = fifo.Read(0, tmp);
	ASSERT_FALSE("second read all empty", out2);
	
	RETURN_TEST("test_fifo_read_all_nondestructive", 0);
}

int test_fifo_read_with_wrap() {
	FIFO fifo;
	(void)fifo.Write("ABCDE");
	DataType temp;
	(void)fifo.Extract(2, temp); // Remove AB, head at position 2
	(void)fifo.Write("12"); // Wraps around
	
	// Read should handle wrap correctly
	DataType out;
	(void)fifo.Read(0, out); // Should read CDE12
	ASSERT_EQUAL("read wrap content", StormByte::String::FromByteVector(out), std::string("CDE12"));
	ASSERT_EQUAL("size unchanged wrap", fifo.Size(), static_cast<std::size_t>(5));
	
	RETURN_TEST("test_fifo_read_with_wrap", 0);
}

int test_fifo_extract_adjusts_read_position() {
	FIFO fifo;
	(void)fifo.Write(std::string("0123456789"));
	
	// Read first 5 bytes (position now at 5)
	DataType r1;
	(void)fifo.Read(5, r1);
	ASSERT_EQUAL("read 5", StormByte::String::FromByteVector(r1), std::string("01234"));
	
	// Extract 3 bytes from current position (5), should get "567"
	DataType e1;
	(void)fifo.Extract(3, e1);
	ASSERT_EQUAL("extract 3", StormByte::String::FromByteVector(e1), std::string("567"));
	ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(7));
	
	// Next read should continue from current position (still at 5 in the new buffer)
	DataType r2;
	(void)fifo.Read(2, r2);
	ASSERT_EQUAL("read after extract", StormByte::String::FromByteVector(r2), std::string("89"));
	
	RETURN_TEST("test_fifo_extract_adjusts_read_position", 0);
}

int test_fifo_seek_absolute() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABCDEFGHIJ"));
	
	// Seek to absolute position 3
	DataType r1;
	fifo.Seek(3, Position::Absolute);
	(void)fifo.Read(3, r1);
	ASSERT_EQUAL("seek absolute 3", StormByte::String::FromByteVector(r1), std::string("DEF"));
	
	// Seek to absolute position 0 (beginning)
	fifo.Seek(0, Position::Absolute);
	DataType r2;
	(void)fifo.Read(2, r2);
	ASSERT_EQUAL("seek absolute 0", StormByte::String::FromByteVector(r2), std::string("AB"));
	
	// Seek to absolute position 7
	fifo.Seek(7, Position::Absolute);
	DataType r3;
	(void)fifo.Read(3, r3);
	ASSERT_EQUAL("seek absolute 7", StormByte::String::FromByteVector(r3), std::string("HIJ"));
	
	// Seek beyond size (should clamp to size)
	fifo.Seek(100, Position::Absolute);
	DataType tmp;
	auto r4 = fifo.Read(0, tmp);
	ASSERT_FALSE("seek beyond size", r4);
	
	RETURN_TEST("test_fifo_seek_absolute", 0);
}

int test_fifo_seek_relative() {
	FIFO fifo;
	(void)fifo.Write(std::string("0123456789"));
	
	// Read first 2 bytes (position now at 2)
	DataType r1;
	(void)fifo.Read(2, r1);
	ASSERT_EQUAL("initial read", StormByte::String::FromByteVector(r1), std::string("01"));
	
	// Seek relative +3 (position now at 5)
	fifo.Seek(3, Position::Relative);
	DataType r2;
	(void)fifo.Read(2, r2);
	ASSERT_EQUAL("seek relative +3", StormByte::String::FromByteVector(r2), std::string("56"));
	
	// Seek relative +2 (position now at 9)
	fifo.Seek(2, Position::Relative);
	DataType r3;
	(void)fifo.Read(1, r3);
	ASSERT_EQUAL("seek relative +2", StormByte::String::FromByteVector(r3), std::string("9"));
	
	// Seek relative beyond size (should clamp)
	fifo.Seek(100, Position::Relative);
	DataType tmp;
	auto r4 = fifo.Read(0, tmp);
	ASSERT_FALSE("seek relative beyond", r4);
	
	RETURN_TEST("test_fifo_seek_relative", 0);
}

int test_fifo_seek_after_extract() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABCDEFGHIJKLMNO"));
	
	// Read first 5 bytes (position now at 5)
	DataType r1;
	(void)fifo.Read(5, r1);
	ASSERT_EQUAL("read before extract", StormByte::String::FromByteVector(r1), std::string("ABCDE"));
	
	// Extract 3 bytes from position 5 (gets "FGH")
	DataType e1;
	(void)fifo.Extract(3, e1);
	ASSERT_EQUAL("extract 3", StormByte::String::FromByteVector(e1), std::string("FGH"));
	ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(12));
	
	// Seek to absolute position 0
	fifo.Seek(0, Position::Absolute);
	DataType r2;
	(void)fifo.Read(3, r2);
	ASSERT_EQUAL("seek absolute after extract", StormByte::String::FromByteVector(r2), std::string("ABC"));
	
	// Seek to absolute position 5
	fifo.Seek(5, Position::Absolute);
	DataType r3;
	(void)fifo.Read(3, r3);
	ASSERT_EQUAL("seek to middle after extract", StormByte::String::FromByteVector(r3), std::string("IJK"));
	
	RETURN_TEST("test_fifo_seek_after_extract", 0);
}

int test_fifo_seek_with_wrap() {
	FIFO fifo;
	(void)fifo.Write("ABCDEFGHIJ");
	
	// Extract 5 bytes (removes ABCDE, head at position 5 in buffer)
	DataType e1;
	(void)fifo.Extract(5, e1);
	ASSERT_EQUAL("size after first extract", fifo.Size(), static_cast<std::size_t>(5));
	
	// Write 5 more bytes (should wrap around)
	(void)fifo.Write("12345");
	ASSERT_EQUAL("size after wrap write", fifo.Size(), static_cast<std::size_t>(10));
	
	// Seek to absolute position 0
	fifo.Seek(0, Position::Absolute);
	DataType r1;
	(void)fifo.Read(5, r1);
	ASSERT_EQUAL("seek 0 after wrap", StormByte::String::FromByteVector(r1), std::string("FGHIJ"));
	
	// Seek to absolute position 5
	fifo.Seek(5, Position::Absolute);
	DataType r2;
	(void)fifo.Read(5, r2);
	ASSERT_EQUAL("seek 5 after wrap", StormByte::String::FromByteVector(r2), std::string("12345"));
	
	RETURN_TEST("test_fifo_seek_with_wrap", 0);
}

int test_fifo_seek_relative_from_current() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABCDEFGHIJ"));

	// Read 2 bytes
	DataType r1;
	auto res1 = fifo.Read(2, r1);
	ASSERT_TRUE("initial read", res1);
	ASSERT_EQUAL("initial read", StormByte::String::FromByteVector(r1), std::string("AB"));

	// Current position is at 2, seek relative 0 (stay at current)
	fifo.Seek(0, Position::Relative);
	DataType r2;
	auto res2 = fifo.Read(2, r2);
	ASSERT_TRUE("seek relative 0", res2);
	ASSERT_EQUAL("seek relative 0", StormByte::String::FromByteVector(r2), std::string("CD"));

	// Seek backwards by going to absolute 1
	fifo.Seek(1, Position::Absolute);
	DataType r3;
	auto res3 = fifo.Read(3, r3);
	ASSERT_TRUE("seek back to 1", res3);
	ASSERT_EQUAL("seek back to 1", StormByte::String::FromByteVector(r3), std::string("BCD"));

	RETURN_TEST("test_fifo_seek_relative_from_current", 0);
}

int test_fifo_read_insufficient_data_error() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABC"));

	// Request more data than available - should return error
	DataType result;
	auto res = fifo.Read(10, result);
	ASSERT_FALSE("read insufficient returns error", res);

	// Read with count=0 should succeed and return available data
	DataType result2;
	auto res2 = fifo.Read(0, result2);
	ASSERT_TRUE("read with 0 succeeds", res2);
	ASSERT_EQUAL("read returns available", result2.size(), static_cast<std::size_t>(3));

	RETURN_TEST("test_fifo_read_insufficient_data_error", 0);
}

int test_fifo_extract_insufficient_data_error() {
	FIFO fifo;
	(void)fifo.Write(std::string("HELLO"));

	// Request more data than available - should return error
	DataType result;
	auto res = fifo.Extract(20, result);
	ASSERT_FALSE("extract insufficient returns error", res);

	// Extract with count=0 should succeed and return available data
	DataType result2;
	auto res2 = fifo.Extract(0, result2);
	ASSERT_TRUE("extract with 0 succeeds", res2);
	ASSERT_EQUAL("extract returns available", result2.size(), static_cast<std::size_t>(5));
	ASSERT_TRUE("buffer empty after extract all", fifo.Empty());

	RETURN_TEST("test_fifo_extract_insufficient_data_error", 0);
}

int test_fifo_read_after_position_beyond_size() {
	FIFO fifo;
	(void)fifo.Write(std::string("1234"));

	// Read all data
	DataType r1;
	auto res1 = fifo.Read(4, r1);
	ASSERT_TRUE("read all data", res1);
	ASSERT_EQUAL("read all data", StormByte::String::FromByteVector(r1), std::string("1234"));

	// Now read position is at end, requesting more should fail
	DataType result;
	auto res = fifo.Read(1, result);
	ASSERT_FALSE("read beyond position returns error", res);

	// Reading with count=0 should return error as well
	DataType result2;
	auto res2 = fifo.Read(0, result2);
	ASSERT_FALSE("read 0 at end returns error", res2);

	RETURN_TEST("test_fifo_read_after_position_beyond_size", 0);
}

int test_fifo_available_bytes() {
	FIFO fifo;
	
	// Empty buffer
	ASSERT_EQUAL("empty available", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	
	// Write some data
	(void)fifo.Write("ABCDEFGHIJ"); // 10 bytes
	ASSERT_EQUAL("after write available", fifo.AvailableBytes(), static_cast<std::size_t>(10));
	
	// Non-destructive read moves position
	DataType r1;
	(void)fifo.Read(3, r1);
	ASSERT_EQUAL("after read 3", fifo.AvailableBytes(), static_cast<std::size_t>(7));
	
	// Another read
	DataType r2;
	(void)fifo.Read(2, r2);
	ASSERT_EQUAL("after read 2 more", fifo.AvailableBytes(), static_cast<std::size_t>(5));
	
	// Seek back to beginning
	fifo.Seek(0, Position::Absolute);
	ASSERT_EQUAL("after seek to 0", fifo.AvailableBytes(), static_cast<std::size_t>(10));
	
	// Seek to middle
	fifo.Seek(4, Position::Absolute);
	ASSERT_EQUAL("after seek to 4", fifo.AvailableBytes(), static_cast<std::size_t>(6));
	
	// Extract removes data from current position (4)
	DataType e1;
	(void)fifo.Extract(3, e1); // Remove from position 4, gets "EFG", leaving ABCDHIJ (7 bytes)
	// Read position stays at 4
	// Available: 7 - 4 = 3
	ASSERT_EQUAL("after extract 3", fifo.AvailableBytes(), static_cast<std::size_t>(3));

	// Read all remaining from current position
	DataType r3;
	(void)fifo.Read(0, r3);
	ASSERT_EQUAL("after reading all remaining", fifo.AvailableBytes(), static_cast<std::size_t>(0));

	// Extract all
	fifo.Seek(0, Position::Absolute);
	DataType e2;
	(void)fifo.Extract(0, e2);
	ASSERT_EQUAL("after extract all", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	ASSERT_TRUE("buffer empty", fifo.Empty());
	
	RETURN_TEST("test_fifo_available_bytes", 0);
}

int test_fifo_available_bytes_after_ops() {
	FIFO fifo;
	
	(void)fifo.Write("ABCDEFGH");
	ASSERT_EQUAL("initial available", fifo.AvailableBytes(), static_cast<std::size_t>(8));
	
	// Read 3, position at 3
	DataType r1;
	(void)fifo.Read(3, r1);
	ASSERT_EQUAL("after read 3", fifo.AvailableBytes(), static_cast<std::size_t>(5));
	
	// Extract 4 from position 3, removes "DEFG", leaving ABCH (4 bytes), position stays at 3
	DataType e1;
	(void)fifo.Extract(4, e1);
	ASSERT_EQUAL("after extract 4", fifo.AvailableBytes(), static_cast<std::size_t>(1));
	
	// Write more - now buffer is ABCH1234 (8 bytes), position at 3
	(void)fifo.Write("1234");
	ASSERT_EQUAL("after wrap write", fifo.AvailableBytes(), static_cast<std::size_t>(5));
	
	// Read 5 from position 3, advances position to 8
	DataType r2;
	(void)fifo.Read(5, r2);
	ASSERT_EQUAL("after read 5", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	
	RETURN_TEST("test_fifo_available_bytes_with_wrap", 0);
}

int test_fifo_equality() {
	FIFO a;
	FIFO b;
	(void)a.Write("ABC");
	(void)b.Write("ABC");

	// Same content and position -> equal
	ASSERT_TRUE("fifo equal same content", a == b);
	ASSERT_FALSE("fifo unequal same content", a != b);

	// Advance read position on one -> positions differ, should be unequal
	DataType tmp;
	(void)a.Read(1, tmp);
	ASSERT_FALSE("fifo unequal after read changes position", a == b);

	// Make positions equal again -> equal
	(void)b.Read(1, tmp);
	ASSERT_TRUE("fifo equal after syncing read position", a == b);

	// Change stored data -> not equal
	(void)b.Write("D");
	ASSERT_FALSE("fifo not equal after different content", a == b);

	RETURN_TEST("test_fifo_equality", 0);
}

int test_fifo_write_remaining_fifo() {
	FIFO src;
	(void)src.Write(std::string("HELLO"));
	// Advance read position to ensure const-write appends full buffer (including read bytes)
	DataType r;
	(void)src.Read(2, r);

	FIFO dst;
	(void)dst.Write(std::string("START"));
	const std::size_t before = dst.Size();
	const std::size_t src_remaining = src.AvailableBytes();

	// Append remaining unread portion of source FIFO (const reference)
	auto ok = dst.Write(src);
	ASSERT_TRUE("fifo write whole returned", ok);
	ASSERT_EQUAL("fifo write remaining size", dst.Size(), before + src_remaining);

	// Extract full destination content and validate
	DataType all;
	(void)dst.Extract(0, all);
	std::string content = StormByte::String::FromByteVector(all);
	ASSERT_EQUAL("fifo write remaining content", content, std::string("STARTLLO"));

	// rvalue write: move-append into dst and ensure source becomes empty
	FIFO src2;
	(void)src2.Write(std::string("WORLD"));
	auto ok2 = dst.Write(std::move(src2));
	ASSERT_TRUE("fifo write whole rvalue returned", ok2);
	// After move-append, dst should end with WORLD
	DataType tail;
	(void)dst.Extract(0, tail);
	std::string tail_s = StormByte::String::FromByteVector(tail);
	ASSERT_EQUAL("fifo write whole rvalue content", tail_s, std::string("WORLD"));

	RETURN_TEST("test_fifo_write_remaining_fifo", 0);
}

int test_fifo_move_steal_preserves_read_position() {
	FIFO src;
	(void)src.Write(std::string("ABCDE"));
	// Non-destructive read advances read position to 2 (reads "AB")
	DataType r;
	auto res = src.Read(2, r);
	ASSERT_TRUE("advance read returned", res);

	// Destination empty: move-append should steal internal storage and preserve read position
	FIFO dst;
	auto ok = dst.Write(std::move(src));
	ASSERT_TRUE("move write returned true", ok);

	// Now reading all available from destination should return "CDE"
	DataType out;
	auto res2 = dst.Read(0, out);
	ASSERT_TRUE("dst read returned", res2);
	ASSERT_EQUAL("dst remaining after move preserves position", StormByte::String::FromByteVector(out), std::string("CDE"));

	RETURN_TEST("test_fifo_move_steal_preserves_read_position", 0);
}

int test_fifo_hexdump() {
	FIFO fifo;
	// 40 bytes total -> with 8 columns this produces 5 lines
	std::string s = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcd";
	(void)fifo.Write(s);

	// Request a hexdump starting at read position 0 with 8 columns
	std::string dump = fifo.HexDump(8, 0);

	// Exact expected output constructed to match FIFO::HexDump formatting
	std::string expected;
	expected += "Size: 40 bytes\n";
	expected += "Read Position: 0\n";
	expected += "\n";
	expected += "00000000: 30 31 32 33 34 35 36 37   01234567\n";
	expected += "00000008: 38 39 41 42 43 44 45 46   89ABCDEF\n";
	expected += "00000010: 47 48 49 4A 4B 4C 4D 4E   GHIJKLMN\n";
	expected += "00000018: 4F 50 51 52 53 54 55 56   OPQRSTUV\n";
	expected += "00000020: 57 58 59 5A 61 62 63 64   WXYZabcd";

	ASSERT_EQUAL("test_fifo_hexdump exact match", expected, dump);
	RETURN_TEST("test_fifo_hexdump", 0);
}

int test_fifo_hexdump_offset() {
	FIFO fifo;
	std::string s = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcd";
	(void)fifo.Write(s);

	// Move read position to 5 and produce a hexdump (8 columns)
	fifo.Seek(5, Position::Absolute);
	std::string dump = fifo.HexDump(8, 0);

	std::string expected;
	expected += "Size: 40 bytes\n";
	expected += "Read Position: 5\n";
	expected += "\n";
	expected += "00000005: 35 36 37 38 39 41 42 43   56789ABC\n";
	expected += "0000000D: 44 45 46 47 48 49 4A 4B   DEFGHIJK\n";
	expected += "00000015: 4C 4D 4E 4F 50 51 52 53   LMNOPQRS\n";
	expected += "0000001D: 54 55 56 57 58 59 5A 61   TUVWXYZa\n";
	expected += "00000025: 62 63 64                  bcd";

	ASSERT_EQUAL("test_fifo_hexdump_offset exact match", expected, dump);
	RETURN_TEST("test_fifo_hexdump_offset", 0);
}

int test_fifo_hexdump_mixed() {
	FIFO fifo;
	// Mix printable and non-printable bytes
	std::vector<std::byte> v;
	v.push_back(std::byte{0x41}); // 'A'
	v.push_back(std::byte{0x00}); // NUL
	v.push_back(std::byte{0x1F}); // unit separator (non-print)
	v.push_back(std::byte{0x20}); // space
	v.push_back(std::byte{0x41}); // 'A'
	v.push_back(std::byte{0x7E}); // '~'
	v.push_back(std::byte{0x7F}); // DEL (non-print)
	v.push_back(std::byte{0x80}); // non-print
	v.push_back(std::byte{0xFF}); // non-print
	v.push_back(std::byte{0x30}); // '0'
	(void)fifo.Write(std::move(v));

	std::string dump = fifo.HexDump(8, 0);

	std::string expected;
	expected += "Size: 10 bytes\n";
	expected += "Read Position: 0\n";
	expected += "\n";
	expected += "00000000: 41 00 1F 20 41 7E 7F 80   A.. A~..\n";
	// Second line: two bytes (FF 30) then padding (6 missing cols -> 18 spaces) and two separation spaces before ASCII
	expected += std::string("00000008: FF 30") + std::string(21, ' ') + ".0";

	ASSERT_EQUAL("test_fifo_hexdump_mixed exact match", expected, dump);
	RETURN_TEST("test_fifo_hexdump_mixed", 0);
}

int test_fifo_skip_basic() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABCDEFG"));
	(void)fifo.Drop(3);
	ASSERT_EQUAL("skip basic size", fifo.Size(), static_cast<std::size_t>(4));
	DataType out;
	(void)fifo.Extract(0, out);
	ASSERT_EQUAL("extract after skip content", StormByte::String::FromByteVector(out), std::string("DEFG"));
	RETURN_TEST("test_fifo_skip_basic", 0);
}

int test_fifo_skip_with_readpos() {
	FIFO fifo;
	(void)fifo.Write(std::string("0123456789"));

	// Move read position forward
	DataType r;
	auto res = fifo.Read(3, r);
	ASSERT_TRUE("read before skip", res);

	// Skip 4 bytes from head
	(void)fifo.Drop(4);

	// After skipping, remaining should be from original index 7..9 => "789"
	ASSERT_EQUAL("size after skip with readpos", fifo.Size(), static_cast<std::size_t>(3));
	DataType out;
	(void)fifo.Extract(0, out);
	ASSERT_EQUAL("content after skip with readpos", StormByte::String::FromByteVector(out), std::string("789"));
	RETURN_TEST("test_fifo_skip_with_readpos", 0);
}

int test_fifo_peek_basic() {
	FIFO fifo;
	(void)fifo.Write(std::string("HELLO"));
	
	// Peek 3 bytes - should not advance read position
	DataType peek1;
	auto res1 = fifo.Peek(3, peek1);
	ASSERT_TRUE("peek returned", res1);
	ASSERT_EQUAL("peek content", StormByte::String::FromByteVector(peek1), std::string("HEL"));
	
	// Peek again - should return same data
	DataType peek2;
	auto res2 = fifo.Peek(3, peek2);
	ASSERT_TRUE("peek2 returned", res2);
	ASSERT_EQUAL("peek2 content", StormByte::String::FromByteVector(peek2), std::string("HEL"));
	
	// Now read - should return same data as peek
	DataType read1;
	auto res3 = fifo.Read(3, read1);
	ASSERT_TRUE("read returned", res3);
	ASSERT_EQUAL("read content matches peek", StormByte::String::FromByteVector(read1), std::string("HEL"));
	
	// Peek remaining
	DataType peek3;
	auto res4 = fifo.Peek(2, peek3);
	ASSERT_TRUE("peek3 returned", res4);
	ASSERT_EQUAL("peek3 content", StormByte::String::FromByteVector(peek3), std::string("LO"));
	
	RETURN_TEST("test_fifo_peek_basic", 0);
}

int test_fifo_peek_all_available() {
	FIFO fifo;
	(void)fifo.Write(std::string("WORLD"));
	
	// Peek all available (count = 0)
	DataType peek_all;
	auto res1 = fifo.Peek(0, peek_all);
	ASSERT_TRUE("peek all returned", res1);
	ASSERT_EQUAL("peek all content", StormByte::String::FromByteVector(peek_all), std::string("WORLD"));
	
	// Read 2 bytes
	DataType read1;
	auto res2 = fifo.Read(2, read1);
	ASSERT_TRUE("read returned", res2);
	
	// Peek all remaining
	DataType peek_remaining;
	auto res3 = fifo.Peek(0, peek_remaining);
	ASSERT_TRUE("peek remaining returned", res3);
	ASSERT_EQUAL("peek remaining content", StormByte::String::FromByteVector(peek_remaining), std::string("RLD"));
	
	RETURN_TEST("test_fifo_peek_all_available", 0);
}

int test_fifo_peek_insufficient_data() {
	FIFO fifo;
	(void)fifo.Write(std::string("ABC"));
	
	// Try to peek more than available
	DataType peek;
	auto res = fifo.Peek(10, peek);
	ASSERT_FALSE("peek insufficient returned error", res);
	
	RETURN_TEST("test_fifo_peek_insufficient_data", 0);
}

int test_fifo_peek_after_seek() {
	FIFO fifo;
	(void)fifo.Write(std::string("0123456789"));
	
	// Seek to position 5
	fifo.Seek(5, Position::Absolute);
	
	// Peek from new position
	DataType peek;
	auto res1 = fifo.Peek(3, peek);
	ASSERT_TRUE("peek after seek returned", res1);
	ASSERT_EQUAL("peek after seek content", StormByte::String::FromByteVector(peek), std::string("567"));
	
	// Verify position didn't change
	DataType read;
	auto res2 = fifo.Read(3, read);
	ASSERT_TRUE("read after peek returned", res2);
	ASSERT_EQUAL("read after peek content", StormByte::String::FromByteVector(read), std::string("567"));
	
	RETURN_TEST("test_fifo_peek_after_seek", 0);
}

int test_fifo_read_span_basic() {
	FIFO fifo;
	(void)fifo.Write("ABCDEF");
	
	DataType out;
	auto res = fifo.Read(3, out);
	ASSERT_TRUE("read_span returned", res);
	ASSERT_EQUAL("read_span size", out.size(), static_cast<std::size_t>(3));
	ASSERT_EQUAL("read_span first byte", static_cast<char>(out[0]), 'A');
	ASSERT_EQUAL("read_span second byte", static_cast<char>(out[1]), 'B');
	ASSERT_EQUAL("read_span third byte", static_cast<char>(out[2]), 'C');
	
	// Verify position advanced
	DataType read;
	auto res2 = fifo.Read(3, read);
	ASSERT_TRUE("read after span returned", res2);
	ASSERT_EQUAL("read after span content", StormByte::String::FromByteVector(read), std::string("DEF"));
	
	RETURN_TEST("test_fifo_read_span_basic", 0);
}

int test_fifo_read_span_all_available() {
	FIFO fifo;
	(void)fifo.Write("HelloWorld");
	
	// Read with count=0 should read all available
	DataType out;
	auto res = fifo.Read(0, out);
	ASSERT_TRUE("read_span_all returned", res);
	ASSERT_EQUAL("read_span_all size", out.size(), static_cast<std::size_t>(10));
	ASSERT_EQUAL("read_span_all content", StormByte::String::FromByteVector(out), std::string("HelloWorld"));
	
	// No more data available after reading all
	ASSERT_EQUAL("read_span_all no available", fifo.AvailableBytes(), static_cast<std::size_t>(0));
	
	RETURN_TEST("test_fifo_read_span_all_available", 0);
}

int test_fifo_read_span_insufficient_data() {
	FIFO fifo;
	(void)fifo.Write("ABC");
	
	// Try to read more than available
	DataType span;
	auto res = fifo.Read(10, span);
	ASSERT_FALSE("read_span_insufficient error", res);
	
	// Verify position didn't advance
	DataType read;
	auto res2 = fifo.Read(3, read);
	ASSERT_TRUE("read after failed span returned", res2);
	ASSERT_EQUAL("read after failed span", StormByte::String::FromByteVector(read), std::string("ABC"));
	
	RETURN_TEST("test_fifo_read_span_insufficient_data", 0);
}

int test_fifo_read_span_vs_read() {
	// Verify sequential reads produce same data across buffers
	FIFO fifo1, fifo2;
	const std::string data = "ComparisonTest";
	(void)fifo1.Write(data);
	(void)fifo2.Write(data);
	
	// Read first 4 bytes from both
	DataType r1;
	DataType r2;
	auto res1 = fifo1.Read(4, r1);
	auto res2 = fifo2.Read(4, r2);
	ASSERT_TRUE("span_vs_read first read ok", res1);
	ASSERT_TRUE("span_vs_read second read ok", res2);
	std::string s1 = StormByte::String::FromByteVector(r1);
	std::string s2 = StormByte::String::FromByteVector(r2);
	ASSERT_EQUAL("span_vs_read content", s1, s2);
	ASSERT_EQUAL("span_vs_read expected", s1, std::string("Comp"));
	
	// Both should have same remaining data
	ASSERT_EQUAL("span_vs_read fifo1 available", fifo1.AvailableBytes(), fifo2.AvailableBytes());
	
	RETURN_TEST("test_fifo_read_span_vs_read", 0);
}

int test_fifo_write_full_telling_zero() {
	FIFO fifo;
	DataType data(10, std::byte{0xFF}); // 10 bytes of data
	// Write zero bytes
	auto res = fifo.Write(0, data);
	ASSERT_TRUE("write zero bytes returned", res);
	ASSERT_EQUAL("size after write zero", fifo.Size(), static_cast<std::size_t>(10));
	RETURN_TEST("test_fifo_write_full_telling_zero", 0);
}

int main() {
	int result = 0;
	result += test_fifo_write_read_vector();
	result += test_fifo_wrap_around();
	result += test_fifo_adopt_storage_move_write();
	result += test_fifo_clear_with_data();
	result += test_fifo_default_ctor();
	result += test_fifo_write_basic();
	result += test_fifo_write_partial_count();
	result += test_fifo_copy_ctor_assign();
	result += test_fifo_move_ctor_assign();
	result += test_fifo_clear();
	result += test_fifo_write_multiple();
	result += test_fifo_write_vector_and_rvalue();
	result += test_fifo_read_default_all();
	result += test_fifo_buffer_stress();
	result += test_fifo_read_nondestructive();
	result += test_fifo_read_vs_extract();
	result += test_fifo_read_all_nondestructive();
	result += test_fifo_read_with_wrap();
	result += test_fifo_extract_adjusts_read_position();
	result += test_fifo_seek_absolute();
	result += test_fifo_seek_relative();
	result += test_fifo_seek_after_extract();
	result += test_fifo_seek_with_wrap();
	result += test_fifo_seek_relative_from_current();
	result += test_fifo_read_insufficient_data_error();
	result += test_fifo_extract_insufficient_data_error();
	result += test_fifo_read_after_position_beyond_size();
	result += test_fifo_available_bytes();
	result += test_fifo_available_bytes_after_ops();
	result += test_fifo_equality();
	result += test_fifo_write_remaining_fifo();
	result += test_fifo_move_steal_preserves_read_position();
	result += test_fifo_hexdump();
	result += test_fifo_hexdump_offset();
	result += test_fifo_hexdump_mixed();
	result += test_fifo_skip_basic();
	result += test_fifo_skip_with_readpos();
	result += test_fifo_peek_basic();
	result += test_fifo_peek_all_available();
	result += test_fifo_peek_insufficient_data();
	result += test_fifo_peek_after_seek();
	result += test_fifo_read_span_basic();
	result += test_fifo_read_span_all_available();
	result += test_fifo_read_span_insufficient_data();
	result += test_fifo_read_span_vs_read();
	result += test_fifo_write_full_telling_zero();

	if (result == 0) {
		std::cout << "FIFO tests passed!" << std::endl;
	} else {
		std::cout << result << " FIFO tests failed." << std::endl;
	}
	return result;
}
