#include <StormByte/buffer/fifo.hxx>
#include <StormByte/string.hxx>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <cctype>

using namespace StormByte::Buffer;

FIFO::FIFO() noexcept: m_buffer(), m_position_offset(0) {}

FIFO::FIFO(const std::vector<std::byte>& data) noexcept: m_position_offset(0) {
	m_buffer.insert(m_buffer.end(), data.begin(), data.end());
}

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

ExpectedData<ReadError> FIFO::Read(std::size_t count) const {
	const std::size_t available = AvailableBytes();

	if (available == 0) {
		return StormByte::Unexpected(ReadError("Insufficient data to read"));
	}

	std::size_t real_count = count == 0 ? available : count;
	if (real_count > available) {
		return StormByte::Unexpected(ReadError("Insufficient data to read"));
	}

	// Read from current position using iterator constructor for efficiency
	auto start_it = m_buffer.begin() + m_position_offset;
	auto end_it = start_it + real_count;
	std::vector<std::byte> result(start_it, end_it);
	
	// Advance read position
	m_position_offset += real_count;
	
	return result;
}

ExpectedData<ReadError> FIFO::Extract(std::size_t count) {
	const std::size_t available = AvailableBytes();

	if (available == 0) {
		return StormByte::Unexpected(ReadError("Insufficient data to read"));
	}

	std::size_t real_count = count == 0 ? available : count;
	if (real_count > available) {
		return StormByte::Unexpected(ReadError("Insufficient data to read"));
	}

	// Fast-path: extracting the whole deque and we're already at position 0.
	// Copy into result and clear the deque — this avoids the deque front-range
	// erase which can be more expensive than a full clear of its segments.
	if (m_position_offset == 0 && real_count == m_buffer.size()) {
		std::vector<std::byte> result;
		result.reserve(real_count);
		result.insert(result.end(), m_buffer.begin(), m_buffer.end()); // unavoidable copy
		m_buffer.clear();
		m_position_offset = 0;
		return result;
	}

	// Extract from beginning using iterator constructor for efficiency
	std::vector<std::byte> result(m_buffer.begin(), m_buffer.begin() + real_count);

	// Delete extracted bytes from the front of the deque
	m_buffer.erase(m_buffer.begin(), m_buffer.begin() + real_count);

	// Adjust the read position: if it was ahead of what we extracted, move it back
	m_position_offset = (m_position_offset > real_count) ? (m_position_offset - real_count) : 0;

	return result;
}

ExpectedVoid<WriteError> FIFO::Write(const std::vector<std::byte>& data) {
	m_buffer.insert(m_buffer.end(), data.begin(), data.end());
	return {};
}

ExpectedVoid<WriteError> FIFO::Write(const FIFO& other) {
	// Append the entire contents of the other FIFO (including bytes
	// before/at its read position). This preserves the full buffer
	// contents rather than only the unread portion.
	if (other.m_buffer.empty()) {
		return {}; // nothing to append
	}

	m_buffer.insert(m_buffer.end(), other.m_buffer.begin(), other.m_buffer.end());
	return {};
}

ExpectedVoid<WriteError> FIFO::Write(FIFO&& other) noexcept {
	// Append the entire contents of the rvalue FIFO. If the destination is
	// empty we can steal the deque via move (O(1)). Otherwise move-insert
	// the elements and leave `other` empty.
	if (other.m_buffer.empty()) {
		other.m_position_offset = 0;
		return {};
	}

	if (m_buffer.empty()) {
		// Preserve the source read position when stealing its storage.
		m_position_offset = other.m_position_offset;
		m_buffer = std::move(other.m_buffer);
		other.m_position_offset = 0;
		return {};
	}

	// General case: move-append the whole deque
	m_buffer.insert(m_buffer.end(), std::make_move_iterator(other.m_buffer.begin()), std::make_move_iterator(other.m_buffer.end()));
	other.m_buffer.clear();
	other.m_position_offset = 0;
	return {};
}

ExpectedVoid<WriteError> FIFO::Write(const std::string& data) {
	return Write(StormByte::String::ToByteVector(data));
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

void FIFO::Skip(const std::size_t& count) noexcept {
	// Advance read position by count, clamped to buffer size
	if (count == 0) {
		return; // noop
	}

	if (m_position_offset + count >= m_buffer.size())
		m_position_offset = m_buffer.size();
	else
		m_position_offset += count;

	Clean();
}

std::string FIFO::HexDump(const std::size_t& collumns, const std::size_t& byte_limit) const noexcept {
	// Build a snapshot of the unread bytes (respecting byte_limit) and delegate
	// the per-line formatting to the shared helper so formatting logic is
	// centralized and consistent with SharedFIFO.
	const std::size_t cols = (collumns == 0) ? 16 : collumns;
	const std::size_t start = m_position_offset;
	const std::size_t end = (byte_limit > 0) ? std::min(m_buffer.size(), start + byte_limit) : m_buffer.size();

	std::vector<std::byte> snapshot;
	if (end > start) {
		snapshot.assign(m_buffer.begin() + start, m_buffer.begin() + end);
	}

	std::ostringstream oss;
	oss << "Read Position: " << m_position_offset;

	if (!snapshot.empty()) {
		oss << '\n';
		const std::string lines = FIFO::FormatHexLines(snapshot, start, cols);
		oss << lines;
	}

	return oss.str();
}

std::string FIFO::FormatHexLines(const std::vector<std::byte>& data, std::size_t start_offset, std::size_t collumns) noexcept {
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

void FIFO::Copy(const FIFO& other) noexcept {
	m_buffer = other.m_buffer;
	m_position_offset = other.m_position_offset;
}