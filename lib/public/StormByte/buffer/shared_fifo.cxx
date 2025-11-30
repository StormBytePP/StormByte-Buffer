#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>

using namespace StormByte::Buffer;

void SharedFIFO::Close() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		m_closed = true;
	}
	m_cv.notify_all();
}

void SharedFIFO::SetError() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		m_error = true;
	}
	m_cv.notify_all();
}

void SharedFIFO::Wait(std::size_t n, std::unique_lock<std::mutex>& lock) const {
	if (n == 0) return;
	m_cv.wait(lock, [&] {
		// Wake when closed or error state set so waiters don't remain blocked
		// indefinitely when the FIFO is no longer usable.
		if (m_closed) return true;
		if (m_error) return true;
		const std::size_t sz = m_buffer.size();
		const std::size_t rp = m_position_offset;
		return sz >= rp + n; // at least n bytes available from current read position
	});
}

ExpectedData<InsufficientData> SharedFIFO::Read(std::size_t count) const {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (count != 0) {
		Wait(count, lock);
		// If closed and insufficient data, read whatever is available (may be empty)
		if (m_closed) {
			const std::size_t available = m_buffer.size() - m_position_offset;
			if (available < count) {
				return FIFO::Read(0); // Read all available (returns empty vector if none)
			}
		}
	}
	return FIFO::Read(count);
}

ExpectedData<InsufficientData> SharedFIFO::Extract(std::size_t count) {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (count != 0) {
		Wait(count, lock);
		// If closed and insufficient data, extract whatever is available (may be empty)
		if (m_closed && m_buffer.size() < count) {
			return FIFO::Extract(0); // Extract all available (returns empty vector if none)
		}
	}
	return FIFO::Extract(count);
}

bool SharedFIFO::Write(const std::vector<std::byte>& data) {
	if (data.empty()) return false;
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		if (m_closed) return false;
		m_buffer.insert(m_buffer.end(), data.begin(), data.end());
	}
	m_cv.notify_all();
	return true;
}

bool SharedFIFO::Write(const std::string& data) {
	return Write(StormByte::String::ToByteVector(data));
}

void SharedFIFO::Clear() noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	FIFO::Clear();
}

void SharedFIFO::Clean() noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	FIFO::Clean();
}

void SharedFIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		FIFO::Seek(offset, mode);
	}
	m_cv.notify_all();
}
