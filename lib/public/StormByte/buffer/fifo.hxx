#pragma once

#include <StormByte/buffer/position.hxx>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <vector>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for diferent buffers
 */
namespace StormByte::Buffer {
	/**
	 * @class FIFO
	 * @brief Byte-oriented ring buffer with grow-on-demand.
	 * @par Overview
	 *  A circular buffer implemented atop @c std::vector<std::byte> that tracks
	 *  head/tail indices and current size. It grows geometrically to fit writes
	 *  and supports efficient reads even across wrap boundaries.
	 * @par Buffer behavior
	 *  The constructor-requested capacity is remembered and restored by @ref Clear().
	 *  When empty, an rvalue @ref Write(std::vector<std::byte>&&) adopts storage
	 *  wholesale to avoid copies. Reading the entire content when it is contiguous
	 *  (head == 0) uses a zero-copy fast path via move.
	 */
	class STORMBYTE_BUFFER_PUBLIC FIFO {
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
			virtual std::size_t Capacity() const noexcept;

			/**
			 * @brief Whether the buffer has no data.
			 */
			inline bool Empty() const noexcept { return m_size == 0; }

			/**
			 * @brief Clear contents and restore capacity to the constructor-requested value.
			 */
			virtual void Clear() noexcept;

			/**
			 * @brief Close the FIFO for further writes
			 */
			inline void Close() noexcept { m_closed.store(true); }

			/**
			 * 	@brief Ensure capacity is at least @p newCapacity; may relinearize.
			 *  @param newCapacity Minimum capacity requested.
			 */
			virtual void Reserve(std::size_t newCapacity);

			/**
			 * 	@brief Write bytes from a vector; grows if needed, handles wrap.
			 *  @param data Byte vector to append to the FIFO.
			 */
			virtual void Write(const std::vector<std::byte>& data);
			
			/**
			 * 	@brief Write bytes from an rvalue vector; adopts storage when empty.
			 *  @param data Byte vector to append; may be moved into the FIFO.
			 */
			virtual void Write(std::vector<std::byte>&& data) noexcept;

			/**
			 * 	@brief Convenience write from string (bytes copied from string data).
			 *  @param data String whose bytes will be written into the FIFO.
			 */
			virtual void Write(const std::string& data);

			/**
			 * 	@brief Non-destructive read up to @p count bytes from current read position.
			 *  @param count Number of bytes to read; 0 reads all available from read position.
			 *  @return A vector containing the requested bytes.
			 *  @note This operation is non-destructive. Data remains in the FIFO and can be
			 *        read again by resetting the read position. The internal read position
			 *        is advanced by the number of bytes read. Use @ref Extract() for
			 *        destructive reads that remove data from the buffer.
			 *  @see Extract(), ResetReadPosition()
			 */
			virtual std::vector<std::byte> Read(std::size_t count = 0);

			/**
			 * 	@brief Extract up to @p count bytes (destructive read).
			 *  @param count Number of bytes to extract; 0 extracts all available.
			 *  @return A vector containing the extracted bytes.
			 *  @note This operation removes data from the FIFO, advancing the head pointer
			 *        and decreasing size. The read position is adjusted accordingly.
			 *        When extracting all data and it's contiguous (head == 0), uses
			 *        zero-copy move semantics.
			 *  @see Read()
			 */
			virtual std::vector<std::byte> Extract(std::size_t count = 0);

			/**
			 * @brief Whether the FIFO is closed for further writes.
			 * @return true if the FIFO is closed, false otherwise.
			 */
			inline bool IsClosed() const noexcept { return m_closed.load(); }

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The position offset to apply.
			 * @param mode Positioning mode: Position::Absolute sets the read position to
			 *             an absolute offset from the head, Position::Relative adjusts
			 *             the read position by the given offset from the current position.
			 * @note In Absolute mode, the position is clamped to the range [0, Size()].
			 *       In Relative mode, the resulting position is clamped to valid bounds.
			 *       Seeking does not affect the data stored in the FIFO, only the position
			 *       from which subsequent @ref Read() operations will start.
			 * @see Read()
			 */
			virtual void Seek(const std::size_t& position, const Position& mode);

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
			std::atomic<std::size_t> m_size {0};

			/**
			 * @brief Whether the FIFO is closed for further writes.
			 */
			std::atomic<bool> m_closed {false};

			/**
			 * @brief Current read position for non-destructive reads.
			 *
			 * Tracks the offset from the head for @ref Read() operations.
			 * This position is relative to the current head and is automatically
			 * adjusted when data is extracted via @ref Extract().
			 */
			std::atomic<std::size_t> m_read_position {0};

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