#pragma once

#include <StormByte/buffer/position.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <atomic>
#include <concepts>
#include <ranges>
#include <string>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for different buffer types
 * including FIFO ring buffers, thread-safe shared buffers, and producer-consumer patterns.
 */
namespace StormByte::Buffer {
	/**
	 * @class FIFO
	 * @brief Byte-oriented ring buffer with grow-on-demand.
	 *
	 * @par Overview
	 *  A circular buffer implemented atop @c std::vector<std::byte> that tracks
	 *  head/tail indices and current size. It grows geometrically to fit writes
	 *  and supports efficient reads even across wrap boundaries.
	 *
	 * @par Thread safety
	 *  This class is **not thread-safe**. For concurrent access, use @ref SharedFIFO.
	 *
	 * @par Buffer behavior
	 *  The constructor-requested capacity is remembered and restored by @ref Clear().
	 *  When empty, an rvalue write adopts storage wholesale to avoid copies.
	 *  Reading the entire content when it is contiguous (head == 0) uses a
	 *  zero-copy fast path via move semantics.
	 *
	 * @see SharedFIFO for thread-safe version
	 * @see Producer and Consumer for higher-level producer-consumer pattern
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
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Capacity(), Empty()
			 */
			inline std::size_t Size() const noexcept { return m_size; }
			
			/**
			 * @brief Get the current allocated capacity.
			 * @return The number of bytes allocated in the underlying buffer.
			 * @details Capacity may be larger than Size(). The buffer grows automatically
			 *          when needed.
			 * @see Size(), Reserve()
			 */
			virtual std::size_t Capacity() const noexcept;

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size()
			 */
			inline bool Empty() const noexcept { return m_size == 0; }

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions,
			 *          and restores capacity to the initial value requested in the constructor.
			 * @see Size(), Empty()
			 */
			virtual void Clear() noexcept;

			/**
			 * @brief Close the FIFO for further writes.
			 * @details Marks the buffer as closed. Subsequent Write() calls will be ignored.
			 *          For SharedFIFO, also notifies waiting readers.
			 * @see IsClosed(), SharedFIFO::Close()
			 */
			inline void Close() noexcept { m_closed.store(true); }

			/**
			 * @brief Pre-allocate buffer capacity.
			 * @param newCapacity Minimum capacity to ensure (in bytes).
			 * @details Ensures the buffer can hold at least newCapacity bytes without
			 *          reallocation. May relinearize the ring buffer if needed.
			 * @see Capacity()
			 */
			virtual void Reserve(std::size_t newCapacity);

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual bool Write(const std::vector<std::byte>& data);

			/**
			 * @brief Write a string to the buffer.
			 * @param data String whose bytes will be written into the FIFO.
			 * @details Convenience method that converts the string to bytes and appends
			 *          to the buffer. Equivalent to Write(std::vector<std::byte>).
			 * @see Write(const std::vector<std::byte>&)
			 */
			bool Write(const std::string& data);

			/**
			 * @brief Non-destructive read from the buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @return A vector containing the requested bytes.
			 * @details Non-destructive operation - data remains in the buffer and can be
			 *          read again using Seek(). The read position advances by the number
			 *          of bytes read. Returns fewer bytes if insufficient data available.
			 * @note This class is not thread-safe. For blocking behavior, see SharedFIFO::Read().
			 * @see Extract(), Seek(), SharedFIFO::Read()
			 */
			virtual ExpectedData<InsufficientData> Read(std::size_t count = 0);

			/**
			 * @brief Destructive read that removes data from the buffer.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @return A vector containing the extracted bytes.
			 * @details Removes data from the buffer, advancing the head and decreasing size.
			 *          The read position is adjusted. Uses zero-copy move semantics when
			 *          extracting all contiguous data (optimization).
			 * @note This class is not thread-safe. For blocking behavior, see SharedFIFO::Extract().
			 * @see Read(), SharedFIFO::Extract()
			 */
			virtual ExpectedData<InsufficientData> Extract(std::size_t count = 0);

			/**
			 * @brief Check if the buffer is closed for further writes.
			 * @return true if closed, false otherwise.
			 * @see Close()
			 */
			inline bool IsClosed() const noexcept { return m_closed.load(); }

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Position::Absolute (from start) or Position::Relative (from current).
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
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