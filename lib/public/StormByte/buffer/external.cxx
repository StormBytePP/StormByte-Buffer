#include <StormByte/buffer/external.hxx>

using StormByte::Buffer::DataType;
using namespace StormByte::Buffer;

bool ExternalBufferReader::Read(std::size_t bytes, DataType& out) const noexcept {
	DataType data;
	bool read_ok = m_buffer.get().Read(bytes, data);
	if (read_ok)
		out = std::move(data);

	return read_ok;
}

bool ExternalBufferReader::Read(std::size_t bytes, DataType& out) noexcept {
	DataType data;
	bool read_ok = m_buffer.get().Read(bytes, data);
	if (read_ok)
		out = std::move(data);

	return read_ok;
}

bool ExternalBufferWriter::Write(DataType&& in) noexcept {
	return m_buffer.get().Write(std::move(in));
}
