#include <StormByte/memory/buffer/fifo.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>
#include <random>

using namespace StormByte;
using StormByte::Memory::Buffer::FIFO;

int test_fifo_write_read_vector() {
    FIFO fifo(2);
    std::string s = "Hello";
    fifo.Write(s);
    auto out = fifo.Read(s.size());
    std::string result(reinterpret_cast<const char*>(out.data()), out.size());
    ASSERT_EQUAL("test_fifo_write_read_vector", result, s);
    ASSERT_TRUE("test_fifo_write_read_vector", fifo.Empty());
    RETURN_TEST("test_fifo_write_read_vector", 0);
}

int test_fifo_wrap_around() {
    FIFO fifo(5);
    fifo.Write("ABCDE");
    auto r1 = fifo.Read(2); // now head moves, tail at end
    ASSERT_EQUAL("test_fifo_wrap_around r1", std::string(reinterpret_cast<const char*>(r1.data()), r1.size()), std::string("AB"));
    fifo.Write("1234"); // this will wrap
    auto all = fifo.Read(7);
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

int test_fifo_memory_stress() {
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
            auto out = fifo.Read(len / 2);
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
            auto out = fifo.Read(len);
            std::string got(reinterpret_cast<const char*>(out.data()), out.size());
            std::string exp = expected.substr(0, out.size());
            ASSERT_EQUAL("stress phase2", got, exp);
            expected.erase(0, out.size());
        }
    }

    auto out = fifo.Read();
    std::string got(reinterpret_cast<const char*>(out.data()), out.size());
    ASSERT_EQUAL("stress final drain", got, expected);
    ASSERT_TRUE("stress empty", fifo.Empty());
    RETURN_TEST("test_fifo_memory_stress", 0);
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
    auto out = b.Read(2);
    ASSERT_EQUAL("copy ctor content", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("AB"));
    FIFO c(1);
    c = a; // copy assign
    ASSERT_EQUAL("copy assign size", c.Size(), a.Size());
    auto out2 = c.Read(2);
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
    auto out = fifo.Read(6);
    ASSERT_EQUAL("write vector+rvalue", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("ABCDEF"));
    RETURN_TEST("test_fifo_write_vector_and_rvalue", 0);
}

int test_fifo_read_default_all() {
    FIFO fifo;
    fifo.Write(std::string("DATA"));
    auto out = fifo.Read(); // default parameter returns all
    ASSERT_EQUAL("read default all", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("DATA"));
    ASSERT_TRUE("read default all empty", fifo.Empty());
    RETURN_TEST("test_fifo_read_default_all", 0);
}

int test_fifo_full_flag_transitions() {
    FIFO fifo(2);
    fifo.Write(std::string("AB"));
    ASSERT_TRUE("full true", fifo.Full());
    auto out = fifo.Read(1);
    ASSERT_TRUE("full false after read", !fifo.Full());
    fifo.Write(std::string("C"));
    ASSERT_TRUE("full true again", fifo.Full());
    RETURN_TEST("test_fifo_full_flag_transitions", 0);
}

int test_fifo_adopt_storage_move_write() {
    FIFO fifo;
    auto v = bytesFromString("MOVE");
    fifo.Write(std::move(v)); // adopt when empty
    ASSERT_EQUAL("test_fifo_adopt_storage_move_write size", fifo.Size(), static_cast<std::size_t>(4));
    auto out = fifo.Read(4);
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

int test_fifo_full_flag() {
    FIFO fifo(3);
    fifo.Write(bytesFromString("ABC"));
    ASSERT_TRUE("test_fifo_full_flag full", fifo.Full());
    auto out = fifo.Read(1);
    ASSERT_TRUE("test_fifo_full_flag not full", !fifo.Full());
    ASSERT_EQUAL("test_fifo_full_flag content", std::string(reinterpret_cast<const char*>(out.data()), out.size()), std::string("A"));
    RETURN_TEST("test_fifo_full_flag", 0);
}

int main() {
    int result = 0;
    result += test_fifo_write_read_vector();
    result += test_fifo_wrap_around();
    result += test_fifo_adopt_storage_move_write();
    result += test_fifo_reserve_and_clear();
    result += test_fifo_full_flag();

    // API coverage tests based on fifo.hxx
    result += test_fifo_default_ctor();
    result += test_fifo_capacity_ctor();
    result += test_fifo_copy_ctor_assign();
    result += test_fifo_move_ctor_assign();
    result += test_fifo_clear_restore_capacity();
    result += test_fifo_reserve();
    result += test_fifo_write_vector_and_rvalue();
    result += test_fifo_read_default_all();
    result += test_fifo_full_flag_transitions();

    // Memory stress test
    result += test_fifo_memory_stress();

    if (result == 0) {
        std::cout << "FIFO tests passed!" << std::endl;
    } else {
        std::cout << result << " FIFO tests failed." << std::endl;
    }
    return result;
}
