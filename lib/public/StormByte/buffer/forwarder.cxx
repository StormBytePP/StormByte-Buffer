#include <StormByte/buffer/forwarder.hxx>

using namespace StormByte::Buffer;

Forwarder::Forwarder(const ExternalReadFunction& readFunc) noexcept:
m_readFunction(readFunc), m_writeFunction(ErrorWriteFunction()) {}

Forwarder::Forwarder(const ExternalWriteFunction& writeFunc) noexcept:
m_readFunction(ErrorReadFunction()), m_writeFunction(writeFunc) {}

Forwarder::Forwarder(const ExternalReadFunction& readFunc, const ExternalWriteFunction& writeFunc) noexcept:
m_readFunction(readFunc), m_writeFunction(writeFunc) {}

ExpectedData<ReadError> Forwarder::Read(const std::size_t& count) const {
	if (count == 0) {
		// Read 0 bytes is a noop
		return {};
	}
	return m_readFunction(count);
}

ExpectedVoid<WriteError> Forwarder::Write(const std::vector<std::byte>& data) {
	return m_writeFunction(data);
}

ExpectedVoid<WriteError> Forwarder::Write(const Buffer::FIFO& data) {
	auto expected_data = data.Read(0);
	if (!expected_data) {
		// If there is no data to write then this is a noop
		return {};
	}
	return m_writeFunction(expected_data.value());
}

ExternalReadFunction Forwarder::ErrorReadFunction() noexcept {
	return [](const std::size_t&) -> ExpectedData<ReadError> {
		return StormByte::Unexpected(ReadError("No read function defined"));
	};
}

ExternalReadFunction Forwarder::NoopReadFunction() noexcept {
	return [](const std::size_t&) -> ExpectedData<ReadError> {
		return {};
	};
}

ExternalWriteFunction Forwarder::ErrorWriteFunction() noexcept {
	return [](const std::vector<std::byte>&) -> ExpectedVoid<WriteError> {
		return StormByte::Unexpected(WriteError("No write function defined"));
	};
}

ExternalWriteFunction Forwarder::NoopWriteFunction() noexcept {
	return [](const std::vector<std::byte>&) -> ExpectedVoid<WriteError> {
		return {};
	};
}