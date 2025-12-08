#include <StormByte/buffer/fifo.hxx>
#include <StormByte/helpers.hxx>
#include <StormByte/string.hxx>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

using namespace StormByte::Buffer;

FIFO::FIFO(const FIFO& other) noexcept: Generic(other), m_position_offset(other.m_position_offset) {}

FIFO::FIFO(FIFO&& other) noexcept: Generic(std::move(other)), m_position_offset(other.m_position_offset) {
}

FIFO& FIFO::operator=(const FIFO& other) {
	if (this != &other) {
		Generic::operator=(other);
		m_position_offset = other.m_position_offset;
	}
	return *this;
}

FIFO& FIFO::operator=(FIFO&& other) noexcept {
	if (this != &other) {
		Generic::operator=(std::move(other));
		m_position_offset = other.m_position_offset;
	}
	return *this;
}

void FIFO::Clean() noexcept {
	if (m_position_offset > 0 && m_position_offset <= m_buffer.size()) {
		// For vector, move remaining data to front instead of erase
		const std::size_t remaining = m_buffer.size() - m_position_offset;
		if (remaining > 0) {
			// Use a raw memory move for trivially-copyable element types (std::byte)
			std::memmove(m_buffer.data(), m_buffer.data() + m_position_offset, remaining);
			m_buffer.resize(remaining);
			// Shrink capacity only if massively over-allocated
			if (m_buffer.capacity() > remaining * 4 && m_buffer.capacity() > 4096) {
				m_buffer.shrink_to_fit();
			}
		} else {
			m_buffer.clear();
			// Only shrink when clearing if capacity was significant
			if (m_buffer.capacity() > 4096) {
				m_buffer.shrink_to_fit();
			}
		}
	}
	else {
		// Failsafe: if position offset is out of bounds, clear entire buffer
		m_buffer.clear();
	}
	m_position_offset = 0;
}

bool FIFO::Drop(const std::size_t& count) noexcept {
	if (FIFO::AvailableBytes() == 0 || count > FIFO::AvailableBytes())
		return false;
	
	// Call the non-virtual FIFO::Seek implementation to avoid
	// virtual dispatch into a derived class (e.g. SharedFIFO::Seek)
	// which may acquire external locks and cause re-entrant
	// locking deadlocks when Drop() is called while already
	// holding a wrapper mutex.
	FIFO::Seek(static_cast<std::ptrdiff_t>(count), Position::Relative);
	// Call non-virtual FIFO::Clean to avoid dispatching to
	// SharedFIFO::Clean which would attempt to lock the wrapper
	// mutex while Drop() may already be holding it.
	FIFO::Clean();
	return true;
}

void FIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	switch (mode) {
		case Position::Absolute:
			if (offset < 0) {
				// Negative absolute offset, clamp to 0
				m_position_offset = 0;
			}
			else {
				m_position_offset = std::min(static_cast<std::size_t>(offset), m_buffer.size());
			}
			break;
		case Position::Relative:
			if (offset < 0) {
				m_position_offset = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(m_position_offset) + offset));
			} else {
				m_position_offset = std::min(m_position_offset + static_cast<std::size_t>(offset), m_buffer.size());
			}
			break;
		default:
			// Invalid position mode, do nothing
			return;
	}
}

std::string FIFO::HexDump(const std::size_t& collumns, const std::size_t& byte_limit) const noexcept {
	const std::size_t cols = (collumns == 0) ? 16 : collumns;
	const std::size_t end = (byte_limit > 0) ? std::min(m_buffer.size(), m_position_offset + byte_limit) : m_buffer.size();

	std::ostringstream oss = HexDumpHeader();

	oss << '\n';

	if (end > m_position_offset) {
		std::span<const std::byte> view(m_buffer.data() + m_position_offset, end - m_position_offset);
		const std::string lines = FormatHexLines(view, m_position_offset, cols);
		oss << lines;
	}

	return oss.str();
}

std::string FIFO::FormatHexLines(std::span<const std::byte>& data, std::size_t start_offset, std::size_t collumns) noexcept {
	const std::size_t cols = (collumns == 0) ? 16 : collumns;
	const int offset_width = 8;

	std::vector<std::string> lines;
	for (std::size_t i = 0; i < data.size(); i += cols) {
		const std::size_t line_end = std::min(data.size(), i + cols);
		std::ostringstream line;

		line << std::hex << std::uppercase << std::setw(offset_width) << std::setfill('0') << (start_offset + i) << ": " << std::dec << std::setfill(' ');

		// hex bytes
		for (std::size_t j = i; j < i + cols; ++j) {
			if (j < line_end) {
				const unsigned int val = static_cast<unsigned int>(std::to_integer<unsigned char>(data[j]));
				line << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << val << ' ' << std::dec;
			} else {
				line << "   ";
			}
		}

		line << "  ";

		// ASCII
		for (std::size_t j = i; j < line_end; ++j) {
			const unsigned char c = std::to_integer<unsigned char>(data[j]);
			if (std::isprint(c)) line << static_cast<char>(c);
			else line << '.';
		}

		lines.push_back(line.str());
	}

	std::ostringstream oss;
	for (size_t li = 0; li < lines.size(); ++li) {
		oss << lines[li];
		if (li + 1 < lines.size()) oss << '\n';
	}

	return oss.str();
}

std::ostringstream FIFO::HexDumpHeader() const noexcept {
	std::ostringstream oss;
	oss << "Size: " << m_buffer.size() << " bytes\n";
	oss << "Read Position: " << m_position_offset << '\n';
	return oss;
}

bool FIFO::ReadInternal(const std::size_t& count, DataType& outBuffer, const Operation& flag) noexcept {
	const std::size_t available_bytes = FIFO::AvailableBytes();
	const std::size_t real_count = count == 0 ? available_bytes : count;
	// If buffer is empty and count==0, return immediately with error
	if ((available_bytes == 0 && count == 0) || real_count > available_bytes)
		return false;

	outBuffer.reserve(outBuffer.size() + real_count);

	const auto start_it = m_buffer.begin() + m_position_offset;
	switch(flag) {
		case Operation::Read: {
			outBuffer.insert(outBuffer.end(), start_it, start_it + real_count);
			m_position_offset += real_count;
			break;
		}
		case Operation::Peek: {
			outBuffer.insert(outBuffer.end(), start_it, start_it + real_count);
			break;
		}
		case Operation::Extract: {
			// Move bytes out to avoid extra copy, then drop from buffer
			outBuffer.insert(outBuffer.end(), std::make_move_iterator(start_it), std::make_move_iterator(start_it + real_count));
			m_buffer.erase(start_it, start_it + real_count);
			// Ensure read position remains valid after destructive erase
			if (m_position_offset > m_buffer.size()) {
				m_position_offset = m_buffer.size();
			}
			break;
		}
		default:
			return false;
	}

	return true;
}

bool FIFO::ReadInternal(const std::size_t& count, WriteOnly& outBuffer, const Operation& flag) noexcept {
	const std::size_t available_bytes = FIFO::AvailableBytes();
	const std::size_t real_count = count == 0 ? available_bytes : count;
	if ((count == 0 && available_bytes == 0) || real_count > available_bytes)
		return false;

	DataType temp;
	temp.reserve(real_count);
	switch(flag) {
		case Operation::Extract: {
			auto res = FIFO::ReadInternal(count, temp, Operation::Extract);
			if (!res)
				return false;
			break;
		}
		case Operation::Read: {
			auto res = FIFO::ReadInternal(count, temp, Operation::Read);
			if (!res)
				return false;
			break;
		}
		case Operation::Peek: {
			auto res = FIFO::ReadInternal(count, temp, Operation::Peek);
			if (!res)
				return false;
			break;
		}
		default:
			return false;
	}

	auto res = outBuffer.Write(std::move(temp));
	if (!res) {
		return false;
	}
	return true;
}

void FIFO::ReadUntilEoFInternal(DataType& outBuffer, const Operation& flag) noexcept {
	while (true) {
		switch(flag) {
			case Operation::Read: {
				(void)Read(0, outBuffer);
				break;
			}
			case Operation::Extract: {
				(void)Extract(0, outBuffer);
				break;
			}
			default:
				return;
		}
		DataType unused;
		if (!Peek(1, unused)) {
			return;
		}
	}
}

void FIFO::ReadUntilEoFInternal(WriteOnly& outBuffer, const Operation& flag) noexcept {
	while (true) {
		switch(flag) {
			case Operation::Read: {
				(void)Read(0, outBuffer);
				break;
			}
			case Operation::Extract: {
				(void)Extract(0, outBuffer);
				break;
			}
			default:
				return;
		}
		DataType unused;
		if (!Peek(1, unused)) {
			return;
		}
	}
}

bool FIFO::WriteInternal(const std::size_t& count, const DataType& src) noexcept {
	if (count > 0 && src.size() < count)
		return false;

	const std::size_t real_count = (count == 0) ? src.size() : count;

	// Reserve target space to avoid repeated reallocations
	m_buffer.reserve(m_buffer.size() + real_count);

	if (real_count == src.size()) {
		// Copy entire source
		append_vector(m_buffer, src);
	}
	else {
		m_buffer.insert(m_buffer.end(), src.begin(), src.begin() + real_count);
	}

	return true;
}

bool FIFO::WriteInternal(const std::size_t& count, DataType&& src) noexcept {
	if (count > 0 && src.size() < count)
		return false;

	const std::size_t real_count = (count == 0) ? src.size() : count;

	// Reserve target space to avoid repeated reallocations
	m_buffer.reserve(m_buffer.size() + real_count);

	if (real_count == src.size()) {
		// Move entire source
		append_vector(m_buffer, std::move(src));
	}
	else {
		m_buffer.insert(m_buffer.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.begin() + real_count));
		// Rearrange src to remove moved elements
		src.erase(src.begin(), src.begin() + real_count);
	}

	return true;
}

bool FIFO::WriteInternal(const std::size_t& count, const ReadOnly& src) noexcept {
	return src.Read(count, m_buffer);
}

bool FIFO::WriteInternal(const std::size_t& count, ReadOnly&& src) noexcept {
	return src.Extract(count, m_buffer);
}