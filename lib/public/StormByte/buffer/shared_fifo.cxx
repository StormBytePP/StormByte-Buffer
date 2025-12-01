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
	const std::size_t available = AvailableBytes();

	if (m_error)
		return StormByte::Unexpected(InsufficientData("Buffer in error state"));

	if (m_closed && available == 0) {
		// If closed and no data available, return EOF indication (empty read)
		return StormByte::Unexpected(InsufficientData("End of file reached"));
	}

	std::size_t real_count = count == 0 ? available : count;

	if (!m_closed && real_count > available) {
		// When not closed, requesting more than available must wait.
		// Wait() will block until enough data is written or buffer is closed/error.
		Wait(real_count, lock);
	}
	
	// If closed it acts like FIFO: requesting more than available is an error.
	return FIFO::Read(real_count);
}

ExpectedData<InsufficientData> SharedFIFO::Extract(std::size_t count) {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_error)
		return StormByte::Unexpected(InsufficientData("Buffer in error state"));

	std::size_t real_count = count == 0 ? AvailableBytes() : count;

	if (!m_closed && count > AvailableBytes()) {
		// When not closed, requesting more than available must wait.
		// Wait() will block until enough data is written or buffer is closed/error.
		Wait(real_count, lock);
	}
	// If closed it acts like FIFO: requesting more than available is an error.
	return FIFO::Extract(real_count);
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
