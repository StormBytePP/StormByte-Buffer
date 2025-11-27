#include <StormByte/buffer/shared_fifo.hxx>

using namespace StormByte::Buffer;

std::size_t SharedFIFO::Capacity() const noexcept {
    std::scoped_lock<std::mutex> lock(m_mutex);
    return FIFO::Capacity();
}

void SharedFIFO::Clear() noexcept {
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        FIFO::Clear();
    }
    m_cv.notify_all();
}

void SharedFIFO::Reserve(std::size_t newCapacity) {
    std::scoped_lock<std::mutex> lock(m_mutex);
    FIFO::Reserve(newCapacity);
}

bool SharedFIFO::Write(const std::vector<std::byte>& data) {
	bool result;
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        if (m_closed.load() || data.empty()) {
            result = false;
        } else {
            const std::size_t count = data.size();
            const std::size_t required = m_size.load() + count;
            if (required > m_buffer.size()) {
                std::size_t newCap = m_buffer.empty() ? 64 : m_buffer.size();
                while (newCap < required) newCap *= 2;
                // Ensure we call base Reserve without re-entering our override
                FIFO::Reserve(newCap);
            }
            // With capacity ensured, delegate to base write (won't re-enter Reserve)
            result = FIFO::Write(data);
        }
    }
    m_cv.notify_all();
	return result;
}

bool SharedFIFO::Write(const std::string& data) {
	bool result;
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        if (data.empty() || m_closed.load()) {
            result = false;
        } else {
            std::vector<std::byte> tmp;
            tmp.resize(data.size());
            std::copy_n(reinterpret_cast<const std::byte*>(data.data()), data.size(), tmp.begin());
            const std::size_t count = tmp.size();
            const std::size_t required = m_size.load() + count;
            if (required > m_buffer.size()) {
                std::size_t newCap = m_buffer.empty() ? 64 : m_buffer.size();
                while (newCap < required) newCap *= 2;
                FIFO::Reserve(newCap);
            }
            result = FIFO::Write(tmp);
        }
    }
    m_cv.notify_all();
	return result;
}

ExpectedData<InsufficientData> SharedFIFO::Read(std::size_t count) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (count != 0) {
        Wait(count, lock);
        // If closed and insufficient data, read whatever is available
        if (m_closed.load()) {
            const std::size_t sz = m_size.load();
            const std::size_t rp = m_read_position.load();
            const std::size_t available = (rp <= sz) ? (sz - rp) : 0;
            if (available < count) {
                return FIFO::Read(0); // Read all available
            }
        }
    }
    return FIFO::Read(count);
}

ExpectedData<InsufficientData> SharedFIFO::Extract(std::size_t count) {
    ExpectedData<InsufficientData> result;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (count != 0) {
            Wait(count, lock);
            // If closed and insufficient data, extract whatever is available
            if (m_closed.load() && m_size.load() < count) {
                result = FIFO::Extract(0); // Extract all available
            } else {
                result = FIFO::Extract(count);
            }
        } else {
            result = FIFO::Extract(count);
        }
        lock.unlock();
    }
    if (result && !result->empty()) m_cv.notify_all();
    return result;
}

void SharedFIFO::Seek(const std::size_t& position, const Position& mode) {
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        FIFO::Seek(position, mode);
    }
    m_cv.notify_all();
}

void SharedFIFO::Close() noexcept {
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        m_closed.store(true);
    }
    m_cv.notify_all();
}

void SharedFIFO::Wait(std::size_t n, std::unique_lock<std::mutex>& lock) {
    if (n == 0) return;
    m_cv.wait(lock, [&] {
        if (m_closed.load()) return true;
        const std::size_t sz = m_size.load();
        const std::size_t rp = m_read_position.load();
        return sz >= rp + n; // at least n bytes available from current read position
    });
}
