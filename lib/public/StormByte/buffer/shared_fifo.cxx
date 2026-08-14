#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>

#include <sstream>
#include <iomanip>
#include <cctype>

using namespace StormByte::Buffer;

// ---------------------------------------------------------------------------
// Assignment
// ---------------------------------------------------------------------------

SharedFIFO& SharedFIFO::operator=(const FIFO& other) {
	std::unique_lock<std::mutex> lock(m_mutex);
	FIFO::operator=(other);          // also copies m_closed / m_error
	m_cv.notify_all();
	return *this;
}

SharedFIFO& SharedFIFO::operator=(FIFO&& other) noexcept {
	std::scoped_lock lock(m_mutex);
	FIFO::operator=(std::move(other));
	m_cv.notify_all();
	return *this;
}

SharedFIFO& SharedFIFO::operator=(SharedFIFO&&) noexcept {
	// Mutex / condition_variable are not movable.
	// Leave the object in a valid state; the moved-from object should not be used.
	return *this;
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

bool SharedFIFO::operator==(const SharedFIFO& other) const noexcept {
	std::scoped_lock lock(m_mutex, other.m_mutex);
	return static_cast<const FIFO&>(*this) == static_cast<const FIFO&>(other);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::size_t SharedFIFO::AvailableBytes() const noexcept {
	std::scoped_lock lock(m_mutex);
	return FIFO::AvailableBytes();
}

void SharedFIFO::Clean() noexcept {
	std::scoped_lock lock(m_mutex);
	FIFO::Clean();
}

void SharedFIFO::Clear() noexcept {
	{
		std::scoped_lock lock(m_mutex);
		FIFO::Clear();
	}
	m_cv.notify_all();
}

void SharedFIFO::Close() noexcept {
	{
		std::scoped_lock lock(m_mutex);
		FIFO::Close();               // sets base m_closed = true
	}
	m_cv.notify_all();
}

bool SharedFIFO::Drop(const std::size_t& count) noexcept {
	bool result;
	{
		std::unique_lock lock(m_mutex);
		if (count != 0 && count > FIFO::AvailableBytes())
			Wait(count, lock);
		result = FIFO::Drop(count);
	}
	m_cv.notify_all();
	return result;
}

bool SharedFIFO::Empty() const noexcept {
	std::scoped_lock lock(m_mutex);
	return FIFO::Empty();
}

bool SharedFIFO::EoF() const noexcept {
	std::scoped_lock lock(m_mutex);
	return FIFO::EoF();              // uses base m_closed / m_error
}

bool SharedFIFO::HasError() const noexcept {
	std::scoped_lock lock(m_mutex);
	return !FIFO::IsReadable();      // equivalent to base m_error
}

std::string SharedFIFO::HexDump(const std::size_t& columns,
								const std::size_t& byte_limit) const noexcept {
	std::scoped_lock lock(m_mutex);
	return FIFO::HexDump(columns, byte_limit);
}

void SharedFIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	std::scoped_lock lock(m_mutex);
	FIFO::Seek(offset, mode);
}

void SharedFIFO::SetError() noexcept {
	{
		std::scoped_lock lock(m_mutex);
		FIFO::SetError();            // sets base m_error = true
	}
	m_cv.notify_all();
}

std::size_t SharedFIFO::Size() const noexcept {
	std::scoped_lock lock(m_mutex);
	return FIFO::Size();
}

// ---------------------------------------------------------------------------
// Protected / private helpers
// ---------------------------------------------------------------------------

std::ostringstream SharedFIFO::HexDumpHeader() const noexcept {
	// Base already includes status (closed/error)
	return FIFO::HexDumpHeader();
}

bool SharedFIFO::ReadInternal(const std::size_t& count, DataType& outBuffer,
							const Operation& flag) noexcept {
	std::unique_lock lock(m_mutex);

	// Solo miembros / lógica base — NUNCA AvailableBytes()/EoF()/IsWritable() virtuales
	const std::size_t avail =
		(m_position_offset <= m_buffer.size())
			? (m_buffer.size() - m_position_offset)
			: 0;

	if (m_error || (m_closed && avail == 0))
		return false;

	const std::size_t real_count = (count == 0) ? avail : count;
	if (real_count > avail && !m_closed && !m_error) {
		Wait(real_count, lock);
		const std::size_t avail2 =
			(m_position_offset <= m_buffer.size())
				? (m_buffer.size() - m_position_offset)
				: 0;
		if (m_error || (m_closed && avail2 == 0))
			return false;
	}

	return FIFO::ReadInternal(count, outBuffer, flag);
}

bool SharedFIFO::ReadInternal(const std::size_t& count, WriteOnly& outBuffer,
							const Operation& flag) noexcept {
	std::unique_lock lock(m_mutex);

	std::size_t avail = FIFO::AvailableBytes();
	if (FIFO::EoF())
		return false;

	std::size_t real_count = (count == 0) ? avail : count;
	if (real_count > avail && FIFO::IsWritable()) {
		Wait(real_count, lock);
		if (FIFO::EoF())
			return false;
	}

	return FIFO::ReadInternal(count, outBuffer, flag);
}

void SharedFIFO::Wait(const std::size_t& n, std::unique_lock<std::mutex>& lock) const {
	if (n == 0) return;

	m_cv.wait(lock, [&] {
		if (m_error || m_closed)
			return true;
		const std::size_t avail =
			(m_position_offset <= m_buffer.size())
				? (m_buffer.size() - m_position_offset)
				: 0;
		return avail >= n;
	});
}

bool SharedFIFO::WriteInternal(const std::size_t& count, const DataType& src) noexcept {
	bool result;
	{
		std::scoped_lock lock(m_mutex);
		// Base already rejects writes when closed/error
		result = FIFO::WriteInternal(count, src);
	}
	if (result)
		m_cv.notify_all();
	return result;
}

bool SharedFIFO::WriteInternal(const std::size_t& count, DataType&& src) noexcept {
	bool result;
	{
		std::scoped_lock lock(m_mutex);
		result = FIFO::WriteInternal(count, std::move(src));
	}
	if (result)
		m_cv.notify_all();
	return result;
}
