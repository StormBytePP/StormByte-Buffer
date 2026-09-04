/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * or later, as published by the Free Software Foundation.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 */

#include <StormByte/buffer/fifo.hxx>
#include <StormByte/helpers.hxx>
#include <StormByte/string.hxx>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>
using namespace StormByte::Buffer;
namespace {
	// Non-virtual helper usable while a derived class holds its mutex.
	inline std::size_t available_bytes_impl(const DataType& buf, std::size_t pos) noexcept {
		return (pos <= buf.size()) ? (buf.size() - pos) : 0;
	}
}
FIFO::FIFO(const FIFO& other) noexcept
	: Generic(other), ReadWrite(other),
	m_buffer(other.m_buffer),
	m_position_offset(other.m_position_offset),
	m_closed(other.m_closed),
	m_error(other.m_error)
{}
FIFO::FIFO(FIFO&& other) noexcept
	: Generic(std::move(other)), ReadWrite(std::move(other)),
	m_buffer(std::move(other.m_buffer)),
	m_position_offset(other.m_position_offset),
	m_closed(other.m_closed),
	m_error(other.m_error)
{
	other.m_position_offset = 0;
	other.m_closed = false;
	other.m_error  = false;
}
FIFO& FIFO::operator=(const FIFO& other) {
	if (this != &other) {
		Generic::operator=(other);
		m_buffer          = other.m_buffer;
		m_position_offset = other.m_position_offset;
		m_closed          = other.m_closed;
		m_error           = other.m_error;
	}
	return *this;
}
FIFO& FIFO::operator=(FIFO&& other) noexcept {
	if (this != &other) {
		Generic::operator=(std::move(other));
		m_buffer          = std::move(other.m_buffer);
		m_position_offset = other.m_position_offset;
		m_closed          = other.m_closed;
		m_error           = other.m_error;
		other.m_position_offset = 0;
		other.m_closed = false;
		other.m_error  = false;
	}
	return *this;
}
void FIFO::Clean() noexcept {
	if (m_position_offset > 0 && m_position_offset <= m_buffer.size()) {
		const std::size_t remaining = m_buffer.size() - m_position_offset;
		if (remaining > 0) {
			std::memmove(m_buffer.data(), m_buffer.data() + m_position_offset, remaining);
			m_buffer.resize(remaining);
			if (m_buffer.capacity() > remaining * 4 && m_buffer.capacity() > 4096) {
				m_buffer.shrink_to_fit();
			}
		} else {
			m_buffer.clear();
			if (m_buffer.capacity() > 4096) {
				m_buffer.shrink_to_fit();
			}
		}
	}
	else {
		m_buffer.clear();
	}
	m_position_offset = 0;
}
bool FIFO::Drop(const std::size_t& count) noexcept {
	const std::size_t avail =
		(m_position_offset <= m_buffer.size())
			? (m_buffer.size() - m_position_offset)
			: 0;
	if (avail == 0 || count > avail)
		return false;
	m_position_offset = std::min(m_position_offset + count, m_buffer.size());
	FIFO::Clean();   // ← NO Clean() virtual
	return true;
}
void FIFO::Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept {
	switch (mode) {
		case Position::Absolute:
			if (offset < 0) {
				m_position_offset = 0;
			}
			else {
				m_position_offset = std::min(static_cast<std::size_t>(offset), m_buffer.size());
			}
			break;
		case Position::Relative:
			if (offset < 0) {
				m_position_offset = static_cast<std::size_t>(
					std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(m_position_offset) + offset));
			} else {
				m_position_offset = std::min(m_position_offset + static_cast<std::size_t>(offset), m_buffer.size());
			}
			break;
		default:
			return;
	}
}
std::string FIFO::HexDump(const std::size_t& columns, const std::size_t& byte_limit) const noexcept {
	const std::size_t cols = (columns == 0) ? 16 : columns;
	const std::size_t end = (byte_limit > 0)
		? std::min(m_buffer.size(), m_position_offset + byte_limit)
		: m_buffer.size();
	std::ostringstream oss = HexDumpHeader();
	oss << '\n';
	if (end > m_position_offset) {
		std::span<const std::byte> view(m_buffer.data() + m_position_offset, end - m_position_offset);
		const std::string lines = FormatHexLines(view, m_position_offset, cols);
		oss << lines;
	}
	return oss.str();
}
std::string FIFO::FormatHexLines(std::span<const std::byte>& data, std::size_t start_offset, std::size_t columns) noexcept {
	const std::size_t cols = (columns == 0) ? 16 : columns;
	const int offset_width = 8;
	std::vector<std::string> lines;
	for (std::size_t i = 0; i < data.size(); i += cols) {
		const std::size_t line_end = std::min(data.size(), i + cols);
		std::ostringstream line;
		line << std::hex << std::uppercase << std::setw(offset_width) << std::setfill('0')
			<< (start_offset + i) << ": " << std::dec << std::setfill(' ');
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
std::ostringstream FIFO::HexDumpHeader() const noexcept {
	std::ostringstream oss;
	oss << "Size: " << m_buffer.size() << " bytes\n";
	oss << "Read Position: " << m_position_offset << '\n';
	oss << "Status: " << (m_closed ? "closed" : "open")
		<< " / " << (m_error ? "error" : "ok");
	return oss;
}
bool FIFO::ReadInternal(const std::size_t& count, DataType& outBuffer, const Operation& flag) noexcept {
	if (m_error)
		return false;
	const std::size_t available_bytes = available_bytes_impl(m_buffer, m_position_offset);
	const std::size_t real_count = count == 0 ? available_bytes : count;
	if ((available_bytes == 0 && count == 0) || real_count > available_bytes)
		return false;
	outBuffer.reserve(outBuffer.size() + real_count);
	const auto start_it = m_buffer.begin() + static_cast<std::ptrdiff_t>(m_position_offset);
	switch (flag) {
		case Operation::Read: {
			outBuffer.insert(outBuffer.end(), start_it, start_it + static_cast<std::ptrdiff_t>(real_count));
			m_position_offset += real_count;
			break;
		}
		case Operation::Peek: {
			outBuffer.insert(outBuffer.end(), start_it, start_it + static_cast<std::ptrdiff_t>(real_count));
			break;
		}
		case Operation::Extract: {
			outBuffer.insert(outBuffer.end(),
				std::make_move_iterator(start_it),
				std::make_move_iterator(start_it + static_cast<std::ptrdiff_t>(real_count)));
			m_buffer.erase(start_it, start_it + static_cast<std::ptrdiff_t>(real_count));
			if (m_position_offset > m_buffer.size()) {
				m_position_offset = m_buffer.size();
			}
			break;
		}
		default:
			return false;
	}
	return true;
}
bool FIFO::ReadInternal(const std::size_t& count, WriteOnly& outBuffer, const Operation& flag) noexcept {
	if (m_error)
		return false;
	const std::size_t available_bytes = available_bytes_impl(m_buffer, m_position_offset);
	const std::size_t real_count = count == 0 ? available_bytes : count;
	if ((count == 0 && available_bytes == 0) || real_count > available_bytes)
		return false;
	DataType temp;
	temp.reserve(real_count);
	if (!FIFO::ReadInternal(count, temp, flag))
		return false;
	return outBuffer.Write(std::move(temp));
}
void FIFO::ReadUntilEoFInternal(DataType& outBuffer, const Operation& flag) noexcept {
	while (true) {
		switch (flag) {
			case Operation::Read:
				(void)Read(0, outBuffer);
				break;
			case Operation::Extract:
				(void)Extract(0, outBuffer);
				break;
			default:
				return;
		}
		DataType unused;
		if (!Peek(1, unused)) {
			return;
		}
	}
}
void FIFO::ReadUntilEoFInternal(WriteOnly& outBuffer, const Operation& flag) noexcept {
	while (true) {
		switch (flag) {
			case Operation::Read:
				(void)Read(0, outBuffer);
				break;
			case Operation::Extract:
				(void)Extract(0, outBuffer);
				break;
			default:
				return;
		}
		DataType unused;
		if (!Peek(1, unused)) {
			return;
		}
	}
}
bool FIFO::WriteInternal(const std::size_t& count, const DataType& src) noexcept {
	if (m_closed || m_error)
		return false;
	if (count > 0 && src.size() < count)
		return false;
	const std::size_t real_count = (count == 0) ? src.size() : count;
	m_buffer.reserve(m_buffer.size() + real_count);
	if (real_count == src.size()) {
		append_vector(m_buffer, src);
	}
	else {
		m_buffer.insert(m_buffer.end(), src.begin(), src.begin() + static_cast<std::ptrdiff_t>(real_count));
	}
	return true;
}
bool FIFO::WriteInternal(const std::size_t& count, DataType&& src) noexcept {
	if (m_closed || m_error)
		return false;
	if (count > 0 && src.size() < count)
		return false;
	const std::size_t real_count = (count == 0) ? src.size() : count;
	m_buffer.reserve(m_buffer.size() + real_count);
	if (real_count == src.size()) {
		append_vector(m_buffer, std::move(src));
	}
	else {
		m_buffer.insert(m_buffer.end(),
			std::make_move_iterator(src.begin()),
			std::make_move_iterator(src.begin() + static_cast<std::ptrdiff_t>(real_count)));
		src.erase(src.begin(), src.begin() + static_cast<std::ptrdiff_t>(real_count));
	}
	return true;
}
bool FIFO::WriteInternal(const std::size_t& count, const ReadOnly& src) noexcept {
	if (m_closed || m_error)
		return false;
	return src.Read(count, m_buffer);
}
bool FIFO::WriteInternal(const std::size_t& count, ReadOnly&& src) noexcept {
	if (m_closed || m_error)
		return false;
	return src.Extract(count, m_buffer);
}
