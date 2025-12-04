#include <StormByte/buffer/forwarder.hxx>
#include <StormByte/string.hxx>

using namespace StormByte::Buffer;

Forwarder::Forwarder(const ExternalReadFunction& readFunc) noexcept:
m_readFunction(readFunc), m_writeFunction(ErrorWriteFunction()) {}

Forwarder::Forwarder(const ExternalWriteFunction& writeFunc) noexcept:
m_readFunction(ErrorReadFunction()), m_writeFunction(writeFunc) {}

Forwarder::Forwarder(const ExternalReadFunction& readFunc, const ExternalWriteFunction& writeFunc) noexcept:
m_readFunction(readFunc), m_writeFunction(writeFunc) {}

ExpectedData<ReadError> Forwarder::Read(const std::size_t& count) const {
	if (!IsReadable()) {
		return StormByte::Unexpected(ReadError("Buffer in not readable"));
	}
	
	// count must be > 0 because we cannot determine external reader size
	if (count == 0) {
		return StormByte::Unexpected(ReadError("Read count must be greater than 0 for Forwarder"));
	}
	
	const std::size_t available = FIFO::AvailableBytes();
	
	// If buffer has enough data, read from buffer only
	if (available >= count) {
		return FIFO::Read(count);
	}
	
	// Read what we have from buffer
	std::vector<std::byte> result;
	if (available > 0) {
		auto buffer_data = FIFO::Read(available);
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

ExpectedData<ReadError> Forwarder::Extract(const std::size_t& count) {
	if (!IsReadable()) {
		return StormByte::Unexpected(ReadError("Buffer in not readable"));
	}
	
	// count must be > 0 because we cannot determine external reader size
	if (count == 0) {
		return StormByte::Unexpected(ReadError("Extract count must be greater than 0 for Forwarder"));
	}
	
	const std::size_t available = FIFO::AvailableBytes();
	
	// If buffer has enough data, extract from buffer only
	if (available >= count) {
		return FIFO::Extract(count);
	}
	
	// Extract what we have from buffer
	std::vector<std::byte> result;
	if (available > 0) {
		auto buffer_data = FIFO::Extract(available);
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
	FIFO::Clear();
}

void Forwarder::Clean() noexcept {
	FIFO::Clean();
}

void Forwarder::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	FIFO::Seek(offset, mode);
}

ExpectedData<ReadError> Forwarder::Peek(const std::size_t& count) const noexcept {
	if (count == 0)
		return StormByte::Unexpected(ReadError("Peek count must be greater than 0 for Forwarder"));
	if (count > FIFO::AvailableBytes()) {
		std::size_t needed = count - FIFO::AvailableBytes();
		auto read_data_expected = m_readFunction(needed);
		if (!read_data_expected)
			return StormByte::Unexpected(ReadError("Forwarder peek failed: " + std::string(read_data_expected.error()->what())));
		// Write to internal buffer (not passthrough) - need to cast away const
		(void)const_cast<Forwarder*>(this)->FIFO::Write(*read_data_expected);
	}
	return FIFO::Peek(count);
}

void Forwarder::Skip(const std::size_t& count) noexcept {
	FIFO::Skip(count);
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