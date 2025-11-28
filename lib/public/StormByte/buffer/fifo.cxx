#include <StormByte/buffer/fifo.hxx>
#include <StormByte/string.hxx>

#include <algorithm>

using namespace StormByte::Buffer;

FIFO::FIFO() noexcept: m_buffer(), m_position_offset(0), m_closed(false) {}

FIFO::FIFO(const FIFO& other) noexcept: m_buffer(), m_position_offset(0), m_closed(false) {
	Copy(other);
}

FIFO::FIFO(FIFO&& other) noexcept: m_buffer(std::move(other.m_buffer)),
m_position_offset(other.m_position_offset), m_closed(other.m_closed) {
	other.m_position_offset = 0;
	other.m_closed = true;
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
		m_closed = other.m_closed;
		other.m_position_offset = 0;
		other.m_closed = true;
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
		m_position_offset = 0;
	}
}

void FIFO::Close() noexcept {
	m_closed = true;
}

bool FIFO::Write(const std::vector<std::byte>& data) {
	if (IsClosed()) return false;
	if (!data.empty())
		m_buffer.insert(m_buffer.end(), data.begin(), data.end());
	return true;
}

bool FIFO::Write(const std::string& data) {
	return Write(StormByte::String::ToByteVector(data));
}

ExpectedData<InsufficientData> FIFO::Read(std::size_t count) const {
	const std::size_t available = AvailableBytes();
	
	// count=0 means "read all available"
	const std::size_t read_size = (count == 0) ? available : std::min(count, available);
	
	// If requesting specific count (count > 0) but no data available, return error
	if (count > 0 && available == 0) {
		return StormByte::Unexpected(InsufficientData("Insufficient data to read"));
	}
	
	// If FIFO is closed and requesting more than available, return error
	if (m_closed && count > available) {
		return StormByte::Unexpected(InsufficientData("Insufficient data in closed FIFO"));
	}
	
	// Empty read is success
	if (read_size == 0) {
		return std::vector<std::byte>();
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
	// Extract always reads from the beginning (head), not from current read position
	const std::size_t buffer_size = m_buffer.size();
	
	// count=0 means "extract all available"
	const std::size_t extract_size = (count == 0) ? buffer_size : std::min(count, buffer_size);
	
	// If requesting specific count (count > 0) but no data available, return error
	if (count > 0 && buffer_size == 0) {
		return StormByte::Unexpected(InsufficientData("Insufficient data to extract"));
	}
	
	// If FIFO is closed and requesting more than available, return error
	if (m_closed && count > buffer_size) {
		return StormByte::Unexpected(InsufficientData("Insufficient data in closed FIFO"));
	}
	
	// Empty extract is success
	if (extract_size == 0) {
		return std::vector<std::byte>();
	}

	// Extract from beginning using iterator constructor for efficiency
	std::vector<std::byte> result(m_buffer.begin(), m_buffer.begin() + extract_size);
	
	// Delete extracted bytes from the front of the deque
	m_buffer.erase(m_buffer.begin(), m_buffer.begin() + extract_size);
	
	// Adjust the read position: if it was ahead of what we extracted, move it back
	m_position_offset = (m_position_offset > extract_size) ? (m_position_offset - extract_size) : 0;
	
	return result;
}

bool FIFO::IsClosed() const noexcept {
	return m_closed;
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
	m_closed = other.m_closed;
}