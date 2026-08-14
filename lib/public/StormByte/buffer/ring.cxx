#include <StormByte/buffer/ring.hxx>
#include <StormByte/helpers.hxx>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <mutex>

using namespace StormByte::Buffer;

// ---------------------------------------------------------------------------
// Move / comparison
// ---------------------------------------------------------------------------

Ring::Ring(Ring&& other) noexcept {
	std::unique_lock lock(other.m_mutex);
	m_buffer          = std::move(other.m_buffer);
	m_position_offset = other.m_position_offset;
	m_closed          = other.m_closed;
	m_error           = other.m_error;
	m_error_message   = std::move(other.m_error_message);

	other.m_buffer.clear();
	other.m_position_offset = 0;
	other.m_closed = false;
	other.m_error  = false;
}

Ring& Ring::operator=(Ring&& other) noexcept {
	if (this != &other) {
		std::unique_lock lock_this(m_mutex, std::defer_lock);
		std::unique_lock lock_other(other.m_mutex, std::defer_lock);
		std::lock(lock_this, lock_other);

		m_buffer          = std::move(other.m_buffer);
		m_position_offset = other.m_position_offset;
		m_closed          = other.m_closed;
		m_error           = other.m_error;
		m_error_message   = std::move(other.m_error_message);

		other.m_buffer.clear();
		other.m_position_offset = 0;
		other.m_closed = false;
		other.m_error  = false;
	}
	return *this;
}

bool Ring::operator==(const Ring& other) const noexcept {
	std::shared_lock lock_this(m_mutex, std::defer_lock);
	std::shared_lock lock_other(other.m_mutex, std::defer_lock);
	std::lock(lock_this, lock_other);
	return m_buffer == other.m_buffer && m_position_offset == other.m_position_offset;
}

// ---------------------------------------------------------------------------
// Pure readers → shared_lock
// ---------------------------------------------------------------------------

std::size_t Ring::AvailableBytes() const noexcept {
	std::shared_lock lock(m_mutex);
	const std::size_t sz = m_buffer.size();
	return (m_position_offset <= sz) ? (sz - m_position_offset) : 0;
}

bool Ring::Empty() const noexcept {
	std::shared_lock lock(m_mutex);
	return m_buffer.empty();
}

bool Ring::EoF() const noexcept {
	std::shared_lock lock(m_mutex);
	return m_error || (m_closed && (m_buffer.size() <= m_position_offset));
}

bool Ring::HasError() const noexcept {
	std::shared_lock lock(m_mutex);
	return m_error;
}

bool Ring::IsReadable() const noexcept {
	std::shared_lock lock(m_mutex);
	return !m_error;
}

bool Ring::IsWritable() const noexcept {
	std::shared_lock lock(m_mutex);
	return !m_closed && !m_error;
}

std::size_t Ring::Size() const noexcept {
	std::shared_lock lock(m_mutex);
	return m_buffer.size();
}

const DataType& Ring::Data() const noexcept {
	std::unique_lock lock(m_mutex);
	m_data_cache.assign(m_buffer.begin(), m_buffer.end());
	return m_data_cache;
}

// ---------------------------------------------------------------------------
// Mutators → unique_lock
// ---------------------------------------------------------------------------

void Ring::Clean() noexcept {
	std::unique_lock lock(m_mutex);
	if (m_position_offset > 0 && m_position_offset <= m_buffer.size()) {
		m_buffer.erase(m_buffer.begin(),
					m_buffer.begin() + static_cast<std::ptrdiff_t>(m_position_offset));
	} else if (m_position_offset > m_buffer.size()) {
		m_buffer.clear();
	}
	m_position_offset = 0;
}

void Ring::Clear() noexcept {
	{
		std::unique_lock lock(m_mutex);
		m_buffer.clear();
		m_position_offset = 0;
	}
	m_cv.notify_all();
}

void Ring::Close() noexcept {
	{
		std::unique_lock lock(m_mutex);
		m_closed = true;
	}
	m_cv.notify_all();
}

void Ring::SetError() noexcept {
	{
		std::unique_lock lock(m_mutex);
		m_error = true;
	}
	m_cv.notify_all();
}

bool Ring::Drop(const std::size_t& count) noexcept {
	bool result = false;
	{
		std::unique_lock lock(m_mutex);
		if (count != 0 && count > (m_buffer.size() - m_position_offset) && !m_closed)
			Wait(count, lock);

		const std::size_t avail = (m_position_offset <= m_buffer.size())
									? (m_buffer.size() - m_position_offset) : 0;
		if (avail == 0 || count > avail)
			return false;

		m_position_offset = std::min(m_position_offset + count, m_buffer.size());
		if (m_position_offset > 0 && m_position_offset <= m_buffer.size()) {
			m_buffer.erase(m_buffer.begin(),
						m_buffer.begin() + static_cast<std::ptrdiff_t>(m_position_offset));
		}
		m_position_offset = 0;
		result = true;
	}
	m_cv.notify_all();
	return result;
}

void Ring::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	std::unique_lock lock(m_mutex);
	switch (mode) {
		case Position::Absolute:
			m_position_offset = (offset < 0) ? 0
				: std::min(static_cast<std::size_t>(offset), m_buffer.size());
			break;
		case Position::Relative:
			if (offset < 0) {
				m_position_offset = static_cast<std::size_t>(
					std::max<std::ptrdiff_t>(0,
						static_cast<std::ptrdiff_t>(m_position_offset) + offset));
			} else {
				m_position_offset = std::min(m_position_offset + static_cast<std::size_t>(offset),
											m_buffer.size());
			}
			break;
		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// HexDump
// ---------------------------------------------------------------------------

std::string Ring::HexDump(const std::size_t& columns,
						const std::size_t& byte_limit) const noexcept {
	std::shared_lock lock(m_mutex);
	const std::size_t cols = (columns == 0) ? 16 : columns;
	const std::size_t end  = (byte_limit > 0)
								? std::min(m_buffer.size(), m_position_offset + byte_limit)
								: m_buffer.size();

	std::ostringstream oss = HexDumpHeader();
	oss << '\n';

	if (end > m_position_offset) {
		DataType tmp(m_buffer.begin() + static_cast<std::ptrdiff_t>(m_position_offset),
					m_buffer.begin() + static_cast<std::ptrdiff_t>(end));
		std::span<const std::byte> view(tmp.data(), tmp.size());
		oss << FormatHexLines(view, m_position_offset, cols);
	}
	return oss.str();
}

std::ostringstream Ring::HexDumpHeader() const noexcept {
	std::ostringstream oss;
	oss << "Size: " << m_buffer.size() << " bytes\n";
	oss << "Read Position: " << m_position_offset << '\n';
	oss << "Status: " << (m_closed ? "closed" : "opened")
		<< " and " << (m_error ? "error" : "ready");
	return oss;
}

std::string Ring::FormatHexLines(std::span<const std::byte> data,
								std::size_t start_offset,
								std::size_t columns) noexcept {
	const std::size_t cols = (columns == 0) ? 16 : columns;
	const int offset_width = 8;
	std::vector<std::string> lines;
	for (std::size_t i = 0; i < data.size(); i += cols) {
		const std::size_t line_end = std::min(data.size(), i + cols);
		std::ostringstream line;
		line << std::hex << std::uppercase << std::setw(offset_width)
			<< std::setfill('0') << (start_offset + i) << ": "
			<< std::dec << std::setfill(' ');
		for (std::size_t j = i; j < i + cols; ++j) {
			if (j < line_end) {
				const unsigned int val = static_cast<unsigned int>(std::to_integer<unsigned char>(data[j]));
				line << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << val << ' ' << std::dec;
			} else {
				line << "   ";
			}
		}
		line << "  ";
		for (std::size_t j = i; j < line_end; ++j) {
			const unsigned char c = std::to_integer<unsigned char>(data[j]);
			line << (std::isprint(c) ? static_cast<char>(c) : '.');
		}
		lines.push_back(line.str());
	}
	std::ostringstream oss;
	for (std::size_t li = 0; li < lines.size(); ++li) {
		oss << lines[li];
		if (li + 1 < lines.size()) oss << '\n';
	}
	return oss.str();
}

// ---------------------------------------------------------------------------
// Read / Extract / Peek
// ---------------------------------------------------------------------------

bool Ring::Peek(const std::size_t& count, DataType& outBuffer) const noexcept {
	return const_cast<Ring*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
}
bool Ring::Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept {
	return const_cast<Ring*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
}
bool Ring::Read(const std::size_t& count, DataType& outBuffer) const noexcept {
	return const_cast<Ring*>(this)->ReadInternal(count, outBuffer, Operation::Read);
}
bool Ring::Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept {
	return const_cast<Ring*>(this)->ReadInternal(count, outBuffer, Operation::Read);
}
bool Ring::Extract(const std::size_t& count, DataType& outBuffer) noexcept {
	return ReadInternal(count, outBuffer, Operation::Extract);
}
bool Ring::Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept {
	return ReadInternal(count, outBuffer, Operation::Extract);
}

void Ring::ReadUntilEoF(DataType& outBuffer) const noexcept {
	const_cast<Ring*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
}
void Ring::ReadUntilEoF(WriteOnly& outBuffer) const noexcept {
	const_cast<Ring*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
}
void Ring::ExtractUntilEoF(DataType& outBuffer) noexcept {
	ReadUntilEoFInternal(outBuffer, Operation::Extract);
}
void Ring::ExtractUntilEoF(WriteOnly& outBuffer) noexcept {
	ReadUntilEoFInternal(outBuffer, Operation::Extract);
}

bool Ring::ReadInternal(const std::size_t& count, DataType& outBuffer, Operation flag) noexcept {
	DataType local;
	{
		std::unique_lock lock(m_mutex);

		std::size_t avail = (m_position_offset <= m_buffer.size())
								? (m_buffer.size() - m_position_offset) : 0;

		if (m_error || (m_closed && avail == 0))
			return false;

		// count == 0 → all available; if empty and still open, wait for data or close
		if (count == 0 && avail == 0 && !m_closed) {
			Wait(1, lock);
			avail = (m_position_offset <= m_buffer.size())
						? (m_buffer.size() - m_position_offset) : 0;
			if (avail == 0)
				return false; // closed/error with nothing left
		}

		const std::size_t real_count = (count == 0) ? avail : count;
		if (real_count > avail && !m_closed)
			Wait(real_count, lock);

		avail = (m_position_offset <= m_buffer.size())
					? (m_buffer.size() - m_position_offset) : 0;
		if ((avail == 0 && count == 0) || real_count > avail)
			return false;

		auto start = m_buffer.begin() + static_cast<std::ptrdiff_t>(m_position_offset);

		switch (flag) {
			case Operation::Read:
				local.insert(local.end(), start, start + static_cast<std::ptrdiff_t>(real_count));
				m_position_offset += real_count;
				break;
			case Operation::Peek:
				local.insert(local.end(), start, start + static_cast<std::ptrdiff_t>(real_count));
				break;
			case Operation::Extract:
				local.insert(local.end(),
							std::make_move_iterator(start),
							std::make_move_iterator(start + static_cast<std::ptrdiff_t>(real_count)));
				m_buffer.erase(start, start + static_cast<std::ptrdiff_t>(real_count));
				if (m_position_offset > m_buffer.size())
					m_position_offset = m_buffer.size();
				break;
			default:
				return false;
		}
	}

	outBuffer.insert(outBuffer.end(),
					std::make_move_iterator(local.begin()),
					std::make_move_iterator(local.end()));
	return true;
}

bool Ring::ReadInternal(const std::size_t& count, WriteOnly& outBuffer, Operation flag) noexcept {
	DataType temp;
	if (!ReadInternal(count, temp, flag))
		return false;
	return outBuffer.Write(std::move(temp));
}

void Ring::ReadUntilEoFInternal(DataType& outBuffer, Operation flag) noexcept {
	while (true) {
		{
			std::unique_lock lock(m_mutex);
			if (m_error)
				return;

			m_cv.wait(lock, [&] {
				if (m_error || m_closed)
					return true;
				const std::size_t avail = (m_position_offset <= m_buffer.size())
					? (m_buffer.size() - m_position_offset) : 0;
				return avail > 0;
			});

			if (m_error)
				return;

			const std::size_t avail = (m_position_offset <= m_buffer.size())
				? (m_buffer.size() - m_position_offset) : 0;
			if (avail == 0 && m_closed)
				return; // true EoF
		}

		DataType chunk;
		bool ok = false;
		switch (flag) {
			case Operation::Read:    ok = Read(0, chunk);    break;
			case Operation::Extract: ok = Extract(0, chunk); break;
			default: return;
		}
		if (!ok || chunk.empty()) {
			if (EoF())
				return;
			continue;
		}
		outBuffer.insert(outBuffer.end(),
						std::make_move_iterator(chunk.begin()),
						std::make_move_iterator(chunk.end()));
	}
}

void Ring::ReadUntilEoFInternal(WriteOnly& outBuffer, Operation flag) noexcept {
	DataType tmp;
	ReadUntilEoFInternal(tmp, flag);
	if (!tmp.empty())
		(void)outBuffer.Write(std::move(tmp));
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

bool Ring::Write(const std::size_t& count, const DataType& data) noexcept {
	return WriteInternal(count, data);
}
bool Ring::Write(const std::size_t& count, DataType&& data) noexcept {
	return WriteInternal(count, std::move(data));
}
bool Ring::Write(const std::size_t& count, const ReadOnly& data) noexcept {
	DataType tmp;
	if (!data.Read(count, tmp)) return false;
	return WriteInternal(0, std::move(tmp));
}
bool Ring::Write(const std::size_t& count, ReadOnly&& data) noexcept {
	DataType tmp;
	if (!data.Extract(count, tmp)) return false;
	return WriteInternal(0, std::move(tmp));
}

bool Ring::WriteInternal(const std::size_t& count, const DataType& src) noexcept {
	bool result = false;
	{
		std::unique_lock lock(m_mutex);
		if (m_closed || m_error) return false;

		const std::size_t real_count = (count == 0) ? src.size() : count;
		if (real_count > src.size()) return false;

		m_buffer.insert(m_buffer.end(),
						src.begin(),
						src.begin() + static_cast<std::ptrdiff_t>(real_count));
		result = true;
	}
	m_cv.notify_all();
	return result;
}

bool Ring::WriteInternal(const std::size_t& count, DataType&& src) noexcept {
	bool result = false;
	{
		std::unique_lock lock(m_mutex);
		if (m_closed || m_error) return false;

		const std::size_t real_count = (count == 0) ? src.size() : count;
		if (real_count > src.size()) return false;

		if (real_count == src.size()) {
			m_buffer.insert(m_buffer.end(),
							std::make_move_iterator(src.begin()),
							std::make_move_iterator(src.end()));
		} else {
			m_buffer.insert(m_buffer.end(),
							std::make_move_iterator(src.begin()),
							std::make_move_iterator(src.begin() + static_cast<std::ptrdiff_t>(real_count)));
			src.erase(src.begin(), src.begin() + static_cast<std::ptrdiff_t>(real_count));
		}
		result = true;
	}
	m_cv.notify_all();
	return result;
}

// ---------------------------------------------------------------------------
// Wait
// ---------------------------------------------------------------------------

void Ring::Wait(const std::size_t& n, std::unique_lock<std::shared_mutex>& lock) const {
	if (n == 0) return;
	m_cv.wait(lock, [&] {
		if (m_closed || m_error) return true;
		const std::size_t sz = m_buffer.size();
		const std::size_t rp = m_position_offset;
		return sz >= rp + n;
	});
}
