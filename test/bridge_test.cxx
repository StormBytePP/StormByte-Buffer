#include <StormByte/buffer/bridge.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>

using StormByte::Buffer::Bridge;

int test_bridge_passthrough_success() {
    std::string payload = "HELLO";
    auto data = StormByte::String::ToByteVector(payload);

    StormByte::Buffer::ExternalReadFunction readFn = [data](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        if (count == 0) return std::vector<std::byte>{};
        std::size_t n = std::min(count, data.size());
        return std::vector<std::byte>(data.begin(), data.begin() + n);
    };

    std::vector<std::byte> received;
    StormByte::Buffer::ExternalWriteFunction writeFn = [&received](const std::vector<std::byte>& d) -> StormByte::Buffer::ExpectedVoid<StormByte::Buffer::WriteError> {
        received = d;
        return {};
    };

    Bridge b(readFn, writeFn);
    auto res = b.Passthrough(payload.size());
    ASSERT_TRUE("test_bridge_passthrough_success result", res.has_value());
    ASSERT_EQUAL("test_bridge_passthrough_success content", StormByte::String::FromByteVector(received), payload);
    RETURN_TEST("test_bridge_passthrough_success", 0);
}

int test_bridge_passthrough_zero_noop() {
    bool read_called = false;
    bool write_called = false;

    StormByte::Buffer::ExternalReadFunction readFn = [&read_called](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        read_called = true;
        return StormByte::Unexpected(StormByte::Buffer::ReadError("should not be called"));
    };

    StormByte::Buffer::ExternalWriteFunction writeFn = [&write_called](const std::vector<std::byte>& d) -> StormByte::Buffer::ExpectedVoid<StormByte::Buffer::WriteError> {
        write_called = true;
        return {};
    };

    Bridge b(readFn, writeFn);
    auto res = b.Passthrough(0);
    ASSERT_TRUE("test_bridge_passthrough_zero_noop result", res.has_value());
    ASSERT_FALSE("read not called", read_called);
    ASSERT_FALSE("write not called", write_called);
    RETURN_TEST("test_bridge_passthrough_zero_noop", 0);
}

int test_bridge_passthrough_read_failure() {
    StormByte::Buffer::ExternalReadFunction readFn = [](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        return StormByte::Unexpected(StormByte::Buffer::ReadError("read failed"));
    };

    StormByte::Buffer::ExternalWriteFunction writeFn = [](const std::vector<std::byte>& d) -> StormByte::Buffer::ExpectedVoid<StormByte::Buffer::WriteError> {
        return {};
    };

    Bridge b(readFn, writeFn);
    auto res = b.Passthrough(1);
    ASSERT_FALSE("test_bridge_passthrough_read_failure should fail", res.has_value());
    RETURN_TEST("test_bridge_passthrough_read_failure", 0);
}

int test_bridge_passthrough_write_failure() {
    std::string payload = "DATA";
    auto data = StormByte::String::ToByteVector(payload);

    StormByte::Buffer::ExternalReadFunction readFn = [data](const std::size_t& count) -> StormByte::Buffer::ExpectedData<StormByte::Buffer::ReadError> {
        return data;
    };

    StormByte::Buffer::ExternalWriteFunction writeFn = [](const std::vector<std::byte>& d) -> StormByte::Buffer::ExpectedVoid<StormByte::Buffer::WriteError> {
        return StormByte::Unexpected(StormByte::Buffer::WriteError("write failed"));
    };

    Bridge b(readFn, writeFn);
    auto res = b.Passthrough(4);
    ASSERT_FALSE("test_bridge_passthrough_write_failure should fail", res.has_value());
    RETURN_TEST("test_bridge_passthrough_write_failure", 0);
}

int main() {
    int result = 0;
    result += test_bridge_passthrough_success();
    result += test_bridge_passthrough_zero_noop();
    result += test_bridge_passthrough_read_failure();
    result += test_bridge_passthrough_write_failure();

    if (result == 0) {
        std::cout << "Bridge tests passed!" << std::endl;
    } else {
        std::cout << result << " Bridge tests failed." << std::endl;
    }
    return result;
}
