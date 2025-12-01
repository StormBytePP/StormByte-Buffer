#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>

using namespace StormByte::Buffer;

SharedFIFO::SharedFIFO() noexcept: FIFO(), m_closed(false), m_error(false) {}

bool SharedFIFO::operator==(const SharedFIFO& other) const noexcept {
	// Lock both mutexes to safely compare internal state and delegate to
	// FIFO::operator== so behavior remains coherent if FIFO's equality
	// semantics change in the future.
	std::scoped_lock lock1(m_mutex, other.m_mutex);
	return static_cast<const FIFO&>(*this) == static_cast<const FIFO&>(other);
}

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
		if (m_error) {
			return StormByte::Unexpected(InsufficientData("Buffer in error state"));
		}
		if (m_closed) {
			const std::size_t available = m_buffer.size() - m_position_offset;
			// When closed, requesting more than available is an error.
			if (available < count) {
				return StormByte::Unexpected(InsufficientData("Insufficient data to read (closed)"));
			}
		}
	} else {
		// count == 0: non-blocking read of all available — still fail if in error state
		if (m_error) {
			return StormByte::Unexpected(InsufficientData("Buffer in error state"));
		}
		// If closed and no bytes available, return an empty vector (value)
		if (m_closed) {
			const std::size_t available = m_buffer.size() - m_position_offset;
			if (available == 0) return std::vector<std::byte>{};
		}
	}

	return FIFO::Read(count);
}

ExpectedData<InsufficientData> SharedFIFO::Extract(std::size_t count) {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (count != 0) {
		if (m_error) {
			return StormByte::Unexpected(InsufficientData("Buffer in error state"));
		}
		Wait(count, lock);
		if (m_closed) {
			const std::size_t available = m_buffer.size() - m_position_offset;
			if (available < count) {
				return StormByte::Unexpected(InsufficientData("Insufficient data to extract (closed)"));
			}
		}
	} else {
		if (m_error) {
			return StormByte::Unexpected(InsufficientData("Buffer in error state"));
		}
		if (m_closed && m_buffer.empty()) {
			return std::vector<std::byte>{};
		}
	}

	return FIFO::Extract(count);
}

bool SharedFIFO::Write(const std::vector<std::byte>& data) {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		// Reject writes when closed or in error state.
		if (m_closed || m_error) return false;
		if (!data.empty())
			m_buffer.insert(m_buffer.end(), data.begin(), data.end());
	}
	// Notify waiters even for empty writes so readers can re-check predicates.
	m_cv.notify_all();
	return true;
}

bool SharedFIFO::Write(const std::string& data) {
	return Write(StormByte::String::ToByteVector(data));
}

bool SharedFIFO::Write(const FIFO& other) {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		// Reject writes when closed or in error state.
		if (m_closed || m_error) return false;
		// Delegate to base FIFO implementation to append the full contents.
		FIFO::Write(other);
	}
	// Notify waiters after performing the write.
	m_cv.notify_all();
	return true;
}

bool SharedFIFO::Write(FIFO&& other) noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		// Reject writes when closed or in error state.
		if (m_closed || m_error) return false;
		// Delegate to base FIFO rvalue overload; it will perform efficient
		// move/steal semantics when possible and leave `other` in a valid
		// empty state.
		FIFO::Write(std::move(other));
	}
	// Notify waiters after performing the write.
	m_cv.notify_all();
	return true;
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

bool SharedFIFO::EoF() const noexcept {
	std::scoped_lock lock(m_mutex);
	return m_error || (m_closed && AvailableBytes() == 0);
}
