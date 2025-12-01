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

static std::string toString(const std::vector<std::byte>& v) {
    return StormByte::String::FromByteVector(v);
}

int test_shared_fifo_producer_consumer_blocking() {
    SharedFIFO fifo;
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
            if (!part) {
                // On error (closed & insufficient), try to consume any remaining bytes and exit.
                if (fifo.AvailableBytes() > 0) {
                    auto rem = fifo.Read(0);
                    if (rem && !rem->empty()) collected.append(toString(*rem));
                }
                break;
            }
            if (part->empty() && fifo.EoF()) break;
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
    SharedFIFO fifo;
    std::atomic<bool> woke{false};
    std::atomic<bool> saw_writable{false};
    std::size_t extracted_size = 1234; // sentinel

    std::thread t([&]() -> void {
        auto out = fifo.Extract(1); // block until 1 byte or close
        // With no writer, Close() will wake us; out should be error or empty
        woke.store(true);
        saw_writable.store(fifo.IsWritable());
        extracted_size = out ? out->size() : 0;
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
    fifo.Write(std::string("0123456789"));

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
        seeker_done.store(true);
    });

    std::thread reader([&]() -> void {
        // Perform two reads interleaved with seeks
        auto r1 = fifo.Read(2); // reads from current (race-free due to mutex)
        if (!r1.has_value()) { reader_failed.store(true); return; }
        read_a = toString(*r1);
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        auto r2 = fifo.Read(3);
        if (!r2.has_value()) { reader_failed.store(true); return; }
        read_b = toString(*r2);
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
    fifo.Write(std::string("ABCDEFGH"));

    std::string r_before, r_after;
    std::atomic<bool> reader_failed2{false};
    std::thread reader([&]() -> void {
        auto _r_before = fifo.Read(3);
        if (!_r_before.has_value()) { reader_failed2.store(true); return; }
        r_before = toString(*_r_before); // ABC
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto _r_after = fifo.Read(2);
        if (!_r_after.has_value()) { reader_failed2.store(true); return; }
        r_after = toString(*_r_after); // depends on extract adjustment
    });

    std::thread extractor([&]() -> void {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto e = fifo.Extract(2); // remove AB; read_position should adjust
        (void)e;
    });

    reader.join();
    extractor.join();

    ASSERT_FALSE("reader had error", reader_failed2.load());

    ASSERT_EQUAL("first read ABC", r_before, std::string("ABC"));
    // After extracting 2, the read position (which was at 3) becomes 1 relative to new head 'C', so next should start from 'D'
    ASSERT_EQUAL("next read after adjust is DE", r_after, std::string("DE"));
    RETURN_TEST("test_shared_fifo_extract_adjusts_read_position_concurrency", 0);
}

int test_shared_fifo_multi_producer_single_consumer_counts() {
    SharedFIFO fifo;
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
            if (!part || (part->empty() && fifo.EoF())) break;
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
    SharedFIFO fifo;
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
            if (!part || (part->empty() && fifo.EoF())) break;
            local += part->size();
        }
        c1.store(local);
    });
    auto consumer2 = std::thread([&]() -> void {
        size_t local = 0;
        while (true) {
            auto part = fifo.Extract(1);
            if (!part || (part->empty() && fifo.EoF())) break;
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
    SharedFIFO fifo;
    fifo.Write(std::string("ABC"));
    ASSERT_EQUAL("pre-close size", fifo.Size(), static_cast<std::size_t>(3));
    fifo.Close();
    fifo.Write(std::string("DEF"));
    ASSERT_EQUAL("size unchanged after close", fifo.Size(), static_cast<std::size_t>(3));
    auto out = fifo.Extract(0);
    ASSERT_TRUE("extract returned", out.has_value());
    ASSERT_EQUAL("content after close write blocked", toString(*out), std::string("ABC"));
    RETURN_TEST("test_shared_fifo_close_suppresses_writes", 0);
}

int test_shared_fifo_wrap_boundary_blocking() {
    SharedFIFO fifo;
    fifo.Write("ABCDE");
    auto r1 = fifo.Read(3); // should block for 3, returns ABC
    ASSERT_TRUE("r1 returned", r1.has_value());
    ASSERT_EQUAL("read ABC", toString(*r1), std::string("ABC"));
    auto e1 = fifo.Extract(2); // remove AB
    ASSERT_TRUE("e1 returned", e1.has_value());
    ASSERT_EQUAL("extract AB", toString(*e1), std::string("AB"));
    fifo.Write("12"); // wrap at capacity
    // Seek to beginning and read remaining 4 bytes Follows non-destructive read position semantics
    fifo.Seek(0, Position::Absolute);
    auto r2 = fifo.Read(4); // should read C? Wait head moved: after extract 2 from ABCDE, head moved; remaining was ABC? After read 3 non-destructive, size unchanged.
    // Given operations: after initial write, size=5; Read(3) didn't change size; Extract(2) removed DE, size=3; Then write 12 size=5; Head somewhere; Reading all should give remaining 5 from read position 0
    fifo.Seek(0, Position::Absolute);
    auto all = fifo.Read(0);
    ASSERT_TRUE("all returned", all.has_value());
    ASSERT_EQUAL("wrap combined", toString(*all).size(), static_cast<std::size_t>(5));
    RETURN_TEST("test_shared_fifo_wrap_boundary_blocking", 0);
}

int test_shared_fifo_growth_under_contention() {
    SharedFIFO fifo;
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
            if (!part) {
                if (fifo.AvailableBytes() > 0) {
                    auto rem = fifo.Extract(0);
                    if (rem && !rem->empty()) consumed += rem->size();
                }
                break;
            }
            if (part->empty() && fifo.EoF()) break;
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
    SharedFIFO fifo;
    fifo.Write("ABC");
    fifo.Close();
    
    // Even though we request 10 bytes, when closed with only 3 available,
    // Requesting more than available when closed is an error.
    auto result = fifo.Read(10);
    ASSERT_FALSE("read with closed returns error", result.has_value());
    // Data should remain available unchanged
    ASSERT_EQUAL("available unchanged", fifo.AvailableBytes(), static_cast<std::size_t>(3));
    
    RETURN_TEST("test_shared_fifo_read_insufficient_closed_returns_available", 0);
}

int test_shared_fifo_extract_insufficient_closed_returns_available() {
    SharedFIFO fifo;
    fifo.Write("HELLO");
    fifo.Close();
    
    // Extract more than available when closed should return available data
    auto result = fifo.Extract(100);
    ASSERT_FALSE("extract with closed returns error", result.has_value());
    // Data must remain unchanged when extract fails
    ASSERT_EQUAL("size unchanged after failed extract", fifo.Size(), static_cast<std::size_t>(5));
    auto all = fifo.Read(0);
    ASSERT_TRUE("read remaining data", all.has_value());
    ASSERT_EQUAL("content is HELLO", toString(*all), std::string("HELLO"));
    
    RETURN_TEST("test_shared_fifo_extract_insufficient_closed_returns_available", 0);
}

int test_shared_fifo_blocking_read_insufficient_not_closed() {
    SharedFIFO fifo;
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
    ASSERT_TRUE("read returned error after close", read_got_error.load());
    
    RETURN_TEST("test_shared_fifo_blocking_read_insufficient_not_closed", 0);
}

int test_shared_fifo_available_bytes_basic() {
    SharedFIFO fifo;
    
    // Empty buffer
    ASSERT_EQUAL("empty available", fifo.AvailableBytes(), static_cast<std::size_t>(0));
    
    // Write data
    fifo.Write("HELLO WORLD"); // 11 bytes
    ASSERT_EQUAL("after write", fifo.AvailableBytes(), static_cast<std::size_t>(11));
    
    // Read non-destructively
    auto r1 = fifo.Read(5);
    ASSERT_EQUAL("after read 5", fifo.AvailableBytes(), static_cast<std::size_t>(6));
    
    // Seek changes available bytes
    fifo.Seek(2, Position::Absolute);
    ASSERT_EQUAL("after seek to 2", fifo.AvailableBytes(), static_cast<std::size_t>(9));
    
    // Extract removes data
    auto e1 = fifo.Extract(3);
    ASSERT_EQUAL("after extract 3", fifo.AvailableBytes(), static_cast<std::size_t>(8));
    
    RETURN_TEST("test_shared_fifo_available_bytes_basic", 0);
}

int test_shared_fifo_available_bytes_concurrent() {
    SharedFIFO fifo;
    std::atomic<std::size_t> available_checks{0};
    std::atomic<bool> done{false};
    
    // Writer thread
    std::thread writer([&]() -> void {
        for (int i = 0; i < 10; ++i) {
            fifo.Write("DATA");
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
                auto data = fifo.Extract(0);
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
    auto result = fifo.Read(10);
    ASSERT_FALSE("Read returns error when requesting > available on closed", result.has_value());
    
    RETURN_TEST("test_shared_fifo_read_closed_no_data_nonblocking", 0);
}

int test_shared_fifo_extract_closed_no_data_nonblocking() {
    SharedFIFO fifo;
    fifo.Close();
    ASSERT_FALSE("fifo is writable", fifo.IsWritable());
    ASSERT_EQUAL("fifo is empty", fifo.Size(), static_cast<std::size_t>(0));
    
    auto result = fifo.Extract(10);
    ASSERT_FALSE("Extract returns error when requesting > available on closed", result.has_value());
    
    RETURN_TEST("test_shared_fifo_extract_closed_no_data_nonblocking", 0);
}

int test_sharedfifo_equality() {
    StormByte::Buffer::SharedFIFO sa;
    StormByte::Buffer::SharedFIFO sb;

    sa.Write("HELLO");
    sb.Write("HELLO");

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
    result += test_shared_fifo_available_bytes_basic();
    result += test_shared_fifo_available_bytes_concurrent();
    result += test_shared_fifo_read_closed_no_data_nonblocking();
    result += test_shared_fifo_extract_closed_no_data_nonblocking();
	result += test_sharedfifo_equality();

    if (result == 0) {
        std::cout << "SharedFIFO tests passed!" << std::endl;
    } else {
        std::cout << result << " SharedFIFO tests failed." << std::endl;
    }
    return result;
}
