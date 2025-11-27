#include <StormByte/buffer/fifo.hxx>

#include <algorithm>

using namespace StormByte::Buffer;

FIFO::FIFO(std::size_t capacity) noexcept: m_buffer(capacity), m_initialCapacity(capacity), m_head(0), m_tail(0), m_size(0) {}

FIFO::FIFO(const FIFO& other) noexcept: m_buffer(other.m_buffer), m_initialCapacity(other.m_initialCapacity), m_head(other.m_head), m_tail(other.m_tail) {
	m_size.store(other.m_size);
	m_read_position.store(other.m_read_position.load());
}

FIFO::FIFO(FIFO&& other) noexcept: m_buffer(std::move(other.m_buffer)), m_initialCapacity(other.m_initialCapacity), m_head(other.m_head), m_tail(other.m_tail) {
	m_size.store(other.m_size);
	m_read_position.store(other.m_read_position.load());
	other.m_head = other.m_tail = other.m_size = 0;
	other.m_read_position.store(0);
    other.m_initialCapacity = 0;
}

FIFO& FIFO::operator=(const FIFO& other) noexcept {
	if (this != &other) {
		m_buffer = other.m_buffer;
		m_initialCapacity = other.m_initialCapacity;
		m_head = other.m_head;
		m_tail = other.m_tail;
		m_size.store(other.m_size);
		m_read_position.store(other.m_read_position.load());
	}
	return *this;
}

FIFO& FIFO::operator=(FIFO&& other) noexcept {
	if (this != &other) {
		m_buffer = std::move(other.m_buffer);
		m_initialCapacity = other.m_initialCapacity;
		m_head = other.m_head;
		m_tail = other.m_tail;
		m_size.store(other.m_size);
		m_read_position.store(other.m_read_position.load());
		other.m_head = other.m_tail = other.m_size = 0;
		other.m_read_position.store(0);
		other.m_initialCapacity = 0;
	}
	return *this;
}

std::size_t FIFO::Capacity() const noexcept {
	return m_buffer.size();
}

void FIFO::Clear() noexcept {
	// Erase logical contents and ensure reserved capacity equals initial request
	m_buffer.clear();
	if (m_initialCapacity > 0) {
		m_buffer.resize(m_initialCapacity);
	}
	m_head = m_tail = m_size = 0;
	m_read_position.store(0);
}

void FIFO::Reserve(std::size_t newCapacity) {
	if (newCapacity <= m_buffer.size()) return;
	std::vector<std::byte> dst(newCapacity);
	RelinearizeInto(dst);
	m_buffer.swap(dst);
	m_head = 0;
	m_tail = m_size;
}

bool FIFO::Write(const std::vector<std::byte>& data) {
	if (m_closed.load()) return false;
	std::size_t count = data.size();
	if (count == 0) return false;
	GrowToFit(m_size + count);
	if (m_buffer.empty()) {
		// Ensure we have capacity after growth attempt
		Reserve(count);
	}
	const std::size_t cap = m_buffer.size();
	const std::size_t first = std::min(count, cap - m_tail);
	std::copy_n(data.data(), first, m_buffer.begin() + static_cast<std::ptrdiff_t>(m_tail));
	const std::size_t second = count - first;
	if (second) {
		std::copy_n(data.data() + static_cast<std::ptrdiff_t>(first), second, m_buffer.begin());
	}
	m_tail = (m_tail + count) % cap;
	m_size += count;
	return true;
}

bool FIFO::Write(const std::string& data) {
	if (m_closed.load()) return false;
	if (data.empty()) return false;
	std::vector<std::byte> tmp;
	tmp.resize(data.size());
	std::copy_n(reinterpret_cast<const std::byte*>(data.data()), data.size(), tmp.begin());
	return Write(tmp);
}

std::vector<std::byte> FIFO::Read(std::size_t count) {
	const std::size_t current_size = m_size.load();
	const std::size_t read_pos = m_read_position.load();
	
	// Calculate available data from read position
	const std::size_t available = (read_pos <= current_size) ? (current_size - read_pos) : 0;
	const std::size_t toRead = count > 0 ? std::min(count, available) : available;
	
	std::vector<std::byte> out;
	if (toRead == 0) return out;
	
	out.resize(toRead);
	const std::size_t cap = m_buffer.size();
	
	// Calculate actual position in circular buffer
	const std::size_t actual_pos = (m_head + read_pos) % cap;
	
	const std::size_t first = std::min(toRead, cap - actual_pos);
	std::copy_n(m_buffer.begin() + static_cast<std::ptrdiff_t>(actual_pos), first, out.begin());
	const std::size_t second = toRead - first;
	if (second) {
		std::copy_n(m_buffer.begin(), second, out.begin() + static_cast<std::ptrdiff_t>(first));
	}
	
	// Update read position atomically
	m_read_position.fetch_add(toRead);
	
	return out;
}

std::vector<std::byte> FIFO::Extract(std::size_t count) {
	const std::size_t current_size = m_size.load();
	const std::size_t toRead = count > 0 ? std::min(count, current_size) : current_size;
	std::vector<std::byte> out;
	if (toRead == 0) return out;
	// Zero-copy fast path: entire content contiguous from head == 0
	if (toRead == m_size && m_head == 0) {
		m_buffer.resize(m_size);
		out = std::move(m_buffer);
		m_head = m_tail = m_size = 0;
		m_read_position.store(0);
		return out;
	}
	out.resize(toRead);
	const std::size_t cap = m_buffer.size();
	const std::size_t first = std::min(toRead, cap - m_head);
	std::copy_n(m_buffer.begin() + static_cast<std::ptrdiff_t>(m_head), first, out.begin());
	const std::size_t second = toRead - first;
	if (second) {
		std::copy_n(m_buffer.begin(), second, out.begin() + static_cast<std::ptrdiff_t>(first));
	}
	m_head = (m_head + toRead) % cap;
	m_size -= toRead;
	// Adjust read position if it points beyond extracted data
	const std::size_t current_read_pos = m_read_position.load();
	if (current_read_pos >= toRead) {
		m_read_position.fetch_sub(toRead);
	} else {
		m_read_position.store(0);
	}
	return out;
}

void FIFO::GrowToFit(std::size_t required) {
	if (required <= m_buffer.size()) return;
	std::size_t newCap = m_buffer.empty() ? 64 : m_buffer.size();
	while (newCap < required) newCap *= 2;
	Reserve(newCap);
}

void FIFO::Seek(const std::size_t& position, const Position& mode) {
	const std::size_t current_size = m_size.load();
	
	if (mode == Position::Absolute) {
		// Set read position to absolute offset from head, clamped to [0, size]
		m_read_position.store(std::min(position, current_size));
	} else { // Position::Relative
		const std::size_t current_pos = m_read_position.load();
		// Calculate new position, clamping to valid range
		const std::size_t new_pos = current_pos + position;
		m_read_position.store(std::min(new_pos, current_size));
	}
}

void FIFO::RelinearizeInto(std::vector<std::byte>& dst) const {
	if (m_size == 0) return;
	const std::size_t cap = m_buffer.size();
	const std::size_t first = std::min(m_size.load(), cap - m_head);
	std::copy_n(m_buffer.begin() + static_cast<std::ptrdiff_t>(m_head), first, dst.begin());
	const std::size_t second = m_size - first;
	if (second) {
		std::copy_n(m_buffer.begin(), second, dst.begin() + static_cast<std::ptrdiff_t>(first));
	}
}
