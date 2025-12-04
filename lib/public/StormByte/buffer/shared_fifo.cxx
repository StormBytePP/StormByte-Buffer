#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>

#include <sstream>
#include <iomanip>
#include <cctype>
#include <iostream>
#include <thread>

using namespace StormByte::Buffer;

SharedFIFO& SharedFIFO::operator=(const FIFO& other) {
	std::unique_lock<std::mutex> lock(m_mutex);

	FIFO::operator=(other);
	m_closed = false;
	m_error = false;

	m_cv.notify_all();

	return *this;
}

SharedFIFO& SharedFIFO::operator=(FIFO&& other) noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);

	FIFO::operator=(std::move(other));
	m_closed = false;
	m_error = false;

	m_cv.notify_all();

	return *this;
}

bool SharedFIFO::operator==(const SharedFIFO& other) const noexcept {
	std::scoped_lock<std::mutex, std::mutex> lock(m_mutex, other.m_mutex);
	return static_cast<const FIFO&>(*this) == static_cast<const FIFO&>(other);
}

std::size_t SharedFIFO::AvailableBytes() const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	return FIFO::AvailableBytes();
}

void SharedFIFO::Clean() noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	FIFO::Clean();
}

void SharedFIFO::Clear() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		FIFO::Clear();
	}
	m_cv.notify_all();
}

void SharedFIFO::Close() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		m_closed = true;
	}
	m_cv.notify_all();
}

ExpectedVoid<WriteError> SharedFIFO::Drop(const std::size_t& count) noexcept {
	ExpectedVoid<WriteError> result;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (count != 0 && count > FIFO::AvailableBytes())
			Wait(count, lock);

		result = FIFO::Drop(count);
	}
	m_cv.notify_all();
	return result;
}

bool SharedFIFO::Empty() const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	return FIFO::Empty();
}

bool SharedFIFO::EoF() const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	return m_error || (m_closed && FIFO::AvailableBytes() == 0);
}

bool SharedFIFO::HasError() const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	return m_error;
}

std::string SharedFIFO::HexDump(const std::size_t& collumns, const std::size_t& byte_limit) const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	
	return FIFO::HexDump(collumns, byte_limit);
}

void SharedFIFO::SetError() noexcept {
	{
		std::scoped_lock<std::mutex> lock(m_mutex);
		m_error = true;
	}
	m_cv.notify_all();
}


void SharedFIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	FIFO::Seek(offset, mode);
}

std::size_t SharedFIFO::Size() const noexcept {
	std::scoped_lock<std::mutex> lock(m_mutex);
	return FIFO::Size();
}

std::ostringstream SharedFIFO::HexDumpHeader() const noexcept {
	std::ostringstream oss = FIFO::HexDumpHeader();
	oss << "Status: " << (m_closed ? "closed" : "opened") << " and " << (m_error ? "error" : "ready");
	return oss;
}

ExpectedVoid<ReadError> SharedFIFO::ReadInternal(const std::size_t& count, DataType& outBuffer, const Operation& flag) noexcept {
	std::unique_lock<std::mutex> lock(m_mutex);
	// Check EOF / error under the lock to avoid re-locking inside EoF().
	std::size_t avail = FIFO::AvailableBytes();
	if (m_error || (m_closed && avail == 0))
		return StormByte::Unexpected(ReadError("End of file reached"));

	std::size_t real_count = count == 0 ? avail : count;
	if (real_count > avail && !m_closed) {
		Wait(real_count, lock);
	}

	auto result = FIFO::ReadInternal(count, outBuffer, flag);
	return result;
}

ExpectedVoid<Error> SharedFIFO::ReadInternal(const std::size_t& count, WriteOnly& outBuffer, const Operation& flag) noexcept {
	std::unique_lock<std::mutex> lock(m_mutex);
	// Check EOF / error under the lock to avoid re-locking inside EoF().
	std::size_t avail = FIFO::AvailableBytes();
	if (m_error || (m_closed && avail == 0))
		return StormByte::Unexpected(ReadError("End of file reached"));

	std::size_t real_count = count == 0 ? avail : count;
	if (real_count > avail && !m_closed) {
		Wait(real_count, lock);
	}

	auto result = FIFO::ReadInternal(count, outBuffer, flag);
	return result;
}

void SharedFIFO::Wait(const std::size_t& n, std::unique_lock<std::mutex>& lock) const {
	if (n == 0) return;
	m_cv.wait(lock, [&] {
		if (m_closed) { return true; }
		if (m_error) { return true; }
		const std::size_t sz = m_buffer.size();
		const std::size_t rp = m_position_offset;
		bool ready = sz >= rp + n;
		return ready;
	});
}

ExpectedVoid<WriteError> SharedFIFO::WriteInternal(const std::size_t& count, const DataType& src) noexcept {
	ExpectedVoid<WriteError> result;
	{
		std::scoped_lock lock(m_mutex);
		if (m_closed) {
		return StormByte::Unexpected(WriteError("Buffer is closed"));
		}
		if (m_error) {
		return StormByte::Unexpected(WriteError("Buffer in error state"));
		}
		result = FIFO::WriteInternal(count, src);
	}
	m_cv.notify_all();
	return result;
}

ExpectedVoid<WriteError> SharedFIFO::WriteInternal(const std::size_t& count, DataType&& src) noexcept {
	ExpectedVoid<WriteError> result;
	{
		std::scoped_lock lock(m_mutex);
		if (m_closed) {
		return StormByte::Unexpected(WriteError("Buffer is closed"));
		}
		if (m_error) {
		return StormByte::Unexpected(WriteError("Buffer in error state"));
		}
		result = FIFO::WriteInternal(count, std::move(src));
	}
	m_cv.notify_all();
	return result;
}