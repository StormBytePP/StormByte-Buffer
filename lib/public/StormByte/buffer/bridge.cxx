#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/fifo.hxx>

#include <algorithm>

using namespace StormByte::Buffer;

bool Bridge::Flush() const noexcept {
	// After PassthroughWrite the internal buffer will contain at most
	// <chunk_size bytes (the remainder). We can therefore extract all
	// remaining bytes and write them in a single call.
	if (m_buffer.Empty())
		return true;

	DataType data;
	// Extract all available data (count == 0 means "all" for FIFO)
	const bool extracted = m_buffer.Extract(0, data);
	if (!extracted)
		return false;

	return m_write_handler->Write(std::move(data));
}

bool Bridge::Passthrough(std::size_t bytes) const noexcept {
	DataType out;
	bool operation_ok = m_read_handler->Read(bytes, out);
	if (operation_ok) {
		operation_ok = PassthroughWrite(std::move(out));
	}
	return operation_ok;
}

bool Bridge::Passthrough(std::size_t bytes) noexcept {
	DataType out;
	bool operation_ok = m_read_handler->Read(bytes, out);
	if (operation_ok) {
		operation_ok = PassthroughWrite(std::move(out));
	}
	return operation_ok;
}

bool Bridge::PassthroughWrite(DataType&& data) const noexcept {
	// Combine existing internal buffer and newly read data into a single
	// contiguous temporary. We do not mutate `m_buffer` until we have
	// attempted to write chunks to the writer; this allows rolling back to
	// the original combined remainder if a write fails.
	const DataType& existing = m_buffer.Data();
	DataType combined;
	combined.reserve(existing.size() + data.size());
	combined.insert(combined.end(), existing.begin(), existing.end());
	combined.insert(combined.end(), std::make_move_iterator(data.begin()), std::make_move_iterator(data.end()));

	std::size_t pos = 0;
	bool operation_ok = true;

	// Special-case: chunk_size == 0 disables chunking — write all data at once.
	if (m_chunk_size == 0) {
		// Clear internal buffer (we're not accumulating leftovers in this mode)
		m_buffer.Clear();
		if (combined.empty()) return true;
		return m_write_handler->Write(std::move(combined));
	}

	// Write as many full chunks as possible from the combined buffer.
	while (operation_ok && pos + m_chunk_size <= combined.size()) {
		DataType chunk(combined.begin() + pos, combined.begin() + pos + m_chunk_size);
		operation_ok = m_write_handler->Write(std::move(chunk));
		if (operation_ok) pos += m_chunk_size;
	}

	// Whatever remains (from `pos` to end) should be stored back into m_buffer.
	// Move-construct the remainder from the combined temporary to avoid an
	// extra copy when element types are movable.
	DataType remainder(
		std::make_move_iterator(combined.begin() + pos),
		std::make_move_iterator(combined.end())
	);
	m_buffer.Clear();
	if (!remainder.empty()) {
		m_buffer.Write(std::move(remainder));
	}

	return operation_ok;
}