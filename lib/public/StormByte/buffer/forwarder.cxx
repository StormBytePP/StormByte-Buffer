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
	if (!IsReadable()) {
		return StormByte::Unexpected(ReadError("Buffer in not readable"));
	}
	
	// count must be > 0 because we cannot determine external reader size
	if (count == 0) {
		return StormByte::Unexpected(ReadError("Read count must be greater than 0 for Forwarder"));
	}
	
	const std::size_t available = m_buffer.AvailableBytes();
	
	// If buffer has enough data, read from buffer only
	if (available >= count) {
		return m_buffer.Read(count);
	}
	
	// Read what we have from buffer
	std::vector<std::byte> result;
	if (available > 0) {
		auto buffer_data = m_buffer.Read(available);
		if (buffer_data.has_value()) {
			result = std::move(*buffer_data);
		}
	}
	
	// Read remaining from external function
	const std::size_t remaining = count - result.size();
	auto external_data = m_readFunction(remaining);
	if (!external_data) {
		return StormByte::Unexpected(ReadError("Forwarder read failed: " + std::string(external_data.error()->what())));
	}
	
	// Combine results
	result.insert(result.end(), external_data->begin(), external_data->end());
	return result;
}

ExpectedData<ReadError> Forwarder::Extract(std::size_t count) {
	if (!IsReadable()) {
		return StormByte::Unexpected(ReadError("Buffer in not readable"));
	}
	
	// count must be > 0 because we cannot determine external reader size
	if (count == 0) {
		return StormByte::Unexpected(ReadError("Extract count must be greater than 0 for Forwarder"));
	}
	
	const std::size_t available = m_buffer.AvailableBytes();
	
	// If buffer has enough data, extract from buffer only
	if (available >= count) {
		return m_buffer.Extract(count);
	}
	
	// Extract what we have from buffer
	std::vector<std::byte> result;
	if (available > 0) {
		auto buffer_data = m_buffer.Extract(available);
		if (buffer_data.has_value()) {
			result = std::move(*buffer_data);
		}
	}
	
	// Read remaining from external function (Extract is destructive, so we just read)
	const std::size_t remaining = count - result.size();
	auto external_data = m_readFunction(remaining);
	if (!external_data) {
		return StormByte::Unexpected(ReadError("Forwarder read failed: " + std::string(external_data.error()->what())));
	}
	
	// Combine results
	result.insert(result.end(), external_data->begin(), external_data->end());
	return result;
}

ExpectedVoid<WriteError> Forwarder::Write(const std::vector<std::byte>& data) {
	if (!IsWritable()) {
		return StormByte::Unexpected(WriteError("Buffer is not writable"));
	}
	
	if (data.empty()) {
		return {};
	}
	
	// Direct passthrough to external write function
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

void Forwarder::Clear() noexcept {
	m_buffer.Clear();
}

void Forwarder::Clean() noexcept {
	m_buffer.Clean();
}

void Forwarder::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	m_buffer.Seek(offset, mode);
}

void Forwarder::Skip(const std::size_t& count) noexcept {
	m_buffer.Skip(count);
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