#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace StormByte::Buffer {

/**
 * @class LockFreeRing
 * @brief Extremely high-performance lock-free SPSC ring buffer (private).
 *
 * Designed exclusively for internal use by Pipeline stages.
 * Guarantees correct behaviour only under Single-Producer / Single-Consumer
 * access (which is exactly the pattern used by the current Async Pipeline).
 *
 * - Lock-free data path (atomics + power-of-two circular buffer)
 * - Grows automatically (doubles capacity) when full
 * - Minimal overhead, excellent cache locality
 * - Same ReadWrite contract as Ring for easy drop-in replacement inside Pipeline
 *
 * @warning Never share a LockFreeRing instance between multiple producers
 *          or multiple consumers. Doing so results in undefined behaviour.
 */
class STORMBYTE_BUFFER_PRIVATE LockFreeRing final : public ReadWrite {
public:
	/**
	 * @brief Construct with an initial capacity (rounded up to power of two).
	 * @param initial_capacity Suggested starting size (default 1 MiB).
	 */
	explicit LockFreeRing(std::size_t initial_capacity = 1u << 20);

	LockFreeRing(const LockFreeRing&) = delete;
	LockFreeRing& operator=(const LockFreeRing&) = delete;

	LockFreeRing(LockFreeRing&& other) noexcept;
	LockFreeRing& operator=(LockFreeRing&& other) noexcept;

	~LockFreeRing() noexcept override = default;

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------
	std::size_t AvailableBytes() const noexcept override;
	bool        Empty() const noexcept override;
	bool        EoF() const noexcept override;
	bool        HasError() const noexcept;
	bool        IsReadable() const noexcept override;
	bool        IsWritable() const noexcept override;
	std::size_t Size() const noexcept override;
	const DataType& Data() const noexcept override;

	// -----------------------------------------------------------------
	// Mutators
	// -----------------------------------------------------------------
	void Clean() noexcept override;
	void Clear() noexcept override;
	void Close() noexcept;
	void SetError() noexcept;
	bool Drop(const std::size_t& count) noexcept override;
	void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

	// -----------------------------------------------------------------
	// Read / Extract / Peek
	// -----------------------------------------------------------------
	bool Peek(const std::size_t& count, DataType& out) const noexcept override;
	bool Peek(const std::size_t& count, WriteOnly& out) const noexcept override;
	bool Read(const std::size_t& count, DataType& out) const noexcept override;
	bool Read(const std::size_t& count, WriteOnly& out) const noexcept override;
	bool Extract(const std::size_t& count, DataType& out) noexcept override;
	bool Extract(const std::size_t& count, WriteOnly& out) noexcept override;

	void ReadUntilEoF(DataType& out) const noexcept override;
	void ReadUntilEoF(WriteOnly& out) const noexcept override;
	void ExtractUntilEoF(DataType& out) noexcept override;
	void ExtractUntilEoF(WriteOnly& out) noexcept override;

	// -----------------------------------------------------------------
	// Write
	// -----------------------------------------------------------------
	bool Write(const std::size_t& count, const DataType& data) noexcept override;
	bool Write(const std::size_t& count, DataType&& data) noexcept override;
	bool Write(const std::size_t& count, const ReadOnly& data) noexcept override;
	bool Write(const std::size_t& count, ReadOnly&& data) noexcept override;

	using WriteOnly::Write;

private:
	enum class Operation { Extract, Read, Peek };

	// Storage (power-of-two circular buffer)
	std::vector<std::byte>          m_storage;
	std::size_t                     m_capacity = 0;
	std::size_t                     m_mask     = 0;

	// Atomic positions (SPSC)
	alignas(64) std::atomic<std::size_t> m_head{0};   // consumer
	alignas(64) std::atomic<std::size_t> m_tail{0};   // producer

	// Logical read cursor (for non-destructive Read/Peek)
	mutable std::atomic<std::size_t> m_logical{0};

	std::atomic<bool> m_closed{false};
	std::atomic<bool> m_error{false};

	// Only used for blocking waits (data path stays lock-free)
	mutable std::mutex              m_wait_mtx;
	mutable std::condition_variable m_cv;

	// Cache for Data()
	mutable DataType m_data_cache;

	// Helpers
	static std::size_t RoundUpPow2(std::size_t v) noexcept;
	void               Grow() noexcept;               // doubles capacity (producer only)
	bool               WaitFor(std::size_t n) const;  // returns false on closed/error

	bool ReadInternal(std::size_t count, DataType& out, Operation op) noexcept;
	bool WriteInternal(std::size_t count, const std::byte* src) noexcept;
};

}
