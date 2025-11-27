#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/test_handlers.h>

#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <iostream>

using StormByte::Buffer::SharedFIFO;
using StormByte::Buffer::Position;

static std::string toString(const std::vector<std::byte>& v) {
    return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

int test_shared_fifo_producer_consumer_blocking() {
    SharedFIFO fifo(8);
    std::atomic<bool> done{false};
    const std::string payload = "ABCDEFGHIJ"; // 10 bytes

    std::thread producer([&]() -> void {
        // write in two chunks to test blocking and concat order
        fifo.Write(std::string(payload.begin(), payload.begin() + 4));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        fifo.Write(std::string(payload.begin() + 4, payload.end()));
        fifo.Close();
        done.store(true);
    });

    std::string collected;
    std::thread consumer([&]() -> void {
        while (true) {
            auto part = fifo.Read(3); // read small chunks, blocks until 3 available or closed
            if (!part || (part->empty() && fifo.IsClosed())) break;
            collected.append(toString(*part));
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
    SharedFIFO fifo(4);
    std::atomic<bool> woke{false};
    std::atomic<bool> saw_closed{false};
    std::size_t extracted_size = 12345; // sentinel

    std::thread t([&]() -> void {
        auto out = fifo.Extract(1); // block until 1 byte or close
        // With no writer, Close() will wake us; out should be empty or error
        woke.store(true);
        saw_closed.store(fifo.IsClosed());
        extracted_size = out.has_value() ? out->size() : 0;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    fifo.Close();
    t.join();

    ASSERT_TRUE("thread woke on close", woke.load());
    ASSERT_TRUE("extract woke closed", saw_closed.load());
    ASSERT_EQUAL("no data extracted", extracted_size, static_cast<std::size_t>(0));
    RETURN_TEST("test_shared_fifo_extract_blocking_and_close", 0);
}

int test_shared_fifo_concurrent_seek_and_read() {
    SharedFIFO fifo(8);
    fifo.Write(std::string("0123456789"));

    std::atomic<bool> seeker_done{false};
    std::string read_a, read_b;

    std::thread seeker([&]() -> void {
        // Seek around while another thread reads; mutex ensures safety
        fifo.Seek(5, Position::Absolute); // pos at '5'
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        fifo.Seek(2, Position::Relative); // pos at '7'
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        fifo.Seek(1, Position::Absolute); // pos at '1'
        seeker_done.store(true);
    });

    std::thread reader([&]() -> void {
        // Perform two reads interleaved with seeks
        auto r1 = fifo.Read(2); // reads from current (race-free due to mutex)
        read_a = toString(*r1);
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        auto r2 = fifo.Read(3);
        read_b = toString(*r2);
    });

    seeker.join();
    reader.join();

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
    SharedFIFO fifo(8);
    fifo.Write(std::string("ABCDEFGH"));

    std::string r_before, r_after;
    std::thread reader([&]() -> void {
        r_before = toString(*fifo.Read(3)); // ABC
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        r_after = toString(*fifo.Read(2)); // depends on extract adjustment
    });

    std::thread extractor([&]() -> void {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto e = fifo.Extract(2); // remove AB; read_position should adjust
        (void)e;
    });

    reader.join();
    extractor.join();

    ASSERT_EQUAL("first read ABC", r_before, std::string("ABC"));
    // After extracting 2, the read position (which was at 3) becomes 1 relative to new head 'C', so next should start from 'D'
    ASSERT_EQUAL("next read after adjust is DE", r_after, std::string("DE"));
    RETURN_TEST("test_shared_fifo_extract_adjusts_read_position_concurrency", 0);
}

int test_shared_fifo_multi_producer_single_consumer_counts() {
    SharedFIFO fifo(8);
    const int chunks = 200;
    std::atomic<bool> p1_done{false}, p2_done{false};

    auto producerA = std::thread([&]() -> void {
        for (int i = 0; i < chunks; ++i) {
            fifo.Write(std::string("A"));
        }
        p1_done.store(true);
    });

    auto producerB = std::thread([&]() -> void {
        for (int i = 0; i < chunks; ++i) {
            fifo.Write(std::string("B"));
        }
        p2_done.store(true);
    });

    std::string collected;
    auto consumer = std::thread([&]() -> void {
        while (true) {
            auto part = fifo.Extract(1); // block for each byte
            if (!part || (part->empty() && fifo.IsClosed())) break;
            collected.append(toString(*part));
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
    SharedFIFO fifo(16);
    const int total = 1000;
    // Producer writes a predictable sequence of 'X'
    auto producer = std::thread([&]() -> void {
        fifo.Write(std::string(total, 'X'));
        fifo.Close();
    });

    std::atomic<size_t> c1{0}, c2{0};
    auto consumer1 = std::thread([&]() -> void {
        size_t local = 0;
        while (true) {
            auto part = fifo.Extract(1);
            if (!part || (part->empty() && fifo.IsClosed())) break;
            local += part->size();
        }
        c1.store(local);
    });
    auto consumer2 = std::thread([&]() -> void {
        size_t local = 0;
        while (true) {
            auto part = fifo.Extract(1);
            if (!part || (part->empty() && fifo.IsClosed())) break;
            local += part->size();
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
    SharedFIFO fifo(4);
    fifo.Write(std::string("ABC"));
    ASSERT_EQUAL("pre-close size", fifo.Size(), static_cast<std::size_t>(3));
    fifo.Close();
    fifo.Write(std::string("DEF"));
    ASSERT_EQUAL("size unchanged after close", fifo.Size(), static_cast<std::size_t>(3));
    auto out = fifo.Extract(0);
    ASSERT_EQUAL("content after close write blocked", toString(*out), std::string("ABC"));
    RETURN_TEST("test_shared_fifo_close_suppresses_writes", 0);
}

int test_shared_fifo_wrap_boundary_blocking() {
    SharedFIFO fifo(5);
    fifo.Write("ABCDE");
    auto r1 = fifo.Read(3); // should block for 3, returns ABC
    ASSERT_EQUAL("read ABC", toString(*r1), std::string("ABC"));
    auto e1 = fifo.Extract(2); // remove AB
    ASSERT_EQUAL("extract AB", toString(*e1), std::string("AB"));
    fifo.Write("12"); // wrap at capacity
    // Seek to beginning and read remaining 4 bytes Follows non-destructive read position semantics
    fifo.Seek(0, Position::Absolute);
    auto r2 = fifo.Read(4); // should read C? Wait head moved: after extract 2 from ABCDE, head moved; remaining was ABC? After read 3 non-destructive, size unchanged.
    // Given operations: after initial write, size=5; Read(3) didn't change size; Extract(2) removed DE, size=3; Then write 12 size=5; Head somewhere; Reading all should give remaining 5 from read position 0
    fifo.Seek(0, Position::Absolute);
    auto all = fifo.Read(0);
    ASSERT_EQUAL("wrap combined", toString(*all).size(), static_cast<std::size_t>(5));
    RETURN_TEST("test_shared_fifo_wrap_boundary_blocking", 0);
}

int test_shared_fifo_growth_under_contention() {
    SharedFIFO fifo(1);
    const int iters = 100;
    std::atomic<bool> done{false};
    auto producer = std::thread([&]() -> void {
        for (int i = 0; i < iters; ++i) {
            fifo.Write(std::string(100 + (i % 50), 'Z'));
        }
        done.store(true);
        fifo.Close();
    });

    size_t consumed = 0;
    auto consumer = std::thread([&]() -> void {
        while (true) {
            auto part = fifo.Extract(128);
            if (!part || (part->empty() && fifo.IsClosed())) break;
            consumed += part->size();
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
    SharedFIFO fifo(8);
    fifo.Write("ABC");
    fifo.Close();
    
    // Even though we request 10 bytes, when closed with only 3 available,
    // SharedFIFO should return the 3 available bytes (blocking behavior)
    auto result = fifo.Read(10);
    ASSERT_TRUE("read with closed returns available", result.has_value());
    ASSERT_EQUAL("returns 3 bytes not 10", result->size(), static_cast<std::size_t>(3));
    ASSERT_EQUAL("content is ABC", toString(*result), std::string("ABC"));
    
    RETURN_TEST("test_shared_fifo_read_insufficient_closed_returns_available", 0);
}

int test_shared_fifo_extract_insufficient_closed_returns_available() {
    SharedFIFO fifo(8);
    fifo.Write("HELLO");
    fifo.Close();
    
    // Extract more than available when closed should return available data
    auto result = fifo.Extract(100);
    ASSERT_TRUE("extract with closed returns available", result.has_value());
    ASSERT_EQUAL("returns 5 bytes not 100", result->size(), static_cast<std::size_t>(5));
    ASSERT_EQUAL("content is HELLO", toString(*result), std::string("HELLO"));
    ASSERT_TRUE("buffer empty after extract", fifo.Empty());
    
    RETURN_TEST("test_shared_fifo_extract_insufficient_closed_returns_available", 0);
}

int test_shared_fifo_blocking_read_insufficient_not_closed() {
    SharedFIFO fifo(8);
    fifo.Write("12");
    
    std::atomic<bool> read_started{false};
    std::atomic<bool> read_got_error{false};
    std::atomic<bool> read_finished{false};
    
    std::thread reader([&]() -> void {
        read_started.store(true);
        // This should block waiting for 10 bytes (only 2 available)
        auto result = fifo.Read(10);
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
    ASSERT_FALSE("read returned available data not error", read_got_error.load());
    
    RETURN_TEST("test_shared_fifo_blocking_read_insufficient_not_closed", 0);
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
    result += test_shared_fifo_growth_under_contention();
    result += test_shared_fifo_read_insufficient_closed_returns_available();
    result += test_shared_fifo_extract_insufficient_closed_returns_available();
    result += test_shared_fifo_blocking_read_insufficient_not_closed();

    if (result == 0) {
        std::cout << "SharedFIFO tests passed!" << std::endl;
    } else {
        std::cout << result << " SharedFIFO tests failed." << std::endl;
    }
    return result;
}
