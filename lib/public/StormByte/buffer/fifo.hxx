#pragma once

#include <StormByte/buffer/position.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <vector>
#include <string>
#include <utility>
#include <span>

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
	*  A contiguous growable buffer implemented atop @c std::vector<std::byte> that tracks
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
			FIFO() noexcept;

			/**
			 * 	@brief Construct FIFO with initial data.
			 *  @param data Initial byte vector to populate the FIFO.
			 */
			FIFO(const std::vector<std::byte>& data) noexcept;

			/**
			 * 	@brief Construct FIFO with initial data using move semantics.
			 *  @param data Initial byte vector to move into the FIFO.
			 */
			FIFO(std::vector<std::byte>&& data) noexcept;

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
			 * @brief Non-destructive read from the buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error if insufficient data.
			 * @details Non-destructive operation - data remains in the buffer and can be
			 *          read again using Seek(). The read position advances by the number
			 *          of bytes read.
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call returns all available bytes. If no
			 *            bytes are available, a `ReadError` is returned.
			 *          - If `count > 0`: the call returns exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, a
			 *            `ReadError` is returned.
			 *
			 *          This class is non-thread-safe; callers that require blocking
			 *          behavior or closed/error semantics should use `SharedFIFO`.
			 * @note This class is not thread-safe. For blocking behavior, see SharedFIFO::Read().
			 * @see Extract(), Seek(), SharedFIFO::Read(), IsReadable()
			 */
			virtual ExpectedData<ReadError>Read(std::size_t count = 0) const;

			/**
			 * @brief Zero-copy read that returns a view over the buffer data.
			 * @param count Number of bytes to read; 0 reads all available.
			 * @return std::span<const std::byte> view over the requested bytes, or empty span if error.
			 * @details Returns a non-owning view (std::span) over the buffer data without
			 *          copying. This is more efficient than Read() when you don't need to
			 *          own the data. The span is valid until the next non-const operation
			 *          (Write, Extract, Clear, etc.) on the FIFO.
			 *          
			 *          The read position is advanced just like Read().
			 *          
			 *          Returns empty span on error (insufficient data or buffer empty).
			 * @warning The returned span becomes invalid after any modifying operation.
			 * @see Read(), Peek()
			 */
			virtual ExpectedSpan<ReadError> Span(std::size_t count = 0) const noexcept;

			/**
			 * @brief Destructive read that removes data from the buffer.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error if insufficient data.
			 * @details Removes data from the buffer, advancing the head and decreasing size.
			 *          The read position is adjusted. Uses zero-copy move semantics when
			 *          extracting all contiguous data (optimization).
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call extracts all available bytes. If no
			 *            bytes are available, a `ReadError` is returned.
			 *          - If `count > 0`: the call extracts exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, a
			 *            `ReadError` is returned.
			 *
			 *          This class is non-thread-safe; callers that require blocking
			 *          behavior or closed/error semantics should use `SharedFIFO`.
			 * @note This class is not thread-safe. For blocking behavior, see SharedFIFO::Extract().
			 * @see Read(), SharedFIFO::Extract(), IsReadable()
			 */
			virtual ExpectedData<ReadError> Extract(std::size_t count = 0);

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<WriteError> Write(std::span<const std::byte> data);

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<WriteError> Write(const std::vector<std::byte>& data);

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<WriteError> Write(const std::vector<std::byte>&& data);

			/**
			 * @brief Append the full contents of another FIFO to this buffer.
			 * @param other FIFO whose entire stored byte sequence will be appended.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details This overload appends all bytes from `other.m_buffer` (from
			 * its beginning to end). The const-reference overload does not modify
			 * the `other` FIFO; it simply copies or inserts the bytes into this
			 * FIFO. Use the rvalue overload to transfer ownership when possible.
			 */

			virtual ExpectedVoid<WriteError> Write(const FIFO& other);

			/**
			 * @brief Append the full contents of an rvalue FIFO into this buffer.
			 * @param other FIFO to move from; its contents will be transferred.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details This rvalue overload will attempt to perform an efficient
			 * move of `other`'s internal storage. If this FIFO is empty it may
			 * steal `other`'s underlying deque (O(1)). Otherwise it will move-insert
			 * the elements from `other` and leave `other` in a valid empty state.
			 * Marked `noexcept` to allow strong exception-safety guarantees for
			 * callers relying on non-throwing move operations.
			 */
			virtual ExpectedVoid<WriteError> Write(FIFO&& other) noexcept;

			/**
			 * @brief Write a string to the buffer.
			 * @param data String whose bytes will be written into the FIFO.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Convenience method that converts the string to bytes and appends
			 *          to the buffer. Equivalent to Write(std::vector<std::byte>).
			 * @see Write(const std::vector<std::byte>&)
			 */
			virtual ExpectedVoid<WriteError> Write(const std::string& data);

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

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return ExpectedData<ReadError> containing the requested bytes, or error if insufficient data.
			 * @details Similar to Read(), but does not advance the read position.
			 *          Allows inspecting upcoming data without consuming it.
			 *
			 *          Semantics:
			 *          - If `count == 0`: the call returns all available bytes. If no
			 *            bytes are available, a `ReadError` is returned.
			 *          - If `count > 0`: the call returns exactly `count` bytes when
			 *            that many bytes are available. If zero bytes are available, or
			 *            if `count` is greater than the number of available bytes, a
			 *            `ReadError` is returned.
			 *
			 * @see Read(), Seek()
			 */
			virtual ExpectedData<ReadError> Peek(std::size_t count = 0) const noexcept;

			// Removes count bytes from the read position
			// A value of 0 is a noop, clamped to AvailableBytes()
			virtual void Skip(const std::size_t& count) noexcept;

			/**
			 * @brief Produce a hexdump of the unread contents starting at the current read position.
			 * @param collumns Number of bytes per line (0 -> default 16).
			 * @param byte_limit Maximum number of bytes to include (0 -> no limit).
			 * @return A formatted string that begins with `Read Position: <offset>` followed by
			 *         the hex/ASCII lines. The returned string does not include a trailing
			 *         newline.
			 * @details The hexdump is produced from a snapshot of the unread bytes and does not
			 *          modify the FIFO's read position. Offsets printed on each line are
			 *          absolute offsets (from the start of the underlying buffer). Formatting
			 *          of the hex/ASCII lines is performed by `FormatHexLines()` to ensure
			 *          consistent output between `FIFO` and `SharedFIFO`.
			 *
			 * @code{.text}
			 * // Example output:
			 * // Read Position: 0
			 * // 00000000: 48 65 6C 6C 6F 2C 20 77 6F 72 6C 64 21           Hello, world!
			 * @endcode
			 */
			virtual std::string HexDump(const std::size_t& collumns = 16, const std::size_t& byte_limit = 0) const noexcept;

			/**
			 * @brief Format a hex dump from an arbitrary byte sequence.
			 * @param data Byte vector to format.
			 * @param start_offset Absolute offset to use for the left-hand offsets.
			 * @param collumns Number of columns per line (defaults to 16 when 0).
			 * @return Formatted hex lines joined by '\n' (no trailing newline).
			 * @note This is a public helper so other components can reuse the same
			 *       formatting logic. It returns only the hex/ASCII block and does
			 *       not include any header lines such as "Read Position".
			 */
			static std::string FormatHexLines(const std::vector<std::byte>& data, std::size_t start_offset, std::size_t collumns = 16) noexcept;

		protected:
			/**
			 * @brief Internal vector storing the buffer data.
			 */
			std::vector<std::byte> m_buffer;

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