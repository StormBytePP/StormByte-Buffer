/**
 * @file fifo.hxx
 * @brief Ring buffer FIFO for byte-oriented data.
 * @details Provides a fixed-capacity, growable ring buffer storing @c std::byte.
 *          Supports wrap-aware writes, partial/full reads, zero-copy fast path
 *          for contiguous full reads, and memory policy to restore initial
 *          capacity on clear. Designed for efficient buffering in memory-bound
 *          workflows where minimal copies are desired.
 */
#pragma once

#include <StormByte/memory/visibility.h>

#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <vector>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for managing different type of buffers 
 */
namespace StormByte::Memory::Buffer {
	/**
	 * @class FIFO
	 * @brief Byte-oriented ring buffer with grow-on-demand.
	 * @par Overview
	 *  A circular buffer implemented atop @c std::vector<std::byte> that tracks
	 *  head/tail indices and current size. It grows geometrically to fit writes
	 *  and supports efficient reads even across wrap boundaries.
	 * @par Memory behavior
	 *  The constructor-requested capacity is remembered and restored by @ref Clear().
	 *  When empty, an rvalue @ref Write(std::vector<std::byte>&&) adopts storage
	 *  wholesale to avoid copies. Reading the entire content when it is contiguous
	 *  (head == 0) uses a zero-copy fast path via move.
	 */
	class STORMBYTE_MEMORY_PUBLIC FIFO {
		public:
			/**
			 * 	@brief Construct FIFO with optional initial capacity.
			 *  @param capacity Initial number of slots to allocate; 0 leaves empty.
			 */
			explicit FIFO(std::size_t capacity = 0) noexcept;

			/**
			 * 	@brief Copy construct, preserving buffer state and initial capacity.
			 *  @param other Source FIFO to copy from.
			 */
			FIFO(const FIFO& other) noexcept;

			/**
			 * 	@brief Convenience write from string (bytes copied from string data).
			 *  @param data String whose bytes will be written into the FIFO.
			 */
			void Write(const std::string& data);
			
			/**
			 * 	@brief Move construct, preserving buffer state and initial capacity.
			 *  @param other Source FIFO to move from; left empty after move.
			 */
			FIFO(FIFO&& other) noexcept;

			/**
			 * 	@brief Virtual destructor.
			 */
			virtual ~FIFO() = default;
			
			/**
			 * 	@brief Copy assign, preserving buffer state and initial capacity.
			 *  @param other Source FIFO to copy from.
			 *  @return Reference to this FIFO.
			 */
			FIFO& operator=(const FIFO& other) noexcept;

			/**
			 * 	@brief Move assign, preserving buffer state and initial capacity.
			 *  @param other Source FIFO to move from; left empty after move.
			 *  @return Reference to this FIFO.
			 */
			FIFO& operator=(FIFO&& other) noexcept;

			/**
			 * @brief Current number of bytes stored.
			 */
			inline std::size_t Size() const noexcept { return m_size; }
			
			/**
			 * @brief Current capacity (number of slots in the buffer).
			 */
			inline std::size_t Capacity() const noexcept { return m_buffer.size(); }

			/**
			 * @brief Whether the buffer has no data.
			 */
			inline bool Empty() const noexcept { return m_size == 0; }

			/**
			 * @brief Whether the buffer is full (non-zero capacity and size == capacity).
			 */
			inline bool Full() const noexcept { return m_size == m_buffer.size() && m_buffer.size() != 0; }

			/**
			 * @brief Clear contents and restore capacity to the constructor-requested value.
			 */
			void Clear() noexcept;

			/**
			 * 	@brief Ensure capacity is at least @p newCapacity; may relinearize.
			 *  @param newCapacity Minimum capacity requested.
			 */
			void Reserve(std::size_t newCapacity);

			/**
			 * 	@brief Write bytes from a vector; grows if needed, handles wrap.
			 *  @param data Byte vector to append to the FIFO.
			 */
			void Write(const std::vector<std::byte>& data);
			
			/**
			 * 	@brief Write bytes from an rvalue vector; adopts storage when empty.
			 *  @param data Byte vector to append; may be moved into the FIFO.
			 */
			void Write(std::vector<std::byte>&& data) noexcept;

			/**
			 * 	@brief Read up to @p count bytes (or all if count == 0).
			 *  @param count Number of bytes to read; 0 reads all available.
			 *  @return A vector containing the requested bytes.
			 *  @note Reading all when contiguous (head == 0) returns bytes via move.
			 */
			std::vector<std::byte> Read(std::size_t count = 0);

		protected:
			/**
			 * @brief Underlying contiguous storage backing the ring buffer.
			 */
			std::vector<std::byte> m_buffer;

			/**
			 * @brief Initial capacity requested by the constructor, restored on clear.
			 */
			std::size_t m_initialCapacity {0};

			/**
			 * @brief Index of the logical head (read position).
			 */
			std::size_t m_head {0};

			/**
			 * @brief Index of the logical tail (write position).
			 */
			std::size_t m_tail {0};

			/**
			 * @brief Number of bytes currently stored.
			 */
			std::size_t m_size {0};

		private:
			/**
			 * 	@brief Grow capacity geometrically to fit @p required bytes.
			 *  @param required Total number of bytes that must fit after growth.
			 */
			void GrowToFit(std::size_t required);
			
			/**
			 * 	@brief Copy logical contents into @p dst starting at index 0.
			 *  @param dst Destination buffer that receives the linearized contents.
			 */
			void RelinearizeInto(std::vector<std::byte>& dst) const;
	};
}