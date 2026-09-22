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

#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/io/backend/bridge.hxx>

using namespace StormByte::Buffer;

Bridge::Bridge(ExternalReader& in, ExternalWriter& out, const std::size_t high_water) noexcept:
	m_io(std::make_unique<IO::Backend::Bridge>(in, out, high_water)) {}

Bridge::Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out, const std::size_t high_water) noexcept:
	m_io(std::make_unique<IO::Backend::Bridge>(in, out, high_water)) {}

Bridge::Bridge(const IO::BufferedReader& in, IO::BufferedWriter& out) noexcept:
	Bridge(in, out, 0) {}

Bridge::Bridge(ExternalReader& in, IO::BufferedWriter& out, const std::size_t high_water) noexcept:
	m_io(std::make_unique<IO::Backend::Bridge>(in, out, high_water)) {}

Bridge::Bridge(ExternalReader& in, IO::BufferedWriter& out) noexcept:
	Bridge(in, out, 0) {}

Bridge::Bridge(const IO::BufferedReader& in, ExternalWriter& out, const std::size_t high_water) noexcept:
	m_io(std::make_unique<IO::Backend::Bridge>(in, out, high_water)) {}

Bridge::Bridge(Bridge&& other) noexcept:
	m_io(std::move(other.m_io)) {}

Bridge::~Bridge() noexcept = default;

Bridge& Bridge::operator=(Bridge&& other) noexcept {
	if (this != &other)
		m_io = std::move(other.m_io);
	return *this;
}

bool Bridge::EoF() const noexcept {
	return m_io && m_io->EoF();
}

bool Bridge::IsReadable() const noexcept {
	return m_io && m_io->IsReadable();
}

bool Bridge::IsWritable() const noexcept {
	return m_io && m_io->IsWritable();
}

std::size_t Bridge::HighWater() const noexcept {
	return m_io ? m_io->HighWater() : 0;
}

void Bridge::HighWater(const std::size_t high_water) noexcept {
	if (m_io)
		m_io->HighWater(high_water);
}

IO::Drainer::Status Bridge::Drainer() const noexcept {
	return m_io ? m_io->Drainer() : IO::Drainer::Status::Stopped;
}

bool Bridge::Drainer(const IO::Drainer::Operation operation) noexcept {
	return m_io && m_io->Drainer(operation);
}

bool Bridge::Flush() noexcept {
	return m_io && m_io->BarrierFlush();
}

bool Bridge::FlushAndClose() noexcept {
	return m_io && m_io->FlushAndClose();
}

void Bridge::SetError() noexcept {
	if (m_io)
		m_io->SetError();
}
