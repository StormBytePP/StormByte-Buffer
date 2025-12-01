#include <StormByte/buffer/fifo.hxx>
#include <StormByte/string.hxx>

#include <algorithm>
#include <iterator>

using namespace StormByte::Buffer;

FIFO::FIFO() noexcept: m_buffer(), m_position_offset(0) {}

FIFO::FIFO(const FIFO& other) noexcept: m_buffer(), m_position_offset(0) {
	Copy(other);
}

FIFO::FIFO(FIFO&& other) noexcept: m_buffer(std::move(other.m_buffer)),
m_position_offset(other.m_position_offset) {
	other.m_position_offset = 0;
}

FIFO::~FIFO() {
	Clear();
}

FIFO& FIFO::operator=(const FIFO& other) {
	if (this != &other) {
		Clear();
		Copy(other);
	}
	return *this;
}

FIFO& FIFO::operator=(FIFO&& other) noexcept {
	if (this != &other) {
		Clear();
		m_buffer = std::move(other.m_buffer);
		m_position_offset = other.m_position_offset;
		other.m_position_offset = 0;
	}
	return *this;
}

std::size_t FIFO::AvailableBytes() const noexcept {
	const std::size_t current_size = m_buffer.size();
	return (m_position_offset <= current_size) ? (current_size - m_position_offset) : 0;
}

std::size_t FIFO::Size() const noexcept {
	return m_buffer.size();
}

bool FIFO::Empty() const noexcept {
	return m_buffer.empty();
}

void FIFO::Clear() noexcept {
	m_buffer.clear();
	m_position_offset = 0;
}

void FIFO::Clean() noexcept {
	if (m_position_offset > 0 && m_position_offset <= m_buffer.size()) {
		m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_position_offset);
	}
	else {
		// Failsafe: if position offset is out of bounds, clear entire buffer
		m_buffer.clear();
	}
	m_position_offset = 0;
}

bool FIFO::EoF() const noexcept {
	// FIFO core does not track closed/error; EOF semantics are provided by
	// SharedFIFO. For the raw FIFO, there's no EOF marker to query.
	return false;
}

bool FIFO::Write(const std::vector<std::byte>& data) {
	m_buffer.insert(m_buffer.end(), data.begin(), data.end());
	return true;
}

bool FIFO::Write(const FIFO& other) {
	// Append the entire contents of the other FIFO (including bytes
	// before/at its read position). This preserves the full buffer
	// contents rather than only the unread portion.
	if (other.m_buffer.empty()) {
		return true; // nothing to append
	}

	m_buffer.insert(m_buffer.end(), other.m_buffer.begin(), other.m_buffer.end());
	return true;
}

bool FIFO::Write(FIFO&& other) noexcept {
	// Append the entire contents of the rvalue FIFO. If the destination is
	// empty we can steal the deque via move (O(1)). Otherwise move-insert
	// the elements and leave `other` empty.
	if (other.m_buffer.empty()) {
		other.m_position_offset = 0;
		return true;
	}

	if (m_buffer.empty()) {
		m_buffer = std::move(other.m_buffer);
		other.m_position_offset = 0;
		return true;
	}

	// General case: move-append the whole deque
	m_buffer.insert(m_buffer.end(), std::make_move_iterator(other.m_buffer.begin()), std::make_move_iterator(other.m_buffer.end()));
	other.m_buffer.clear();
	other.m_position_offset = 0;
	return true;
}

bool FIFO::Write(const std::string& data) {
	return Write(StormByte::String::ToByteVector(data));
}

ExpectedData<InsufficientData> FIFO::Read(std::size_t count) const {
	const std::size_t available = AvailableBytes();

	// Semantics for FIFO core:
	// - count == 0: read all available; if none available => error
	// - count > 0: if none available => error; if count > available => error
	std::size_t read_size = 0;
	if (count == 0) {
		if (available == 0) {
			return StormByte::Unexpected(InsufficientData("Insufficient data to read"));
		}
		read_size = available;
	} else {
		if (available == 0) {
			return StormByte::Unexpected(InsufficientData("Insufficient data to read"));
		}
		if (count > available) {
			return StormByte::Unexpected(InsufficientData("Insufficient data to read"));
		}
		read_size = count;
	}

	// Read from current position using iterator constructor for efficiency
	auto start_it = m_buffer.begin() + m_position_offset;
	auto end_it = start_it + read_size;
	std::vector<std::byte> result(start_it, end_it);
	
	// Advance read position
	m_position_offset += read_size;
	
	return result;
}

ExpectedData<InsufficientData> FIFO::Extract(std::size_t count) {
	const std::size_t buffer_size = m_buffer.size();

	std::size_t extract_size = 0;
	if (count > AvailableBytes()) {
		return StormByte::Unexpected(InsufficientData("Insufficient data to extract"));
	}
	if (count == 0) {
		if (buffer_size == 0) {
			return StormByte::Unexpected(InsufficientData("Insufficient data to extract"));
		}
		extract_size = buffer_size;
	} else {
		if (buffer_size == 0) {
			return StormByte::Unexpected(InsufficientData("Insufficient data to extract"));
		}
		extract_size = count;
	}

	// Fast-path: extracting the whole deque and we're already at position 0.
	// Copy into result and clear the deque — this avoids the deque front-range
	// erase which can be more expensive than a full clear of its segments.
	if (m_position_offset == 0 && extract_size == m_buffer.size()) {
		std::vector<std::byte> result;
		result.reserve(extract_size);
		result.insert(result.end(), m_buffer.begin(), m_buffer.end()); // unavoidable copy
		m_buffer.clear();
		m_position_offset = 0;
		return result;
	}

	// Extract from beginning using iterator constructor for efficiency
	std::vector<std::byte> result(m_buffer.begin(), m_buffer.begin() + extract_size);

	// Delete extracted bytes from the front of the deque
	m_buffer.erase(m_buffer.begin(), m_buffer.begin() + extract_size);

	// Adjust the read position: if it was ahead of what we extracted, move it back
	m_position_offset = (m_position_offset > extract_size) ? (m_position_offset - extract_size) : 0;

	return result;
}

void FIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	std::ptrdiff_t new_offset;
	
	if (mode == Position::Absolute) {
		new_offset = offset;
	} else { // Position::Relative
		new_offset = static_cast<std::ptrdiff_t>(m_position_offset) + offset;
	}
	
	// Clamp to valid range [0, buffer.size()]
	if (new_offset < 0) {
		m_position_offset = 0;
	} else if (static_cast<std::size_t>(new_offset) > m_buffer.size()) {
		m_position_offset = m_buffer.size();
	} else {
		m_position_offset = static_cast<std::size_t>(new_offset);
	}
}

void FIFO::Copy(const FIFO& other) noexcept {
	m_buffer = other.m_buffer;
	m_position_offset = other.m_position_offset;
}