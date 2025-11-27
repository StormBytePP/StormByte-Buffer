#include <StormByte/memory/buffer/fifo.hxx>

#include <algorithm>

using namespace StormByte::Memory::Buffer;

FIFO::FIFO(std::size_t capacity) noexcept: m_buffer(capacity), m_initialCapacity(capacity), m_head(0), m_tail(0), m_size(0) {}

FIFO::FIFO(const FIFO& other) noexcept: m_buffer(other.m_buffer), m_initialCapacity(other.m_initialCapacity), m_head(other.m_head), m_tail(other.m_tail) {
	m_size.store(other.m_size);
}

FIFO::FIFO(FIFO&& other) noexcept: m_buffer(std::move(other.m_buffer)), m_initialCapacity(other.m_initialCapacity), m_head(other.m_head), m_tail(other.m_tail) {
	m_size.store(other.m_size);
	other.m_head = other.m_tail = other.m_size = 0;
    other.m_initialCapacity = 0;
}

FIFO& FIFO::operator=(const FIFO& other) noexcept {
	if (this != &other) {
		m_buffer = other.m_buffer;
		m_initialCapacity = other.m_initialCapacity;
		m_head = other.m_head;
		m_tail = other.m_tail;
		m_size.store(other.m_size);
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
		other.m_head = other.m_tail = other.m_size = 0;
		other.m_initialCapacity = 0;
	}
	return *this;
}

void FIFO::Reserve(std::size_t newCapacity) {
	if (newCapacity <= m_buffer.size()) return;
	std::vector<std::byte> dst(newCapacity);
	RelinearizeInto(dst);
	m_buffer.swap(dst);
	m_head = 0;
	m_tail = m_size;
}

void FIFO::Write(const std::vector<std::byte>& data) {
	if (m_closed.load()) return;
	std::size_t count = data.size();
	if (count == 0) return;
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
}

void FIFO::Write(std::vector<std::byte>&& data) noexcept {
	if (m_closed.load()) return;
	const std::size_t count = data.size();
	if (count == 0) return;
	if (m_size == 0) {
		// Adopt storage wholesale for zero-copy when empty
		m_buffer.clear();
		m_buffer.shrink_to_fit();
		m_buffer = std::move(data);
		m_head = 0;
		m_size = count;
		m_tail = m_size % m_buffer.size();
		return;
	}
	// Otherwise, fall back to regular write (wrap-aware, at most two copies)
	Write(static_cast<const std::vector<std::byte>&>(data));
}

void FIFO::Write(const std::string& data) {
	if (m_closed.load()) return;
	if (data.empty()) return;
	std::vector<std::byte> tmp;
	tmp.resize(data.size());
	std::copy_n(reinterpret_cast<const std::byte*>(data.data()), data.size(), tmp.begin());
	Write(tmp);
}

std::vector<std::byte> FIFO::Read(std::size_t count) {
	const std::size_t current_size = m_size.load();
	const std::size_t toRead = count > 0 ? std::min(count, current_size) : current_size;
	std::vector<std::byte> out;
	if (toRead == 0) return out;
	// Zero-copy fast path: entire content contiguous from head == 0
	if (toRead == m_size && m_head == 0) {
		m_buffer.resize(m_size);
		out = std::move(m_buffer);
		m_head = m_tail = m_size = 0;
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
	return out;
}

void FIFO::GrowToFit(std::size_t required) {
	if (required <= m_buffer.size()) return;
	std::size_t newCap = m_buffer.empty() ? 64 : m_buffer.size();
	while (newCap < required) newCap *= 2;
	Reserve(newCap);
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

void FIFO::Clear() noexcept {
	// Erase logical contents and ensure reserved capacity equals initial request
	m_buffer.clear();
	if (m_initialCapacity > 0) {
		m_buffer.resize(m_initialCapacity);
	}
	m_head = m_tail = m_size = 0;
}
