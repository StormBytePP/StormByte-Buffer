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

#include <StormByte/buffer/io/backend/bridge.hxx>

using namespace StormByte::Buffer;

IO::Backend::Bridge::Bridge(ExternalReader& in, ExternalWriter& out) noexcept:
	m_ext_in(&in),
	m_ext_out(&out) {}

IO::Backend::Bridge::Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out) noexcept:
	m_io_in(&in),
	m_io_out(&out) {}

IO::Backend::Bridge::Bridge(ExternalReader& in, IO::BufferedWriter& out) noexcept:
	m_ext_in(&in),
	m_io_out(&out) {}

IO::Backend::Bridge::Bridge(const IO::BufferedReader& in, ExternalWriter& out) noexcept:
	m_ext_out(&out),
	m_io_in(&in) {}

bool IO::Backend::Bridge::EoF() const noexcept {
	if (m_ext_in)
		return m_ext_in->EoF();
	if (m_io_in)
		return m_io_in->EoF();
	return true;
}

bool IO::Backend::Bridge::IsReadable() const noexcept {
	if (m_ext_in)
		return m_ext_in->IsReadable();
	if (m_io_in)
		return static_cast<bool>(*m_io_in);
	return false;
}

bool IO::Backend::Bridge::IsWritable() const noexcept {
	if (m_ext_out)
		return m_ext_out->IsWritable();
	if (m_io_out)
		return static_cast<bool>(*m_io_out);
	return false;
}

bool IO::Backend::Bridge::Flush() noexcept {
	if (m_io_out)
		return m_io_out->Flush().status == IO::Status::Ok;
	return true;
}

bool IO::Backend::Bridge::FlushAndClose() noexcept {
	const bool ok = Flush();
	if (m_ext_out)
		m_ext_out->Close();
	return ok;
}

void IO::Backend::Bridge::SetError() noexcept {
	if (m_ext_out)
		m_ext_out->SetError();
}

std::size_t IO::Backend::Bridge::AvailableNow() const noexcept {
	if (m_ext_in)
		return m_ext_in->AvailableBytes();
	if (m_io_in) {
		FIFO peek;
		static_cast<void>(m_io_in->Peek(0, peek));
		return peek.AvailableBytes();
	}
	return 0;
}

bool IO::Backend::Bridge::Pull(const std::size_t n, FIFO& dest) noexcept {
	dest.Clear();
	if (n == 0)
		return true;

	if (m_ext_in) {
		DataType chunk;
		if (!m_ext_in->Extract(n, chunk)) {
			if (!m_ext_in->Read(n, chunk))
				return false;
		}
		if (chunk.empty())
			return true;
		return dest.Write(std::move(chunk));
	}

	if (m_io_in) {
		const IO::Result pulled = m_io_in->Read(n, dest);
		return pulled.status == IO::Status::Ok || pulled.status == IO::Status::End;
	}

	return false;
}

bool IO::Backend::Bridge::Push(FIFO& src) noexcept {
	if (src.AvailableBytes() == 0)
		return true;

	if (m_ext_out) {
		DataType chunk;
		if (!src.Extract(0, chunk))
			return false;
		if (chunk.empty())
			return true;
		return m_ext_out->Write(std::move(chunk));
	}

	if (m_io_out) {
		for (;;) {
			const IO::Result pushed = m_io_out->Write(src);
			if (pushed.status == IO::Status::Ok)
				return true;
			if (pushed.status != IO::Status::TryAgain)
				return false;
			static_cast<void>(m_io_out->Flush());
		}
	}

	return false;
}

bool IO::Backend::Bridge::Passthrough(const std::size_t bytes) noexcept {
	if (!m_ext_in && !m_io_in)
		return false;
	if (!m_ext_out && !m_io_out)
		return false;
	if (!IsWritable())
		return false;

	std::size_t want = bytes;
	if (want == 0)
		want = AvailableNow();
	if (want == 0)
		return true;

	if (m_io_out) {
		while (!m_io_out->WillWrite(want)) {
			if (m_io_out->Flush().status != IO::Status::Ok)
				return false;
			if (!m_io_out->WillWrite(want))
				return false;
		}
	}

	FIFO work;
	if (!Pull(want, work))
		return false;
	return Push(work);
}
