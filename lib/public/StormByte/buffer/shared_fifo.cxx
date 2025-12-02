#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>

#include <sstream>
#include <iomanip>
#include <cctype>

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

ExpectedData<ReadError> SharedFIFO::Read(std::size_t count) const {
	std::unique_lock<std::mutex> lock(m_mutex);
	const std::size_t available = AvailableBytes();

	if (m_error)
		return StormByte::Unexpected(ReadError("Buffer in error state"));

	if (m_closed && available == 0) {
		// If closed and no data available, return EOF indication (empty read)
		return StormByte::Unexpected(ReadError("End of file reached"));
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

ExpectedData<ReadError> SharedFIFO::Extract(std::size_t count) {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_error)
		return StormByte::Unexpected(ReadError("Buffer in error state"));

	std::size_t real_count = count == 0 ? AvailableBytes() : count;

	if (!m_closed && count > AvailableBytes()) {
		// When not closed, requesting more than available must wait.
		// Wait() will block until enough data is written or buffer is closed/error.
		Wait(real_count, lock);
	}
	// If closed it acts like FIFO: requesting more than available is an error.
	return FIFO::Extract(real_count);
}

ExpectedVoid<WriteError> SharedFIFO::Write(const std::vector<std::byte>& data) {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		// Reject writes when closed or in error state.
		if (m_closed || m_error) return StormByte::Unexpected(WriteError("Buffer is closed or in error state"));
		if (!data.empty())
			m_buffer.insert(m_buffer.end(), data.begin(), data.end());
	}
	// Notify waiters even for empty writes so readers can re-check predicates.
	m_cv.notify_all();
	return {};
}

ExpectedVoid<WriteError> SharedFIFO::Write(const std::string& data) {
	return Write(StormByte::String::ToByteVector(data));
}

ExpectedVoid<WriteError> SharedFIFO::Write(const FIFO& other) {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		// Reject writes when closed or in error state.
		if (m_closed || m_error) return StormByte::Unexpected(WriteError("Buffer is closed or in error state"));
		// Delegate to base FIFO implementation to append the full contents.
		(void)FIFO::Write(other);
	}
	// Notify waiters after performing the write.
	m_cv.notify_all();
	return {};
}

ExpectedVoid<WriteError> SharedFIFO::Write(FIFO&& other) noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		// Reject writes when closed or in error state.
		if (m_closed || m_error) return StormByte::Unexpected(WriteError("Buffer is closed or in error state"));
		// Delegate to base FIFO rvalue overload; it will perform efficient
		// move/steal semantics when possible and leave `other` in a valid
		// empty state.
		(void)FIFO::Write(std::move(other));
	}
	// Notify waiters after performing the write.
	m_cv.notify_all();
	return {};
}

void SharedFIFO::Clear() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		FIFO::Clear();
	}
	m_cv.notify_all();
}

void SharedFIFO::Clean() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		FIFO::Clean();
	}
	m_cv.notify_all();
}

void SharedFIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		FIFO::Seek(offset, mode);
	}
	m_cv.notify_all();
}

ExpectedData<ReadError> SharedFIFO::Peek(std::size_t count) const noexcept {
	// Peek will wait for data in case it is not enough available, similar to Read().
	std::unique_lock<std::mutex> lock(m_mutex);
	const std::size_t available = AvailableBytes();
	if (m_error)
		return StormByte::Unexpected(ReadError("Buffer in error state"));
	if (m_closed && available == 0) {
		// If closed and no data available, return EOF indication (empty peek)
		return StormByte::Unexpected(ReadError("End of file reached"));
	}
	
	std::size_t real_count = count == 0 ? available : count;

	if (!m_closed && real_count > available) {
		// When not closed, requesting more than available must wait.
		// Wait() will block until enough data is written or buffer is closed/error.
		Wait(real_count, lock);
	}
	
	// If closed it acts like FIFO: requesting more than available is an error.
	return FIFO::Peek(real_count);
}

void SharedFIFO::Skip(const std::size_t& count) noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);

		// Replicate FIFO::Skip logic but call the non-virtual FIFO::Clean()
		// to avoid invoking the overridden SharedFIFO::Clean() while holding
		// the mutex (which would deadlock trying to re-lock the same mutex).
		if (count == 0) {
			// noop
		} else {
			if (m_position_offset + count >= m_buffer.size())
				m_position_offset = m_buffer.size();
			else
				m_position_offset += count;

			// call base class Clean directly (non-virtual) while still holding lock
			FIFO::Clean();
		}
	}
	m_cv.notify_all();
}

bool SharedFIFO::EoF() const noexcept {
	std::scoped_lock lock(m_mutex);
	return m_error || (m_closed && AvailableBytes() == 0);
}

std::string SharedFIFO::HexDump(const std::size_t& collumns, const std::size_t& byte_limit) const noexcept {
	bool fifo_closed, fifo_status;
	std::vector<std::byte> snapshot;
	std::size_t start_offset = 0;
	{
		// Acquire read lock to produce a consistent snapshot
		std::unique_lock<std::mutex> lock(m_mutex);
		fifo_closed = m_closed;
		fifo_status = m_error;

		// Determine available bytes from current read position and apply byte_limit
		const std::size_t available = AvailableBytes();
		const std::size_t to_copy = (byte_limit > 0) ? std::min(available, byte_limit) : available;

		if (to_copy > 0) {
			start_offset = m_position_offset;
			auto start_it = m_buffer.begin() + start_offset;
			auto end_it = start_it + to_copy;
			snapshot.assign(start_it, end_it);
		}
	}
	// Build status line
	std::string status = "Status: ";
	status += (fifo_closed ? "closed" : "opened");
	status += ", ";
	status += (fifo_status ? "error" : "ready");

	// If nothing to dump, return just the status
	if (snapshot.empty()) return status;

	// Format the hex dump from the snapshot using the shared FIFO helper.
	const std::size_t cols = (collumns == 0) ? 16 : collumns;
	const std::string lines = FIFO::FormatHexLines(snapshot, start_offset, cols);

	if (lines.empty()) return status; // defensive: shouldn't happen since we checked snapshot

	std::ostringstream oss;
	oss << status << '\n';
	oss << "Read Position: " << start_offset << '\n';
	oss << lines;

	return oss.str();
}
