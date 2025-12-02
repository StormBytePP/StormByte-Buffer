#include <StormByte/buffer/forwarder.hxx>
#include <StormByte/string.hxx>

using namespace StormByte::Buffer;

Forwarder::Forwarder(const ExternalReadFunction& readFunc) noexcept:
m_readFunction(readFunc), m_writeFunction(ErrorWriteFunction()) {}

Forwarder::Forwarder(const ExternalWriteFunction& writeFunc) noexcept:
m_readFunction(ErrorReadFunction()), m_writeFunction(writeFunc) {}

Forwarder::Forwarder(const ExternalReadFunction& readFunc, const ExternalWriteFunction& writeFunc) noexcept:
m_readFunction(readFunc), m_writeFunction(writeFunc) {}

ExpectedData<ReadError> Forwarder::Read(std::size_t count) const {
	if (count == 0) {
		// A read of 0 bytes is a no-op that returns an empty vector
		return {};
	}
	if (!IsReadable()) {
		return StormByte::Unexpected(ReadError("Buffer in not readable"));
	}
	auto data = m_readFunction(count);
	if (!data) {
		return StormByte::Unexpected(ReadError("Forwarder read failed: " + std::string(data.error()->what())));
	}
	return *data;
}

ExpectedData<ReadError> Forwarder::Extract(std::size_t count) {
	return Read(count);
}

ExpectedVoid<WriteError> Forwarder::Write(const std::vector<std::byte>& data) {
	if (!IsWritable() || data.empty()) {
		return StormByte::Unexpected(WriteError("Buffer is not writable or data is empty"));
	}
	return m_writeFunction(data);
}

ExpectedVoid<WriteError> Forwarder::Write(const std::string& data) {
	return Write(String::ToByteVector(data));
}

ExpectedVoid<WriteError> Forwarder::Write(const FIFO& other) {
	auto data = other.Read(0);
	if (!data)
		return StormByte::Unexpected(WriteError("Failed to read from other FIFO"));
	return Write(*data);
}

ExpectedVoid<WriteError> Forwarder::Write(FIFO&& other) noexcept {
	return Write(other);
}

void Forwarder::Clear() noexcept {}

void Forwarder::Clean() noexcept {}

void Forwarder::Seek(const std::ptrdiff_t&, const Position&) const noexcept {}

void Forwarder::Skip(const std::size_t&) noexcept {}

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