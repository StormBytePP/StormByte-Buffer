#include <StormByte/buffer/fifo.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>
#include <random>

using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

int test_fifo_write_read_vector() {
    FIFO fifo;
    std::string s = "Hello";
    fifo.Write(s);
    auto out = fifo.Extract(s.size());
    std::string result = StormByte::String::FromByteVector(*out);
    ASSERT_EQUAL("test_fifo_write_read_vector", result, s);
    ASSERT_TRUE("test_fifo_write_read_vector", fifo.Empty());
    RETURN_TEST("test_fifo_write_read_vector", 0);
}

int test_fifo_wrap_around() {
    FIFO fifo;
    fifo.Write("ABCDE");
    auto r1 = fifo.Extract(2); // now head moves, tail at end
    ASSERT_EQUAL("test_fifo_wrap_around r1", StormByte::String::FromByteVector(*r1), std::string("AB"));
    fifo.Write("1234"); // this will wrap
    auto all = fifo.Extract(7);
    ASSERT_EQUAL("test_fifo_wrap_around size", all->size(), static_cast<std::size_t>(7));
    ASSERT_EQUAL("test_fifo_wrap_around content", StormByte::String::FromByteVector(*all), std::string("CDE1234"));
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
        fifo.Write(chunk);
        expected.append(chunk);
        if (i % 10 == 0) {
            auto out = fifo.Extract(len / 2);
            std::string got = StormByte::String::FromByteVector(*out);
            std::string exp = expected.substr(0, out->size());
            ASSERT_EQUAL("stress phase1", got, exp);
            expected.erase(0, out->size());
        }
    }

    for (int i = 0; i < 200; ++i) {
        int len = large(rng);
        std::string chunk = makePattern(len);
        fifo.Write(chunk);
        expected.append(chunk);
        if (i % 5 == 0) {
            auto out = fifo.Extract(len);
            std::string got = StormByte::String::FromByteVector(*out);
            std::string exp = expected.substr(0, out->size());
            ASSERT_EQUAL("stress phase2", got, exp);
            expected.erase(0, out->size());
        }
    }

    auto out = fifo.Extract();
    std::string got = StormByte::String::FromByteVector(*out);
    ASSERT_EQUAL("stress final drain", got, expected);
    ASSERT_TRUE("stress empty", fifo.Empty());
    RETURN_TEST("test_fifo_buffer_stress", 0);
}

int test_fifo_default_ctor() {
    FIFO fifo;
    ASSERT_TRUE("default ctor empty", fifo.Empty());
    ASSERT_EQUAL("default ctor size", fifo.Size(), static_cast<std::size_t>(0));
    RETURN_TEST("test_fifo_default_ctor", 0);
}

int test_fifo_write_basic() {
    FIFO fifo;
    fifo.Write(std::string("1234"));
    ASSERT_EQUAL("write size", fifo.Size(), static_cast<std::size_t>(4));
    RETURN_TEST("test_fifo_write_basic", 0);
}

int test_fifo_copy_ctor_assign() {
    FIFO a;
    a.Write(std::string("AB"));
    FIFO b(a); // copy ctor
    ASSERT_EQUAL("copy ctor size", b.Size(), a.Size());
    auto out = b.Extract(2);
    ASSERT_EQUAL("copy ctor content", StormByte::String::FromByteVector(*out), std::string("AB"));
    FIFO c;
    c = a; // copy assign
    ASSERT_EQUAL("copy assign size", c.Size(), a.Size());
    auto out2 = c.Extract(2);
    ASSERT_EQUAL("copy assign content", StormByte::String::FromByteVector(*out2), std::string("AB"));
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

int test_fifo_clear() {
    FIFO fifo;
    fifo.Write(std::string(100, 'A'));
    fifo.Clear();
    ASSERT_TRUE("clear empty", fifo.Empty());
    ASSERT_EQUAL("clear size", fifo.Size(), static_cast<std::size_t>(0));
    RETURN_TEST("test_fifo_clear", 0);
}

int test_fifo_write_multiple() {
    FIFO fifo;
    fifo.Write(std::string(10, 'Z'));
    ASSERT_EQUAL("write size", fifo.Size(), static_cast<std::size_t>(10));
    fifo.Write(std::string(5, 'Y'));
    ASSERT_EQUAL("size after second write", fifo.Size(), static_cast<std::size_t>(15));
    RETURN_TEST("test_fifo_write_multiple", 0);
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
    ASSERT_EQUAL("write vector+rvalue", StormByte::String::FromByteVector(*out), std::string("ABCDEF"));
    RETURN_TEST("test_fifo_write_vector_and_rvalue", 0);
}

int test_fifo_read_default_all() {
    FIFO fifo;
    fifo.Write(std::string("DATA"));
    auto out = fifo.Extract(); // default parameter returns all
    ASSERT_EQUAL("read default all", StormByte::String::FromByteVector(*out), std::string("DATA"));
    ASSERT_TRUE("read default all empty", fifo.Empty());
    RETURN_TEST("test_fifo_read_default_all", 0);
}

int test_fifo_adopt_storage_move_write() {
    FIFO fifo;
    auto v = StormByte::String::ToByteVector("MOVE");
    fifo.Write(std::move(v)); // adopt when empty
    ASSERT_EQUAL("test_fifo_adopt_storage_move_write size", fifo.Size(), static_cast<std::size_t>(4));
    auto out = fifo.Extract(4);
    ASSERT_EQUAL("test_fifo_adopt_storage_move_write content", StormByte::String::FromByteVector(*out), std::string("MOVE"));
    ASSERT_TRUE("test_fifo_adopt_storage_move_write empty", fifo.Empty());
    RETURN_TEST("test_fifo_adopt_storage_move_write", 0);
}

int test_fifo_clear_with_data() {
    FIFO fifo;
    fifo.Write(StormByte::String::ToByteVector("X"));
    ASSERT_FALSE("has data before clear", fifo.Empty());
    fifo.Clear();
    ASSERT_TRUE("empty after clear", fifo.Empty());
    ASSERT_EQUAL("size is zero", fifo.Size(), static_cast<std::size_t>(0));
    RETURN_TEST("test_fifo_clear_with_data", 0);
}

int test_fifo_read_nondestructive() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEF"));
    
    // First read - should get data without removing it
    auto out1 = fifo.Read(3);
    ASSERT_EQUAL("first read content", StormByte::String::FromByteVector(*out1), std::string("ABC"));
    ASSERT_EQUAL("size unchanged after read", fifo.Size(), static_cast<std::size_t>(6));
    
    // Second read - should continue from where we left off
    auto out2 = fifo.Read(3);
    ASSERT_EQUAL("second read content", StormByte::String::FromByteVector(*out2), std::string("DEF"));
    ASSERT_EQUAL("size still unchanged", fifo.Size(), static_cast<std::size_t>(6));
    
    // Third read - should return error (no more data)
    auto out3 = fifo.Read(0);
    ASSERT_FALSE("third read error", out3.has_value());
    
    RETURN_TEST("test_fifo_read_nondestructive", 0);
}

int test_fifo_read_vs_extract() {
    FIFO fifo;
    fifo.Write(std::string("123456"));
    
    // Read shouldn't remove data
    auto r1 = fifo.Read(2);
    ASSERT_EQUAL("read content", StormByte::String::FromByteVector(*r1), std::string("12"));
    ASSERT_EQUAL("size after read", fifo.Size(), static_cast<std::size_t>(6));
    
    // Extract should remove data
    auto e1 = fifo.Extract(2);
    ASSERT_EQUAL("extract content", StormByte::String::FromByteVector(*e1), std::string("12"));
    ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(4));
    
    // Read should continue from adjusted position
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("read after extract", StormByte::String::FromByteVector(*r2), std::string("34"));
    
    RETURN_TEST("test_fifo_read_vs_extract", 0);
}

int test_fifo_read_all_nondestructive() {
    FIFO fifo;
    fifo.Write(std::string("HELLO"));
    
    // Read all with default parameter
    auto out1 = fifo.Read();
    ASSERT_EQUAL("read all content", StormByte::String::FromByteVector(*out1), std::string("HELLO"));
    ASSERT_EQUAL("size unchanged", fifo.Size(), static_cast<std::size_t>(5));
    ASSERT_FALSE("not empty after read", fifo.Empty());
    
    // Read again should return error (no more data)
    auto out2 = fifo.Read(0);
    ASSERT_FALSE("second read all empty", out2.has_value());
    
    RETURN_TEST("test_fifo_read_all_nondestructive", 0);
}

int test_fifo_read_with_wrap() {
    FIFO fifo;
    fifo.Write("ABCDE");
    fifo.Extract(2); // Remove AB, head at position 2
    fifo.Write("12"); // Wraps around
    
    // Read should handle wrap correctly
    auto out = fifo.Read(); // Should read CDE12
    ASSERT_EQUAL("read wrap content", StormByte::String::FromByteVector(*out), std::string("CDE12"));
    ASSERT_EQUAL("size unchanged wrap", fifo.Size(), static_cast<std::size_t>(5));
    
    RETURN_TEST("test_fifo_read_with_wrap", 0);
}

int test_fifo_extract_adjusts_read_position() {
    FIFO fifo;
    fifo.Write(std::string("0123456789"));
    
    // Read first 5 bytes
    auto r1 = fifo.Read(5);
    ASSERT_EQUAL("read 5", StormByte::String::FromByteVector(*r1), std::string("01234"));
    
    // Extract first 3 bytes (should adjust read position)
    auto e1 = fifo.Extract(3);
    ASSERT_EQUAL("extract 3", StormByte::String::FromByteVector(*e1), std::string("012"));
    ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(7));
    
    // Next read should continue from adjusted position (was at 5, extract removed 3, now at 2 relative to new head)
    // New head is at '3', read position is 2, so we read from '5' onwards
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("read after extract", StormByte::String::FromByteVector(*r2), std::string("56"));
    
    RETURN_TEST("test_fifo_extract_adjusts_read_position", 0);
}

int test_fifo_seek_absolute() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEFGHIJ"));
    
    // Seek to absolute position 3
    fifo.Seek(3, Position::Absolute);
    auto r1 = fifo.Read(3);
    ASSERT_EQUAL("seek absolute 3", StormByte::String::FromByteVector(*r1), std::string("DEF"));
    
    // Seek to absolute position 0 (beginning)
    fifo.Seek(0, Position::Absolute);
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("seek absolute 0", StormByte::String::FromByteVector(*r2), std::string("AB"));
    
    // Seek to absolute position 7
    fifo.Seek(7, Position::Absolute);
    auto r3 = fifo.Read(3);
    ASSERT_EQUAL("seek absolute 7", StormByte::String::FromByteVector(*r3), std::string("HIJ"));
    
    // Seek beyond size (should clamp to size)
    fifo.Seek(100, Position::Absolute);
    auto r4 = fifo.Read(0);
    ASSERT_FALSE("seek beyond size", r4.has_value());
    
    RETURN_TEST("test_fifo_seek_absolute", 0);
}

int test_fifo_seek_relative() {
    FIFO fifo;
    fifo.Write(std::string("0123456789"));
    
    // Read first 2 bytes (position now at 2)
    auto r1 = fifo.Read(2);
    ASSERT_EQUAL("initial read", StormByte::String::FromByteVector(*r1), std::string("01"));
    
    // Seek relative +3 (position now at 5)
    fifo.Seek(3, Position::Relative);
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("seek relative +3", StormByte::String::FromByteVector(*r2), std::string("56"));
    
    // Seek relative +2 (position now at 9)
    fifo.Seek(2, Position::Relative);
    auto r3 = fifo.Read(1);
    ASSERT_EQUAL("seek relative +2", StormByte::String::FromByteVector(*r3), std::string("9"));
    
    // Seek relative beyond size (should clamp)
    fifo.Seek(100, Position::Relative);
    auto r4 = fifo.Read(0);
    ASSERT_FALSE("seek relative beyond", r4.has_value());
    
    RETURN_TEST("test_fifo_seek_relative", 0);
}

int test_fifo_seek_after_extract() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEFGHIJKLMNO"));
    
    // Read first 5 bytes
    auto r1 = fifo.Read(5);
    ASSERT_EQUAL("read before extract", StormByte::String::FromByteVector(*r1), std::string("ABCDE"));
    
    // Extract first 3 bytes (removes ABC, head now at D)
    auto e1 = fifo.Extract(3);
    ASSERT_EQUAL("extract 3", StormByte::String::FromByteVector(*e1), std::string("ABC"));
    ASSERT_EQUAL("size after extract", fifo.Size(), static_cast<std::size_t>(12));
    
    // Seek to absolute position 0 (should start from new head at D)
    fifo.Seek(0, Position::Absolute);
    auto r2 = fifo.Read(3);
    ASSERT_EQUAL("seek absolute after extract", StormByte::String::FromByteVector(*r2), std::string("DEF"));
    
    // Seek to absolute position 5
    fifo.Seek(5, Position::Absolute);
    auto r3 = fifo.Read(3);
    ASSERT_EQUAL("seek to middle after extract", StormByte::String::FromByteVector(*r3), std::string("IJK"));
    
    RETURN_TEST("test_fifo_seek_after_extract", 0);
}

int test_fifo_seek_with_wrap() {
    FIFO fifo;
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
    ASSERT_EQUAL("seek 0 after wrap", StormByte::String::FromByteVector(*r1), std::string("FGHIJ"));
    
    // Seek to absolute position 5
    fifo.Seek(5, Position::Absolute);
    auto r2 = fifo.Read(5);
    ASSERT_EQUAL("seek 5 after wrap", StormByte::String::FromByteVector(*r2), std::string("12345"));
    
    RETURN_TEST("test_fifo_seek_with_wrap", 0);
}

int test_fifo_seek_relative_from_current() {
    FIFO fifo;
    fifo.Write(std::string("ABCDEFGHIJ"));
    
    // Read 2 bytes
    auto r1 = fifo.Read(2);
    ASSERT_EQUAL("initial read", StormByte::String::FromByteVector(*r1), std::string("AB"));
    
    // Current position is at 2, seek relative 0 (stay at current)
    fifo.Seek(0, Position::Relative);
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("seek relative 0", StormByte::String::FromByteVector(*r2), std::string("CD"));
    
    // Seek backwards by going to absolute 1
    fifo.Seek(1, Position::Absolute);
    auto r3 = fifo.Read(3);
    ASSERT_EQUAL("seek back to 1", StormByte::String::FromByteVector(*r3), std::string("BCD"));
    
    RETURN_TEST("test_fifo_seek_relative_from_current", 0);
}

int test_fifo_read_insufficient_data_error() {
    FIFO fifo;
    fifo.Write(std::string("ABC"));
    
    // Request more data than available - should return error
    auto result = fifo.Read(10);
    ASSERT_FALSE("read insufficient returns error", result.has_value());
    
    // Read with count=0 should succeed and return available data
    auto result2 = fifo.Read(0);
    ASSERT_TRUE("read with 0 succeeds", result2.has_value());
    ASSERT_EQUAL("read returns available", result2->size(), static_cast<std::size_t>(3));
    
    RETURN_TEST("test_fifo_read_insufficient_data_error", 0);
}

int test_fifo_extract_insufficient_data_error() {
    FIFO fifo;
    fifo.Write(std::string("HELLO"));
    
    // Request more data than available - should return error
    auto result = fifo.Extract(20);
    ASSERT_FALSE("extract insufficient returns error", result.has_value());
    
    // Extract with count=0 should succeed and return available data
    auto result2 = fifo.Extract(0);
    ASSERT_TRUE("extract with 0 succeeds", result2.has_value());
    ASSERT_EQUAL("extract returns available", result2->size(), static_cast<std::size_t>(5));
    ASSERT_TRUE("buffer empty after extract all", fifo.Empty());
    
    RETURN_TEST("test_fifo_extract_insufficient_data_error", 0);
}

int test_fifo_read_after_position_beyond_size() {
    FIFO fifo;
    fifo.Write(std::string("1234"));
    
    // Read all data
    auto r1 = fifo.Read(4);
    ASSERT_EQUAL("read all data", StormByte::String::FromByteVector(*r1), std::string("1234"));
    
    // Now read position is at end, requesting more should fail
    auto result = fifo.Read(1);
    ASSERT_FALSE("read beyond position returns error", result.has_value());
    
    // Reading with count=0 should return error as well
    auto result2 = fifo.Read(0);
    ASSERT_FALSE("read 0 at end returns error", result2.has_value());
    
    RETURN_TEST("test_fifo_read_after_position_beyond_size", 0);
}

int test_fifo_available_bytes() {
    FIFO fifo;
    
    // Empty buffer
    ASSERT_EQUAL("empty available", fifo.AvailableBytes(), static_cast<std::size_t>(0));
    
    // Write some data
    fifo.Write("ABCDEFGHIJ"); // 10 bytes
    ASSERT_EQUAL("after write available", fifo.AvailableBytes(), static_cast<std::size_t>(10));
    
    // Non-destructive read moves position
    auto r1 = fifo.Read(3);
    ASSERT_EQUAL("after read 3", fifo.AvailableBytes(), static_cast<std::size_t>(7));
    
    // Another read
    auto r2 = fifo.Read(2);
    ASSERT_EQUAL("after read 2 more", fifo.AvailableBytes(), static_cast<std::size_t>(5));
    
    // Seek back to beginning
    fifo.Seek(0, Position::Absolute);
    ASSERT_EQUAL("after seek to 0", fifo.AvailableBytes(), static_cast<std::size_t>(10));
    
    // Seek to middle
    fifo.Seek(4, Position::Absolute);
    ASSERT_EQUAL("after seek to 4", fifo.AvailableBytes(), static_cast<std::size_t>(6));
    
    // Extract removes data from head
    auto e1 = fifo.Extract(3); // Remove ABC, leaving DEFGHIJ (7 bytes)
    // Read position was at 4, now adjusted to 1 (4-3)
    // Available: 7 - 1 = 6
    ASSERT_EQUAL("after extract 3", fifo.AvailableBytes(), static_cast<std::size_t>(6));
    
    // Read all remaining from current position
    auto r3 = fifo.Read(0);
    ASSERT_EQUAL("after reading all remaining", fifo.AvailableBytes(), static_cast<std::size_t>(0));
    
    // Extract all
    fifo.Seek(0, Position::Absolute);
    auto e2 = fifo.Extract(0);
    ASSERT_EQUAL("after extract all", fifo.AvailableBytes(), static_cast<std::size_t>(0));
    ASSERT_TRUE("buffer empty", fifo.Empty());
    
    RETURN_TEST("test_fifo_available_bytes", 0);
}

int test_fifo_available_bytes_after_ops() {
    FIFO fifo;
    
    fifo.Write("ABCDEFGH");
    ASSERT_EQUAL("initial available", fifo.AvailableBytes(), static_cast<std::size_t>(8));
    
    // Read 3, position at 3
    [[maybe_unused]] auto r1 = fifo.Read(3);
    ASSERT_EQUAL("after read 3", fifo.AvailableBytes(), static_cast<std::size_t>(5));
    
    // Extract 4, removes ABCD, head now at E, read position adjusted to 0
    [[maybe_unused]] auto e1 = fifo.Extract(4);
    ASSERT_EQUAL("after extract 4", fifo.AvailableBytes(), static_cast<std::size_t>(4));
    
    // Write more causing wrap
    fifo.Write("1234");
    ASSERT_EQUAL("after wrap write", fifo.AvailableBytes(), static_cast<std::size_t>(8));
    
    // Read some
    [[maybe_unused]] auto r2 = fifo.Read(5);
    ASSERT_EQUAL("after read 5", fifo.AvailableBytes(), static_cast<std::size_t>(3));
    
    RETURN_TEST("test_fifo_available_bytes_with_wrap", 0);
}

int test_fifo_equality() {
    FIFO a;
    FIFO b;
    a.Write("ABC");
    b.Write("ABC");

    // Same content -> equal (equality is content-only)
    ASSERT_TRUE("fifo equal same content", a == b);
	ASSERT_FALSE("fifo unequal same content", a != b);

    // Advance read position on one -> still equal (positions/status ignored)
    [[maybe_unused]] auto r = a.Read(1);
    ASSERT_TRUE("fifo equal after read changes position", a == b);

    // Make positions equal again -> still equal
    [[maybe_unused]] auto r2 = b.Read(1);
    ASSERT_TRUE("fifo equal after syncing read position", a == b);

    // Change stored data -> not equal
    b.Write("D");
    ASSERT_FALSE("fifo not equal after different content", a == b);

    RETURN_TEST("test_fifo_equality", 0);
}

int test_fifo_write_whole_fifo() {
    FIFO src;
    src.Write(std::string("HELLO"));
    // Advance read position to ensure const-write appends full buffer (including read bytes)
    [[maybe_unused]] auto r = src.Read(2);

    FIFO dst;
    dst.Write(std::string("START"));
    const std::size_t before = dst.Size();

    // Append entire source FIFO (const reference)
    dst.Write(src);

    ASSERT_EQUAL("fifo write whole size", dst.Size(), before + src.Size());

    // Extract full destination content and validate
    auto all = dst.Extract();
    ASSERT_TRUE("extract all returned", all.has_value());
    std::string content = StormByte::String::FromByteVector(*all);
    ASSERT_EQUAL("fifo write whole content", content, std::string("STARTHELLO"));

    // rvalue write: move-append into dst and ensure source becomes empty
    FIFO src2;
    src2.Write(std::string("WORLD"));
    dst.Write(std::move(src2));
    // After move-append, dst should end with WORLD
    auto tail = dst.Extract();
    ASSERT_TRUE("extract tail returned", tail.has_value());
    std::string tail_s = StormByte::String::FromByteVector(*tail);
    ASSERT_EQUAL("fifo write whole rvalue content", tail_s, std::string("WORLD"));

    RETURN_TEST("test_fifo_write_whole_fifo", 0);
}

int test_fifo_move_steal_preserves_read_position() {
    FIFO src;
    src.Write(std::string("ABCDE"));
    // Non-destructive read advances read position to 2 (reads "AB")
    auto r = src.Read(2);
    ASSERT_TRUE("advance read returned", r.has_value());

    // Destination empty: move-append should steal internal storage and preserve read position
    FIFO dst;
    bool ok = dst.Write(std::move(src));
    ASSERT_TRUE("move write returned true", ok);

    // Now reading all available from destination should return "CDE"
    auto out = dst.Read(0);
    ASSERT_TRUE("dst read returned", out.has_value());
    ASSERT_EQUAL("dst remaining after move preserves position", StormByte::String::FromByteVector(*out), std::string("CDE"));

    RETURN_TEST("test_fifo_move_steal_preserves_read_position", 0);
}

int main() {
    int result = 0;
    result += test_fifo_write_read_vector();
    result += test_fifo_wrap_around();
    result += test_fifo_adopt_storage_move_write();
    result += test_fifo_clear_with_data();
    result += test_fifo_default_ctor();
    result += test_fifo_write_basic();
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
    result += test_fifo_write_whole_fifo();
    result += test_fifo_move_steal_preserves_read_position();

    if (result == 0) {
        std::cout << "FIFO tests passed!" << std::endl;
    } else {
        std::cout << result << " FIFO tests failed." << std::endl;
    }
    return result;
}
