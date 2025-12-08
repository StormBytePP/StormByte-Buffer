#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/fifo.hxx>

#include <algorithm>

using namespace StormByte::Buffer;

bool Bridge::Passthrough(const std::size_t& count) noexcept {
	const std::size_t bytes_to_transfer = count == 0 ? m_readHandler.get().AvailableBytes() : count;
	std::size_t remaining = bytes_to_transfer;

	while (remaining > 0) {
		const std::size_t chunk_size = std::min(remaining, m_chunk_size);
		auto res = m_readHandler.get().Extract(chunk_size, m_writeHandler.get());
		if (!res)
			return false;

		remaining -= chunk_size;
	}

	return true;
}

bool Bridge::Passthrough() noexcept {
	while (!m_readHandler.get().EoF()) {
		auto res = Passthrough(m_readHandler.get().AvailableBytes());
		if (!res)
			return false;
	}

	return true;
}