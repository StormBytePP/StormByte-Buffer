#pragma once

#include <StormByte/buffer/position.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <deque>
#include <string>
#include <utility>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, and producer-consumer patterns.
 */
namespace StormByte::Buffer {
	/**
	 * @class FIFO
	* @brief Byte-oriented FIFO buffer with grow-on-demand.
	 *
	 * @par Overview
	*  A contiguous growable buffer implemented atop @c std::deque<std::byte> that tracks
	*  a logical read position. It grows automatically to fit writes and supports
	*  efficient non-destructive reads and destructive extracts.
	 *
	 * @par Thread safety
	 *  This class is **not thread-safe**. For concurrent access, use @ref SharedFIFO.
	 *
	 * @par Buffer behavior
	*  The buffer supports clearing and cleaning operations, a movable read position
	*  for non-destructive reads, and a closed state to signal end-of-writes.
	 *
	 * @see SharedFIFO for thread-safe version
	 * @see Producer and Consumer for higher-level producer-consumer pattern
	 */
	class STORMBYTE_BUFFER_PUBLIC FIFO {
		public:
			/**
			 * 	@brief Construct FIFO.
			 */
			explicit FIFO() noexcept;

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
			virtual ~FIFO();
			
			/**
			 * 	@brief Copy assign, preserving buffer state and initial capacity.
			 *  @param other Source FIFO to copy from.
			 *  @return Reference to this FIFO.
			 */
			FIFO& operator=(const FIFO& other);

			/**
			 * 	@brief Move assign, preserving buffer state and initial capacity.
			 *  @param other Source FIFO to move from; left empty after move.
			 *  @return Reference to this FIFO.
			 */
			FIFO& operator=(FIFO&& other) noexcept;

			// Two buffers are considered equal if they have the same content
			inline bool operator==(const FIFO& other) const noexcept {
				return m_buffer == other.m_buffer;
			}

			//
			inline bool operator!=(const FIFO& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Get the number of bytes available for non-destructive reading.
			 * @return The number of bytes that can be read from the current read position
			 *         without blocking or advancing the buffer.
			 * @details Returns the difference between the total buffer size and the current
			 *          read position. This is the amount of data available for Read() operations
			 *          starting from the current read position. Does not include data that has
			 *          already been read past via previous Read() calls unless Seek() is used
			 *          to reposition.
			 * @see Size(), Read(), Seek()
			 */
			virtual std::size_t AvailableBytes() const noexcept;

			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Capacity(), Empty()
			 */
			virtual std::size_t Size() const noexcept;

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size()
			 */
			virtual bool Empty() const noexcept;

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions,
			 *          and restores capacity to the initial value requested in the constructor.
			 * @see Size(), Empty()
			 */
			virtual void Clear() noexcept;

			/**
			 * @brief Clean buffer data (from start to readposition)
			 */
			virtual void Clean() noexcept;

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
			virtual bool Write(const std::string& data);

			/**
			 * @brief Non-destructive read from the buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @return A vector containing the requested bytes, or error if insufficient data.
			 * @details Non-destructive operation - data remains in the buffer and can be
			 *          read again using Seek(). The read position advances by the number
			 *          of bytes read.
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call returns all available bytes. If no
			 *            bytes are available, an `InsufficientData` error is returned.
			 *          - If `count > 0`: the call returns exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, an
			 *            `InsufficientData` error is returned.
			 *
			 *          This class is non-thread-safe; callers that require blocking
			 *          behavior or closed/error semantics should use `SharedFIFO`.
			 * @note This class is not thread-safe. For blocking behavior, see SharedFIFO::Read().
			 * @see Extract(), Seek(), SharedFIFO::Read(), IsReadable()
			 */
			virtual ExpectedData<InsufficientData> Read(std::size_t count = 0) const;

			/**
			 * @brief Destructive read that removes data from the buffer.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @return A vector containing the extracted bytes, or error if insufficient data.
			 * @details Removes data from the buffer, advancing the head and decreasing size.
			 *          The read position is adjusted. Uses zero-copy move semantics when
			 *          extracting all contiguous data (optimization).
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call extracts all available bytes. If no
			 *            bytes are available, an `InsufficientData` error is returned.
			 *          - If `count > 0`: the call extracts exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, an
			 *            `InsufficientData` error is returned.
			 *
			 *          This class is non-thread-safe; callers that require blocking
			 *          behavior or closed/error semantics should use `SharedFIFO`.
			 * @note This class is not thread-safe. For blocking behavior, see SharedFIFO::Extract().
			 * @see Read(), SharedFIFO::Extract(), IsReadable()
			 */
			virtual ExpectedData<InsufficientData> Extract(std::size_t count = 0);

			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is unreadable and no bytes available, false otherwise.
			 * @details Returns true when the buffer is unreadable (error state) AND
			 *          AvailableBytes() == 0, indicating no more data can be read.
			 *          This occurs when the buffer is in error state and all data has
			 *          been consumed.
			 * @see AvailableBytes()
			 */
			virtual bool EoF() const noexcept;

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Position::Absolute (from start) or Position::Relative (from current).
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
			 * If Position is set to Absolute and offset is negative the operation is noop
			 */
			virtual void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept;

		protected:
			/**
			 * @brief Internal deque storing the buffer data.
			 */
			std::deque<std::byte> m_buffer;

			/**
			 * @brief Current read position for non-destructive reads.
			 *
			 * Tracks the offset from the start of the buffer for @ref Read() operations.
			 * This position is automatically adjusted when data is extracted via @ref Extract().
			 */
			mutable std::size_t m_position_offset;

		private:
			void Copy(const FIFO& other) noexcept;
	};
}