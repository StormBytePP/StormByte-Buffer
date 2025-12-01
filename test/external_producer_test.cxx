#include <StormByte/buffer/external_producer.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <iostream>

using StormByte::Buffer::ExternalProducer;
using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int test_external_producer_basic() {
    // Prepare messages to be provided by the external reader
    std::vector<std::string> messages = {"Hello", "-", "External", "-", "Producer"};

    // ExternalReader: returns next chunk as vector<std::byte>, returns empty vector to signal EOF
    auto reader = [msgs = messages, idx = std::size_t{0}]() mutable -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReaderExhausted> {
        if (idx < msgs.size()) {
            const std::string &s = msgs[idx++];
            return StormByte::String::ToByteVector(s);
        }
        // Return empty vector to indicate no more data
        return std::vector<std::byte>();
    };

    ExternalProducer ext(reader);
    Consumer consumer = ext.Consumer();

    // Collect output until producer signals EOF and all bytes consumed.
    std::string collected;

    while (!consumer.EoF() || consumer.AvailableBytes() > 0) {
        auto chunk = consumer.Extract(0);
        if (chunk && !chunk->empty()) {
            collected += StormByte::String::FromByteVector(*chunk);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Expected concatenation
    std::string expected;
    for (const auto &m : messages) expected += m;

    ASSERT_TRUE("external producer produced data", !collected.empty());
    ASSERT_EQUAL("external producer content", collected, expected);

    RETURN_TEST("test_external_producer_basic", 0);
}

int main() {
    int result = 0;
    result += test_external_producer_basic();

    if (result == 0) std::cout << "ExternalProducer tests passed!" << std::endl;
    else std::cout << result << " ExternalProducer tests failed." << std::endl;
    return result;
}
