#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/string.hxx>

#include <vector>
#include <span>
#include <string>
#include <sstream>
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
	*  A contiguous growable buffer implemented atop @c DataType that tracks
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
	class STORMBYTE_BUFFER_PUBLIC FIFO: public ReadWrite {
		public:
			/**
			 * 	@brief Construct FIFO.
			 */
			FIFO() noexcept 										= default;

			/**
			 * 	@brief Construct FIFO with initial data.
			 *  @param data Initial byte vector to populate the FIFO.
			 */
			inline FIFO(const DataType& data) noexcept: ReadWrite(), m_buffer(data), m_position_offset(0) {}

			/**
			 * 	@brief Construct FIFO with initial data using move semantics.
			 *  @param data Initial byte vector to move into the FIFO.
			 */
			inline FIFO(DataType&& data) noexcept: ReadWrite(), m_buffer(std::move(data)), m_position_offset(0) {}

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
			virtual ~FIFO() noexcept 								= default;
			
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

			/**
			 * @brief Equality comparison.
			 *
			 * Compares this `FIFO` with `other` by comparing their internal buffers.
			 * Equality is defined as having identical byte contents in the buffer
			 * and same read position.
			 */
			inline bool operator==(const FIFO& other) const noexcept {
				return m_buffer == other.m_buffer && m_position_offset == other.m_position_offset;
			}

			/**
			 * @brief Inequality comparison.
			 *
			 * Negates `operator==`.
			 */
			inline bool operator!=(const FIFO& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Get the number of bytes available for reading.
			 * @return The number of bytes that can be read from the current read position
			 * @see Size(), Read(), Extract() Seek()
			 */
			inline virtual std::size_t 								AvailableBytes() const noexcept {
				const std::size_t current_size = m_buffer.size();
				return (m_position_offset <= current_size) ? (current_size - m_position_offset) : 0;
			}

			/**
			 * @brief Clean buffer data (from start to readposition)
			 */
			virtual void 											Clean() noexcept override;

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions,
			 *          and restores capacity to the initial value requested in the constructor.
			 * @see Size(), Empty()
			 */
			inline virtual void 									Clear() noexcept override {
				m_buffer.clear();
				m_position_offset = 0;
			}

			/**
			 * @brief Drop bytes in the buffer and updates read position.
			 * @param count Number of bytes to drop.
			 * @see Read(), Seek()
			 */
			virtual ExpectedVoid<WriteError>						Drop(const std::size_t& count) noexcept override;

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size(), AvailableBytes()
			 * @note Since buffer works with read positions, Empty() might return false
			 * 	   even if there is no unread data (i.e., when read position is at the end of the buffer).
			 */
			inline virtual bool 									Empty() const noexcept override {
				return m_buffer.empty();
			}

			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			inline virtual bool 									EoF() const noexcept override {
				return AvailableBytes() == 0;
			}

			/**
			 * @brief Destructive read that removes data from the buffer into an existing vector.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer Vector to fill with extracted bytes; resized as needed.
			 * @return ExpectedVoid<ReadError> indicating success or failure.
			 */
			inline ExpectedVoid<ReadError> 							Extract(const std::size_t& count, DataType& outBuffer) noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Extract);
			}

			/**
			 * @brief Destructive read that removes data from the buffer into a FIFO.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer WriteOnly to fill with extracted bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			inline ExpectedVoid<Error> 								Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Extract);
			}

			/**
			 * @brief Read all bytes until end-of-file into an existing buffer.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 */
			inline void												ExtractUntilEoF(DataType& outBuffer) noexcept override {
				ReadUntilEoFInternal(outBuffer, Operation::Extract);
			}

			/**
			 * @brief Read all bytes until end-of-file into a WriteOnly buffer.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			inline ExpectedVoid<Error>								ExtractUntilEoF(WriteOnly& outBuffer) noexcept override {
				return ReadUntilEoFInternal(outBuffer, Operation::Extract);
			}

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
			 * Example output:
			 * @code{.text}
			 * Size: 13 bytes
			 * Read Position: 0
			 * 00000000: 48 65 6C 6C 6F 2C 20 77 6F 72 6C 64 21           Hello, world!
			 * @endcode
			 */
			virtual std::string										HexDump(const std::size_t& collumns = 16, const std::size_t& byte_limit = 0) const noexcept;

			/**
			 * @brief Check if the buffer is readable.
			 * @return true if the buffer can be read from, false otherwise.
			 */
			inline virtual bool 									IsReadable() const noexcept override {
				return true;
			}

			/**
			 * @brief Check if the buffer is writable.
			 * @return true if the buffer can accept write operations, false otherwise.
			 */
			inline virtual bool										IsWritable() const noexcept override {
				return true;
			}

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
			inline ExpectedVoid<ReadError> 							Peek(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
			}

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
			inline ExpectedVoid<Error> 								Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
			}

			/**
			 * @brief Non destructive read that removes data from the buffer into an existing vector.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer Vector to fill with extracted bytes; resized as needed.
			 * @return ExpectedVoid<ReadError> indicating success or failure.
			 */
			inline ExpectedVoid<ReadError> 							Read(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Read);
			}

			/**
			 * @brief Destructive read that removes data from the buffer into a vector.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer WriteOnly to fill with extracted bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			inline ExpectedVoid<Error> 								Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Read);
			}

			/**
			 * @brief Read all bytes until end-of-file into an existing buffer.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 */
			inline void												ReadUntilEoF(DataType& outBuffer) const noexcept override {
				const_cast<FIFO*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
			}

			/**
			 * @brief Read all bytes until end-of-file into a WriteOnly buffer.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			inline ExpectedVoid<Error>								ReadUntilEoF(WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
			}

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Unused for base class; included for API consistency.
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
			 * If Position is set to Absolute and offset is negative the operation is noop
			 */
			virtual void 											Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Empty(), AvailableBytes()
			 */
			inline virtual std::size_t 								Size() const noexcept override {
				return m_buffer.size();
			}

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<WriteError> 						Write(const std::size_t& count, const DataType& data) noexcept override {
				return WriteInternal(count, data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<WriteError> 						Write(const std::size_t& count, DataType&& data) noexcept override {
				return WriteInternal(count, std::move(data));
			}

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<Error> 								Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return WriteInternal(count, data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<Error> 								Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return WriteInternal(count, std::move(data));
			}

			using WriteOnly::Write;

		protected:
			/**
			 * @brief Internal vector storing the buffer data.
			 */
			DataType m_buffer;

			/**
			 * @brief Current read position for read operations.
			 *
			 * Tracks the offset from the start of the buffer for all read operations.
			 */
			mutable std::size_t m_position_offset {0};

			/**
			 * @brief Enumeration of read operation types.
			 */
			enum class Operation {
				Extract,												///< Destructive read
				Read,													///< Non-destructive read
				Peek													///< Non-destructive peek
			};

			/**
			 * @brief Produce a hexdump of the given data span.
			 * @param data Span of bytes to format.
			 * @param start_offset Starting offset for line addresses.
			 * @param collumns Number of bytes per line.
			 * @return Formatted hexdump string.
			 * @details Formats the provided byte span into a hexdump string with
			 *          specified number of columns per line. Each line begins with
			 *          the absolute offset (from `start_offset`), followed by
			 *          hexadecimal byte values and ASCII representation.
			 */
			static std::string 											FormatHexLines(std::span<const std::byte>& data, std::size_t start_offset, std::size_t collumns) noexcept;

			/**
			 * @brief Produce a hexdump header with size and read position.
			 * @return ostringstream containing the hexdump header.
			 */
			virtual std::ostringstream 									HexDumpHeader() const noexcept;

			/**
			 * @brief Internal helper for read operations.
			 * @param count Number of bytes to read.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Additional flag for read operation (1: copy, 2: move)
			 * @return `ExpectedVoid<ReadError>` indicating success or failure.
			 * @details Shared logic for read operations that first checks the internal
			 *          buffer, then calls the external read function if needed.
			 */
			virtual ExpectedVoid<ReadError> 							ReadInternal(const std::size_t& count, DataType& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for read operations.
			 * @param count Number of bytes to read.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Additional flag for read operation (1: copy, 2: move)
			 * @return `ExpectedVoid<Error>` indicating success or failure.
			 * @details Shared logic for read operations that first checks the internal
			 *          buffer, then calls the external read function if needed.
			 */
			virtual ExpectedVoid<Error> 								ReadInternal(const std::size_t& count, WriteOnly& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for reading until end-of-file.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Additional flag for read operation (1: copy, 2: move)
			 */
			virtual void 												ReadUntilEoFInternal(DataType& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for reading until end-of-file.
			 * @param outBuffer Output buffer to store read bytes.
			 * @param flag Additional flag for read operation (1: copy, 2: move)
			 * @return `ExpectedVoid<Error>` indicating success or failure.
			 */
			virtual ExpectedVoid<Error> 								ReadUntilEoFInternal(WriteOnly& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for write operations.
			 * @param dst Destination buffer to write into.
			 * @param count Number of bytes to write.
			 * @param src Source buffer to write from.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
			 */
			virtual ExpectedVoid<WriteError> 							WriteInternal(const std::size_t& count, const DataType& src) noexcept;

			/**
			 * @brief Internal helper for write operations.
			 * @param dst Destination buffer to write into.
			 * @param count Number of bytes to write.
			 * @param src Source buffer to write from.
			 * @return `ExpectedVoid<WriteError>` indicating success or failure.
			 */
			virtual ExpectedVoid<WriteError> 							WriteInternal(const std::size_t& count, DataType&& src) noexcept;

			/**
			 * @brief Internal helper for write operations.
			 * @param count Number of bytes to write.
			 * @param src Source ReadOnly buffer to write from.
			 * @return `ExpectedVoid<Error>` indicating success or failure.
			 */
			virtual ExpectedVoid<Error> 								WriteInternal(const std::size_t& count, const ReadOnly& src) noexcept;

			/**
			 * @brief Internal helper for write operations.
			 * @param count Number of bytes to write.
			 * @param src Source ReadOnly buffer to write from.
			 * @return `ExpectedVoid<Error>` indicating success or failure.
			 */
			virtual ExpectedVoid<Error> 								WriteInternal(const std::size_t& count, ReadOnly&& src) noexcept;
	};
}