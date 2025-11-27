#include <StormByte/buffer/fifo.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>
#include <random>

using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

int test_fifo_write_read_vector() {
    FIFO fifo(2);
    std::string s = "Hello";
    fifo.Write(s);
    auto out = fifo.Extract(s.size());
    std::string result(reinterpret_cast<const char*>(out.data()), out.size());
    ASSERT_EQUAL("test_fifo_write_read_vector", result, s);
    ASSERT_TRUE("test_fifo_write_read_vector", fifo.Empty());
    RETURN_TEST("test_fifo_write_read_vector", 0);
}

int test_fifo_wrap_around() {
    FIFO fifo(5);
    fifo.Write("ABCDE");
    auto r1 = fifo.Extract(2); // now head moves, tail at end
    ASSERT_EQUAL("test_fifo_wrap_around r1", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("AB"));
    fifo.Write("1234"); // this will wrap
    auto all = fifo.Extract(7);
    ASSERT_EQUAL("test_fifo_wrap_around size", all.size(), static_cast<std::size_t>(7));
    ASSERT_EQUAL("test_fifo_wrap_around content", std::string(reinterpret_cast<const char*>(all.data()), all.size()), std::string("CDE1234"));
    ASSERT_TRUE("test_fifo_wrap_around empty", fifo.Empty());
    RETURN_TEST("test_fifo_wrap_around", 0);
}

std::vector<std::byte> bytesFromString(const std::string& str) {
	std::vector<std::byte> vec;
	vec.resize(str.size());
	std::copy_n(reinterpret_cast<const std::byte*>(str.data()), str.size(), vec.begin());
	return vec;
}

static std::string makePattern(std::size_t n) {
    std::string s; s.reserve(n);
    for (std::size_t i = 0; i < n; ++i) s.push_back(static_cast<char>('A' + (i % 26)));
    return s;
}

int test_fifo_buffer_stress() {
    FIFO fifo(64);
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int> small(1, 256);
    std::uniform_int_distribution<int> large(512, 4096);

    std::string expected; expected.reserve(200000);

    for (int i = 0; i < 1000; ++i) {
        int len = small(rng);
        std::string chunk = makePattern(len);
        fifo.Write(chunk);
        expected.append(chunk);
        if (i % 10 == 0) {
            auto out = fifo.Extract(len / 2);
            std::string got(reinterpret_cast<const char*>(out.data()), out.size());
            std::string exp = expected.substr(0, out.size());
            ASSERT_EQUAL("stress phase1", got, exp);
            expected.erase(0, out.size());
        }
    }

    for (int i = 0; i < 200; ++i) {
        int len = large(rng);
        std::string chunk = makePattern(len);
        fifo.Write(chunk);
        expected.append(chunk);
        if (i % 5 == 0) {
            auto out = fifo.Extract(len);
            std::string got(reinterpret_cast<const char*>(out.data()), out.size());
            std::string exp = expected.substr(0, out.size());
            ASSERT_EQUAL("stress phase2", got, exp);
            expected.erase(0, out.size());
        }
    }

    auto out = fifo.Extract();
    std::string got(reinterpret_cast<const char*>(out.data()), out.size());
    ASSERT_EQUAL("stress final drain", got, expected);
    ASSERT_TRUE("stress empty", fifo.Empty());
    RETURN_TEST("test_fifo_buffer_stress", 0);
}

int test_fifo_default_ctor() {
    FIFO fifo; // default capacity = 0
    ASSERT_TRUE("default ctor empty", fifo.Empty());
    ASSERT_EQUAL("default ctor size", fifo.Size(), static_cast<std::size_t>(0));
    ASSERT_EQUAL("default ctor capacity", fifo.Capacity(), static_cast<std::size_t>(0));
    RETURN_TEST("test_fifo_default_ctor", 0);
}

int test_fifo_capacity_ctor() {
    FIFO fifo(8);
    ASSERT_EQUAL("capacity ctor capacity", fifo.Capacity(), static_cast<std::size_t>(8));
    fifo.Write(std::string("1234"));
    ASSERT_EQUAL("capacity ctor size", fifo.Size(), static_cast<std::size_t>(4));
    RETURN_TEST("test_fifo_capacity_ctor", 0);
}

int test_fifo_copy_ctor_assign() {
    FIFO a(4);
    a.Write(std::string("AB"));
    FIFO b(a); // copy ctor
    ASSERT_EQUAL("copy ctor size", b.Size(), a.Size());
    auto out = b.Extract(2);
    ASSERT_EQUAL("copy ctor content", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("AB"));
    FIFO c(1);
    c = a; // copy assign
    ASSERT_EQUAL("copy assign size", c.Size(), a.Size());
    auto out2 = c.Extract(2);
    ASSERT_EQUAL("copy assign content", std::string(reinterpret_cast<const char*>(out2.data()), out2.size()), std::string("AB"));
    RETURN_TEST("test_fifo_copy_ctor_assign", 0);
}

int test_fifo_move_ctor_assign() {
    FIFO a; a.Write(std::string("XY"));
    FIFO b(std::move(a));
    ASSERT_EQUAL("move ctor size", b.Size(), static_cast<std::size_t>(2));
    ASSERT_TRUE("move ctor a empty", a.Empty());
    FIFO c; c = std::move(b);
    ASSERT_EQUAL("move assign size", c.Size(), static_cast<std::size_t>(2));
    ASSERT_TRUE("move assign b empty", b.Empty());
    RETURN_TEST("test_fifo_move_ctor_assign", 0);
}

int test_fifo_clear_restore_capacity() {
    FIFO fifo(16);
    fifo.Write(std::string(100, 'A'));
    fifo.Clear();
    ASSERT_TRUE("clear empty", fifo.Empty());
    ASSERT_EQUAL("clear capacity restored", fifo.Capacity(), static_cast<std::size_t>(16));
    RETURN_TEST("test_fifo_clear_restore_capacity", 0);
}

int test_fifo_reserve() {
    FIFO fifo(2);
    fifo.Reserve(32);
    ASSERT_EQUAL("reserve capacity", fifo.Capacity(), static_cast<std::size_t>(32));
    fifo.Write(std::string(10, 'Z'));
    ASSERT_EQUAL("reserve size", fifo.Size(), static_cast<std::size_t>(10));
    RETURN_TEST("test_fifo_reserve", 0);
}

int test_fifo_write_vector_and_rvalue() {
    FIFO fifo;
    std::vector<std::byte> v;
    v.resize(3);
    v[0] = std::byte{'A'}; v[1] = std::byte{'B'}; v[2] = std::byte{'C'};
    fifo.Write(v);
    std::vector<std::byte> w;
    w.resize(3);
    w[0] = std::byte{'D'}; w[1] = std::byte{'E'}; w[2] = std::byte{'F'};
    fifo.Write(std::move(w));
    auto out = fifo.Extract(6);
    ASSERT_EQUAL("write vector+rvalue", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("ABCDEF"));
    RETURN_TEST("test_fifo_write_vector_and_rvalue", 0);
}

int test_fifo_read_default_all() {
    FIFO fifo;
    fifo.Write(std::string("DATA"));
    auto out = fifo.Extract(); // default parameter returns all
    ASSERT_EQUAL("read default all", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("DATA"));
    ASSERT_TRUE("read default all empty", fifo.Empty());
    RETURN_TEST("test_fifo_read_default_all", 0);
}

int test_fifo_adopt_storage_move_write() {
    FIFO fifo;
    auto v = bytesFromString("MOVE");
    fifo.Write(std::move(v)); // adopt when empty
    ASSERT_EQUAL("test_fifo_adopt_storage_move_write size", fifo.Size(), static_cast<std::size_t>(4));
    auto out = fifo.Extract(4);
    ASSERT_EQUAL("test_fifo_adopt_storage_move_write content", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("MOVE"));
    ASSERT_TRUE("test_fifo_adopt_storage_move_write empty", fifo.Empty());
    RETURN_TEST("test_fifo_adopt_storage_move_write", 0);
}

int test_fifo_reserve_and_clear() {
    FIFO fifo(1);
    fifo.Reserve(16);
    ASSERT_EQUAL("test_fifo_reserve_and_clear capacity", fifo.Capacity(), static_cast<std::size_t>(16));
    fifo.Write(bytesFromString("X"));
    fifo.Clear();
    ASSERT_TRUE("test_fifo_reserve_and_clear empty", fifo.Empty());
    ASSERT_EQUAL("test_fifo_reserve_and_clear size", fifo.Size(), static_cast<std::size_t>(0));
    RETURN_TEST("test_fifo_reserve_and_clear", 0);
}

int test_fifo_closed_noop_on_empty() {
    FIFO fifo(8);
    fifo.Close();
    ASSERT_TRUE("closed flag set", fifo.IsClosed());
    fifo.Write(std::string("DATA"));
    ASSERT_TRUE("no write after close (empty)", fifo.Empty());
    ASSERT_EQUAL("size remains zero", fifo.Size(), static_cast<std::size_t>(0));
    RETURN_TEST("test_fifo_closed_noop_on_empty", 0);
}

int test_fifo_closed_noop_on_nonempty() {
    FIFO fifo(8);
    fifo.Write(std::string("ABC"));
    ASSERT_EQUAL("pre-close size", fifo.Size(), static_cast<std::size_t>(3));
    fifo.Close();
    ASSERT_TRUE("closed flag set", fifo.IsClosed());
    fifo.Write(std::string("DEF"));
    ASSERT_EQUAL("size unchanged after close", fifo.Size(), static_cast<std::size_t>(3));
    auto out = fifo.Extract();
    ASSERT_EQUAL("content unchanged after close write", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("ABC"));
    RETURN_TEST("test_fifo_closed_noop_on_nonempty", 0);
}

int test_fifo_read_nondestructive() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEF"));
    
    // First read - should get data without removing it
    auto out1 = fifo.Read(3);
    ASSERT_EQUAL("first read content", std::string(reinterpret_cast<const char*>(out1.data()), out1.size()), std::string("ABC"));
    ASSERT_EQUAL("size unchanged after read", fifo.Size(), static_cast<std::size_t>(6));
    
    // Second read - should continue from where we left off
    auto out2 = fifo.Read(3);
    ASSERT_EQUAL("second read content", std::string(reinterpret_cast<const char*>(out2.data()), out2.size()), std::string("DEF"));
    ASSERT_EQUAL("size still unchanged", fifo.Size(), static_cast<std::size_t>(6));
    
    // Third read - should return empty (read position beyond available data)
    auto out3 = fifo.Read(1);
    ASSERT_EQUAL("third read empty", out3.size(), static_cast<std::size_t>(0));
    
    RETURN_TEST("test_fifo_read_nondestructive", 0);
}

int test_fifo_read_vs_extract() {
    FIFO fifo;
    fifo.Write(std::string("123456"));
    
    // Read shouldn't remove data
    auto r1 = fifo.Read(2);
    ASSERT_EQUAL("read content", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("12"));
    ASSERT_EQUAL("size after read", fifo.Size(), static_cast<std::size_t>(6));
    
    // Extract should remove data
    auto e1 = fifo.Extract(2);
    ASSERT_EQUAL("extract content", std::string(reinterpret_cast<const char*>(e1.data()), e1.size()), std::string("12"));
    ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(4));
    
    // Read should continue from adjusted position
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("read after extract", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("34"));
    
    RETURN_TEST("test_fifo_read_vs_extract", 0);
}

int test_fifo_read_all_nondestructive() {
    FIFO fifo;
    fifo.Write(std::string("HELLO"));
    
    // Read all with default parameter
    auto out1 = fifo.Read();
    ASSERT_EQUAL("read all content", std::string(reinterpret_cast<const char*>(out1.data()), out1.size()), std::string("HELLO"));
    ASSERT_EQUAL("size unchanged", fifo.Size(), static_cast<std::size_t>(5));
    ASSERT_FALSE("not empty after read", fifo.Empty());
    
    // Read again should return empty (position at end)
    auto out2 = fifo.Read();
    ASSERT_EQUAL("second read all empty", out2.size(), static_cast<std::size_t>(0));
    
    RETURN_TEST("test_fifo_read_all_nondestructive", 0);
}

int test_fifo_read_with_wrap() {
    FIFO fifo(5);
    fifo.Write("ABCDE");
    fifo.Extract(2); // Remove AB, head at position 2
    fifo.Write("12"); // Wraps around
    
    // Read should handle wrap correctly
    auto out = fifo.Read(); // Should read CDE12
    ASSERT_EQUAL("read wrap content", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("CDE12"));
    ASSERT_EQUAL("size unchanged wrap", fifo.Size(), static_cast<std::size_t>(5));
    
    RETURN_TEST("test_fifo_read_with_wrap", 0);
}

int test_fifo_extract_adjusts_read_position() {
    FIFO fifo;
    fifo.Write(std::string("0123456789"));
    
    // Read first 5 bytes
    auto r1 = fifo.Read(5);
    ASSERT_EQUAL("read 5", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("01234"));
    
    // Extract first 3 bytes (should adjust read position)
    auto e1 = fifo.Extract(3);
    ASSERT_EQUAL("extract 3", std::string(reinterpret_cast<const char*>(e1.data()), e1.size()), std::string("012"));
    ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(7));
    
    // Next read should continue from adjusted position (was at 5, extract removed 3, now at 2 relative to new head)
    // New head is at '3', read position is 2, so we read from '5' onwards
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("read after extract", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("56"));
    
    RETURN_TEST("test_fifo_extract_adjusts_read_position", 0);
}

int test_fifo_seek_absolute() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEFGHIJ"));
    
    // Seek to absolute position 3
    fifo.Seek(3, Position::Absolute);
    auto r1 = fifo.Read(3);
    ASSERT_EQUAL("seek absolute 3", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("DEF"));
    
    // Seek to absolute position 0 (beginning)
    fifo.Seek(0, Position::Absolute);
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("seek absolute 0", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("AB"));
    
    // Seek to absolute position 7
    fifo.Seek(7, Position::Absolute);
    auto r3 = fifo.Read(3);
    ASSERT_EQUAL("seek absolute 7", std::string(reinterpret_cast<const char*>(r3.data()), r3.size()), std::string("HIJ"));
    
    // Seek beyond size (should clamp to size)
    fifo.Seek(100, Position::Absolute);
    auto r4 = fifo.Read(5);
    ASSERT_EQUAL("seek beyond size", r4.size(), static_cast<std::size_t>(0));
    
    RETURN_TEST("test_fifo_seek_absolute", 0);
}

int test_fifo_seek_relative() {
    FIFO fifo;
    fifo.Write(std::string("0123456789"));
    
    // Read first 2 bytes (position now at 2)
    auto r1 = fifo.Read(2);
    ASSERT_EQUAL("initial read", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("01"));
    
    // Seek relative +3 (position now at 5)
    fifo.Seek(3, Position::Relative);
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("seek relative +3", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("56"));
    
    // Seek relative +2 (position now at 9)
    fifo.Seek(2, Position::Relative);
    auto r3 = fifo.Read(1);
    ASSERT_EQUAL("seek relative +2", std::string(reinterpret_cast<const char*>(r3.data()), r3.size()), std::string("9"));
    
    // Seek relative beyond size (should clamp)
    fifo.Seek(100, Position::Relative);
    auto r4 = fifo.Read(1);
    ASSERT_EQUAL("seek relative beyond", r4.size(), static_cast<std::size_t>(0));
    
    RETURN_TEST("test_fifo_seek_relative", 0);
}

int test_fifo_seek_after_extract() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEFGHIJKLMNO"));
    
    // Read first 5 bytes
    auto r1 = fifo.Read(5);
    ASSERT_EQUAL("read before extract", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("ABCDE"));
    
    // Extract first 3 bytes (removes ABC, head now at D)
    auto e1 = fifo.Extract(3);
    ASSERT_EQUAL("extract 3", std::string(reinterpret_cast<const char*>(e1.data()), e1.size()), std::string("ABC"));
    ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(12));
    
    // Seek to absolute position 0 (should start from new head at D)
    fifo.Seek(0, Position::Absolute);
    auto r2 = fifo.Read(3);
    ASSERT_EQUAL("seek absolute after extract", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("DEF"));
    
    // Seek to absolute position 5
    fifo.Seek(5, Position::Absolute);
    auto r3 = fifo.Read(3);
    ASSERT_EQUAL("seek to middle after extract", std::string(reinterpret_cast<const char*>(r3.data()), r3.size()), std::string("IJK"));
    
    RETURN_TEST("test_fifo_seek_after_extract", 0);
}

int test_fifo_seek_with_wrap() {
    FIFO fifo(10);
    fifo.Write("ABCDEFGHIJ");
    
    // Extract 5 bytes (removes ABCDE, head at position 5 in buffer)
    fifo.Extract(5);
    ASSERT_EQUAL("size after first extract", fifo.Size(), static_cast<std::size_t>(5));
    
    // Write 5 more bytes (should wrap around)
    fifo.Write("12345");
    ASSERT_EQUAL("size after wrap write", fifo.Size(), static_cast<std::size_t>(10));
    
    // Seek to absolute position 0
    fifo.Seek(0, Position::Absolute);
    auto r1 = fifo.Read(5);
    ASSERT_EQUAL("seek 0 after wrap", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("FGHIJ"));
    
    // Seek to absolute position 5
    fifo.Seek(5, Position::Absolute);
    auto r2 = fifo.Read(5);
    ASSERT_EQUAL("seek 5 after wrap", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("12345"));
    
    RETURN_TEST("test_fifo_seek_with_wrap", 0);
}

int test_fifo_seek_relative_from_current() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEFGHIJ"));
    
    // Read 2 bytes
    auto r1 = fifo.Read(2);
    ASSERT_EQUAL("initial read", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("AB"));
    
    // Current position is at 2, seek relative 0 (stay at current)
    fifo.Seek(0, Position::Relative);
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("seek relative 0", std::string(reinterpret_cast<const char*>(r2.data()), r2.size()), std::string("CD"));
    
    // Seek backwards by going to absolute 1
    fifo.Seek(1, Position::Absolute);
    auto r3 = fifo.Read(3);
    ASSERT_EQUAL("seek back to 1", std::string(reinterpret_cast<const char*>(r3.data()), r3.size()), std::string("BCD"));
    
    RETURN_TEST("test_fifo_seek_relative_from_current", 0);
}

int main() {
    int result = 0;
    result += test_fifo_write_read_vector();
    result += test_fifo_wrap_around();
    result += test_fifo_adopt_storage_move_write();
    result += test_fifo_reserve_and_clear();
    result += test_fifo_default_ctor();
    result += test_fifo_capacity_ctor();
    result += test_fifo_copy_ctor_assign();
    result += test_fifo_move_ctor_assign();
    result += test_fifo_clear_restore_capacity();
    result += test_fifo_reserve();
    result += test_fifo_write_vector_and_rvalue();
    result += test_fifo_read_default_all();
    result += test_fifo_closed_noop_on_empty();
    result += test_fifo_closed_noop_on_nonempty();
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

    if (result == 0) {
        std::cout << "FIFO tests passed!" << std::endl;
    } else {
        std::cout << result << " FIFO tests failed." << std::endl;
    }
    return result;
}
