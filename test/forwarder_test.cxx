#include <StormByte/buffer/forwarder.hxx>
#include <StormByte/buffer/producer.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>

using StormByte::Buffer::Forwarder;

int test_forwarder_read_only() {
    auto data = StormByte::String::ToByteVector("README");

    // Read lambda returns the stored data (ignores count for simplicity)
    StormByte::Buffer::ExternalReadFunction readFn = [data](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        if (count == 0) return data;
        std::vector<std::byte> out;
        std::size_t n = std::min(count, data.size());
        out.insert(out.end(), data.begin(), data.begin() + n);
        return out;
    };

    Forwarder f(readFn);
    auto r = f.Read(data.size());
    ASSERT_TRUE("test_forwarder_read_only read succeeded", r.has_value());
    ASSERT_EQUAL("test_forwarder_read_only content", StormByte::String::FromByteVector(*r), std::string("README"));

    // Writes are not configured -> should return an error
    auto wres = f.Write(StormByte::String::ToByteVector("X"));
    ASSERT_FALSE("test_forwarder_read_only write fails", wres.has_value());

    RETURN_TEST("test_forwarder_read_only", 0);
}

int test_forwarder_write_only() {
    std::vector<std::byte> received;
    StormByte::Buffer::ExternalWriteFunction writeFn = [&received](const std::vector<std::byte>& d) -> StormByte::Buffer::ExpectedVoid<StormByte::Buffer::WriteError> {
        received = d;
        return {};
    };

    Forwarder f(writeFn);

    auto wres = f.Write(StormByte::String::ToByteVector("XYZ"));
    ASSERT_TRUE("test_forwarder_write_only write succeeded", wres.has_value());
    ASSERT_EQUAL("test_forwarder_write_only content", StormByte::String::FromByteVector(received), std::string("XYZ"));

    // Reads not configured -> should return an error when requesting >0 bytes
    auto r = f.Read(1);
    ASSERT_FALSE("test_forwarder_write_only read fails", r.has_value());

    RETURN_TEST("test_forwarder_write_only", 0);
}

int test_forwarder_write_fifo() {
    // prepare a FIFO with some data
    StormByte::Buffer::FIFO fifo;
    fifo.Write(std::string("ABC"));

    std::vector<std::byte> received;
    StormByte::Buffer::ExternalWriteFunction writeFn = [&received](const std::vector<std::byte>& d) -> StormByte::Buffer::ExpectedVoid<StormByte::Buffer::WriteError> {
        received = d;
        return {};
    };

    Forwarder f(Forwarder::NoopReadFunction(), writeFn);

    auto wres = f.Write(fifo);
    ASSERT_TRUE("test_forwarder_write_fifo write succeeded", wres.has_value());
    ASSERT_EQUAL("test_forwarder_write_fifo content", StormByte::String::FromByteVector(received), std::string("ABC"));

    RETURN_TEST("test_forwarder_write_fifo", 0);
}

int test_forwarder_as_producer() {
	// Read lambda returns count bytes of 'A's
    StormByte::Buffer::ExternalReadFunction readFn = [](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
		return StormByte::String::ToByteVector(std::string(count, 'A'));
    };

	std::shared_ptr<Forwarder> forwarder = std::make_shared<Forwarder>(readFn);
	StormByte::Buffer::Producer producer(forwarder);
	StormByte::Buffer::Consumer consumer(producer.Consumer());

	auto data_expected = consumer.Read(4);
	ASSERT_TRUE("test_forwarder_as_producer read succeeded", data_expected.has_value());
	ASSERT_EQUAL("test_forwarder_as_producer content", StormByte::String::FromByteVector(*data_expected), std::string(4, 'A'));

	data_expected = consumer.Read(9);
	ASSERT_TRUE("test_forwarder_as_producer read succeeded", data_expected.has_value());
	ASSERT_EQUAL("test_forwarder_as_producer content", StormByte::String::FromByteVector(*data_expected), std::string(9, 'A'));

	RETURN_TEST("test_forwarder_as_producer", 0);
}

int test_forwarder_peek_basic() {
    int external_read_calls = 0;
    
    // External read function that tracks calls
    auto reader = [&external_read_calls](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        external_read_calls++;
        std::string data = "EXTERNAL";
        std::size_t n = std::min(count, data.size());
        return StormByte::String::ToByteVector(data.substr(0, n));
    };
    
    Forwarder forwarder(reader);
    
    // Peek 3 bytes - will trigger external read and buffer the data
    auto peek1 = forwarder.Peek(3);
    ASSERT_TRUE("peek returned", peek1.has_value());
    ASSERT_EQUAL("peek content", StormByte::String::FromByteVector(*peek1), std::string("EXT"));
    ASSERT_EQUAL("external call after first peek", external_read_calls, 1);
    
    // Peek again - should return same data from buffer, no additional external call
    auto peek2 = forwarder.Peek(3);
    ASSERT_TRUE("peek2 returned", peek2.has_value());
    ASSERT_EQUAL("peek2 content", StormByte::String::FromByteVector(*peek2), std::string("EXT"));
    ASSERT_EQUAL("still one external call", external_read_calls, 1);
    
    RETURN_TEST("test_forwarder_peek_basic", 0);
}

int test_forwarder_peek_triggers_external_read() {
    int external_read_calls = 0;
    
    // External read function that tracks calls
    auto reader = [&external_read_calls](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        external_read_calls++;
        std::string data = "EXTERNAL";
        std::size_t n = std::min(count, data.size());
        return StormByte::String::ToByteVector(data.substr(0, n));
    };
    
    Forwarder forwarder(reader);
    
    // Peek 5 bytes - will read from external
    auto peek = forwarder.Peek(5);
    ASSERT_TRUE("peek with external returned", peek.has_value());
    ASSERT_EQUAL("peek content", StormByte::String::FromByteVector(*peek), std::string("EXTER"));
    ASSERT_EQUAL("external called once", external_read_calls, 1);
    
    RETURN_TEST("test_forwarder_peek_triggers_external_read", 0);
}

int test_forwarder_peek_then_read_no_external_call() {
    int external_read_calls = 0;
    
    // External read function that tracks calls
    auto reader = [&external_read_calls](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        external_read_calls++;
        std::string data = "EXTERNAL_DATA";
        std::size_t n = std::min(count, data.size());
        return StormByte::String::ToByteVector(data.substr(0, n));
    };
    
    Forwarder forwarder(reader);
    
    // Peek 2 bytes - will trigger external read
    auto peek_result = forwarder.Peek(2);
    ASSERT_TRUE("peek returned", peek_result.has_value());
    std::string peek_data = StormByte::String::FromByteVector(*peek_result);
    ASSERT_EQUAL("peek content", peek_data, std::string("EX"));
    ASSERT_EQUAL("external called once for peek", external_read_calls, 1);
    
    // Now Read 2 bytes - should come from buffer (filled by Peek), no external call
    auto read_result = forwarder.Read(2);
    ASSERT_TRUE("read returned", read_result.has_value());
    std::string read_data = StormByte::String::FromByteVector(*read_result);
    ASSERT_EQUAL("read content matches peek", read_data, peek_data);
    ASSERT_EQUAL("no additional external call for read", external_read_calls, 1);
    
    RETURN_TEST("test_forwarder_peek_then_read_no_external_call", 0);
}

int test_forwarder_peek_insufficient_buffer() {
    // External read function that returns less than requested
    auto reader = [](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        return StormByte::String::ToByteVector(std::string("AB")); // Always returns 2 bytes
    };
    
    Forwarder forwarder(reader);
    
    // Peek 10 bytes - external will only provide 2
    auto peek = forwarder.Peek(10);
    ASSERT_FALSE("peek with insufficient data returns error", peek.has_value());
    
    RETURN_TEST("test_forwarder_peek_insufficient_buffer", 0);
}

int test_forwarder_peek_zero_returns_error() {
    auto reader = [](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        return StormByte::String::ToByteVector(std::string("DATA"));
    };
    
    Forwarder forwarder(reader);
    
    // Peek(0) should return error for Forwarder
    auto peek = forwarder.Peek(0);
    ASSERT_FALSE("peek(0) returns error", peek.has_value());
    
    RETURN_TEST("test_forwarder_peek_zero_returns_error", 0);
}

int main() {
    int result = 0;
    result += test_forwarder_read_only();
    result += test_forwarder_write_only();
    result += test_forwarder_write_fifo();
	result += test_forwarder_as_producer();
    result += test_forwarder_peek_basic();
    result += test_forwarder_peek_triggers_external_read();
    result += test_forwarder_peek_then_read_no_external_call();
    result += test_forwarder_peek_insufficient_buffer();
    result += test_forwarder_peek_zero_returns_error();

    if (result == 0) {
        std::cout << "Forwarder tests passed!" << std::endl;
    } else {
        std::cout << result << " Forwarder tests failed." << std::endl;
    }
    return result;
}
