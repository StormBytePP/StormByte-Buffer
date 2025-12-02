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
    ASSERT_FALSE("test_forwarder_read_only write fails", wres);

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
    ASSERT_TRUE("test_forwarder_write_only write succeeded", wres);
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
    ASSERT_TRUE("test_forwarder_write_fifo write succeeded", wres);
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

int main() {
    int result = 0;
    result += test_forwarder_read_only();
    result += test_forwarder_write_only();
    result += test_forwarder_write_fifo();
	result += test_forwarder_as_producer();

    if (result == 0) {
        std::cout << "Forwarder tests passed!" << std::endl;
    } else {
        std::cout << result << " Forwarder tests failed." << std::endl;
    }
    return result;
}
