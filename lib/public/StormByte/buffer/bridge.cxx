#include <StormByte/buffer/bridge.hxx>

#include <algorithm>

using namespace StormByte::Buffer;

// ---------------------------------------------------------------------------
// Flush / Close / Error
// ---------------------------------------------------------------------------

bool Bridge::Flush() const noexcept {
	if (m_buffer.Empty())
		return true;

	DataType data;
	if (!m_buffer.Extract(0, data))
		return false;

	if (data.empty())
		return true;

	return m_write_handler->Write(std::move(data));
}

bool Bridge::FlushAndClose() const noexcept {
	const bool ok = Flush();
	m_write_handler->Close();
	return ok;
}

void Bridge::SetError() const noexcept {
	m_write_handler->SetError();
}

// ---------------------------------------------------------------------------
// Passthrough entry points
// ---------------------------------------------------------------------------

bool Bridge::Passthrough(std::size_t bytes) const noexcept {
	// NO hacer early-return si bytes == 0:
	// 0 significa "todo lo disponible" en la semántica del reader.
	DataType out;
	if (!m_read_handler->Read(bytes, out))
		return false;
	return PassthroughWrite(std::move(out));
}

bool Bridge::Passthrough(std::size_t bytes) noexcept {
	DataType out;
	if (!m_read_handler->Extract(bytes, out)) {
		if (!m_read_handler->Read(bytes, out))
			return false;
	}
	return PassthroughWrite(std::move(out));
}

// ---------------------------------------------------------------------------
// Core chunking logic
// ---------------------------------------------------------------------------

bool Bridge::PassthroughWrite(DataType&& data) const noexcept {
	// Fast path: no previous leftovers and no chunking
	if (m_buffer.Empty() && m_chunk_size == 0) {
		if (data.empty())
			return true;
		return m_write_handler->Write(std::move(data));
	}

	// Merge previous leftovers + new data
	DataType combined;
	const DataType& existing = m_buffer.Data();
	combined.reserve(existing.size() + data.size());

	if (!existing.empty())
		combined.insert(combined.end(), existing.begin(), existing.end());

	if (!data.empty()) {
		combined.insert(combined.end(),
						std::make_move_iterator(data.begin()),
						std::make_move_iterator(data.end()));
	}

	// Clear the internal buffer; we will put back only the final remainder
	m_buffer.Clear();

	if (combined.empty())
		return true;

	// No chunking → write everything
	if (m_chunk_size == 0)
		return m_write_handler->Write(std::move(combined));

	// Write as many full chunks as possible
	std::size_t pos = 0;
	bool ok = true;

	while (ok && pos + m_chunk_size <= combined.size()) {
		DataType chunk(combined.begin() + static_cast<std::ptrdiff_t>(pos),
					combined.begin() + static_cast<std::ptrdiff_t>(pos + m_chunk_size));
		ok = m_write_handler->Write(std::move(chunk));
		if (ok)
			pos += m_chunk_size;
	}

	// Store the unwritten tail (if any) back into the internal buffer
	if (pos < combined.size()) {
		DataType remainder(
			std::make_move_iterator(combined.begin() + static_cast<std::ptrdiff_t>(pos)),
			std::make_move_iterator(combined.end())
		);
		(void)m_buffer.Write(std::move(remainder));
	}

	return ok;
}
