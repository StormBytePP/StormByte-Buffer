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
	 * @brief High-performance lock-free SPSC ring buffer (private / internal).
	 *
	 * Designed exclusively for intermediate Pipeline stages.
	 * Correct behaviour is guaranteed only under Single-Producer /
	 * Single-Consumer access (the pattern used by the current Async Pipeline).
	 *
	 * - Lock-free data path (atomics + power-of-two circular buffer)
	 * - Grows automatically (doubles capacity) when full
	 * - Minimal overhead, good cache locality
	 * - Same @ref ReadWrite contract as @ref Ring for drop-in use inside Pipeline
	 *
	 * Blocking waits (when data is not yet available) use a mutex + condition
	 * variable; the data path itself stays lock-free.
	 *
	 * @warning Never share a LockFreeRing instance between multiple producers
	 *          or multiple consumers. Doing so is undefined behaviour.
	 *
	 * @see Pipeline, Ring, ReadWrite
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

			/**
			 * @brief Whether @ref SetError() has been called.
			 * @return true if the buffer is in a permanent error state.
			 */
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

			/**
			 * @brief Close the buffer for further writes (SPSC-safe).
			 * @details Sets the closed flag and wakes any @c WaitFor() waiters.
			 *          Remaining bytes can still be read until EoF.
			 */
			void Close() noexcept override;

			/**
			 * @brief Enter a permanent error state.
			 * @details Makes the buffer unreadable and unwritable and wakes waiters.
			 */
			void SetError() noexcept override;

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
