#include <StormByte/buffer/bridge.hxx>

using namespace StormByte::Buffer;

Bridge::Bridge(const ExternalReadFunction& readFunc, const ExternalWriteFunction& writeFunc) noexcept:
Forwarder(readFunc, writeFunc) {}

ExpectedVoid<Exception> Bridge::Passthrough(const std::size_t& count) {
	if (count == 0) {
		// Passthrough of 0 bytes is a noop
		return {};
	}
	auto expected_data = Read(count);
	if (!expected_data) {
		return StormByte::Unexpected(Exception("Bridge read failed: " + std::string(expected_data.error()->what())));
	}
	auto write_result = Write(*expected_data);
	if (!write_result) {
		return StormByte::Unexpected(Exception("Bridge write failed: " + std::string(write_result.error()->what())));
	}
	return {};
}