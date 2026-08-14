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
	* @brief Byte-oriented FIFO buffer with grow-on-demand and close/error support.
	*
	* @par Overview
	*  A contiguous growable buffer implemented atop @c DataType that tracks
	*  a logical read position. It grows automatically to fit writes and supports
	*  efficient non-destructive reads and destructive extracts.
	*
	* @par Thread safety
	*  This class is **not thread-safe**. For concurrent access, use @ref SharedFIFO
	*  or @ref Ring.
	*
	* @par Buffer behavior
	*  The buffer supports clearing and cleaning operations, a movable read position
	*  for non-destructive reads, and a closed/error state to signal end-of-writes
	*  or permanent failure. Once closed, further writes fail; readers can still
	*  drain remaining data until EoF.
	*
	* @see SharedFIFO, Ring, Producer, Consumer
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
			inline FIFO(const DataType& data) noexcept: m_buffer(data), m_position_offset(0) {}

			/**
			 * 	@brief Construct FIFO with initial data using move semantics.
			 *  @param data Initial byte vector to move into the FIFO.
			 */
			inline FIFO(DataType&& data) noexcept: m_buffer(std::move(data)), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from an input range.
			 * @tparam R Input range whose elements are convertible to `std::byte`.
			 * @param r The input range to copy from.
			 * @note This overload is constrained so that it does not participate when
			 *       the argument type is the library `DataType` to avoid ambiguity
			 *       with the existing `DataType` overloads.
			 */
			template<std::ranges::input_range R>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
				requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
				(!std::same_as<std::remove_cvref_t<R>, DataType>)
			inline FIFO(const R& r) noexcept: m_buffer(DataConvert(r)), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from an rvalue range (moves when DataType rvalue).
			 * @tparam Rr Input range type; if it's `DataType` this will be moved into
			 *            the internal buffer, otherwise elements are converted.
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline FIFO(Rr&& r) noexcept: m_buffer(DataConvert(std::forward<Rr>(r))), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from a string view (does not include terminating NUL).
			*/
			inline FIFO(std::string_view sv) noexcept: m_buffer(DataConvert(sv)), m_position_offset(0) {}

			/**
			 * @brief Construct FIFO from a C string pointer (null-terminated).
			*/
			inline FIFO(const char* s) noexcept: FIFO(s ? std::string_view(s) : std::string_view()) {}

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
			 * Compares this `FIFO` with `other` by comparing their internal buffers,
			 * read position and closed/error state.
			 */
			inline bool operator==(const FIFO& other) const noexcept {
				return m_buffer == other.m_buffer &&
					m_position_offset == other.m_position_offset &&
					m_closed == other.m_closed &&
					m_error == other.m_error;
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
			 * @brief Clean buffer data (from start to read position)
			 */
			virtual void 											Clean() noexcept override;

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions.
			 *          Does **not** clear the closed/error flags.
			 * @see Size(), Empty()
			 */
			inline virtual void 									Clear() noexcept override {
				m_buffer.clear();
				m_position_offset = 0;
			}

			/**
			 * @brief Mark the buffer closed for further writes.
			 * @details Subsequent Write() calls will fail. Readers can still
			 *          consume remaining data until AvailableBytes() becomes zero.
			 */
			inline void 											Close() noexcept override {
				m_closed = true;
			}

			/**
			 * @brief Access the internal data buffer.
			 * @return Constant reference to the internal DataType buffer.
			 */
			inline virtual const DataType& 							Data() const noexcept override {
				return m_buffer;
			}

			/**
			 * @brief Drop bytes in the buffer and updates read position.
			 * @param count Number of bytes to drop.
			 * @return bool indicating success or failure.
			 * @see Read(), Seek()
			 */
			virtual bool 											Drop(const std::size_t& count) noexcept override;

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
			inline virtual bool EoF() const noexcept override {
				const std::size_t avail =
					(m_position_offset <= m_buffer.size())
						? (m_buffer.size() - m_position_offset)
						: 0;
				return m_error || (m_closed && avail == 0);
			}

			/**
			 * @brief Destructive read that removes data from the buffer into an existing vector.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer Vector to fill with extracted bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 											Extract(const std::size_t& count, DataType& outBuffer) noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Extract);
			}

			/**
			 * @brief Destructive read that removes data from the buffer into a FIFO.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer WriteOnly to fill with extracted bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 											Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Extract);
			}

			/** Expose the rest of overloads */
			using ReadOnly::Extract;

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
			 */
			inline void												ExtractUntilEoF(WriteOnly& outBuffer) noexcept override {
				ReadUntilEoFInternal(outBuffer, Operation::Extract);
			}

			/**
			 * @brief Produce a hexdump of the unread contents starting at the current read position.
			 * @param collumns Number of bytes per line (0 -> default 16).
			 * @param byte_limit Maximum number of bytes to include (0 -> no limit).
			 * @return A formatted string that begins with size / position / status followed by
			 *         the hex/ASCII lines. The returned string does not include a trailing
			 *         newline.
			 */
			virtual std::string										HexDump(const std::size_t& collumns = 16, const std::size_t& byte_limit = 0) const noexcept;

			/**
			 * @brief Check if the buffer is readable.
			 * @return false when the buffer is in a permanent error state.
			 */
			inline virtual bool 									IsReadable() const noexcept override {
				return !m_error;
			}

			/**
			 * @brief Check if the buffer is writable.
			 * @return false when the buffer has been closed or is in error state.
			 */
			inline virtual bool										IsWritable() const noexcept override {
				return !m_closed && !m_error;
			}

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return bool indicating success or failure.
			 */
			inline bool 											Peek(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
			}

			/**
			 * @brief Non-destructive peek at buffer data without advancing read position.
			 * @param count Number of bytes to peek; 0 peeks all available from read position.
			 * @return bool indicating success or failure.
			 */
			inline bool 											Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Peek);
			}

			/**
			 * @brief Non destructive read that advances the read position.
			 * @param count Number of bytes to read; 0 reads all available.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 											Read(const std::size_t& count, DataType& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Read);
			}

			/**
			 * @brief Non destructive read that advances the read position.
			 * @param count Number of bytes to read; 0 reads all available.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return bool indicating success or failure.
			 */
			inline bool 											Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override {
				return const_cast<FIFO*>(this)->ReadInternal(count, outBuffer, Operation::Read);
			}

			/** Expose the rest of overloads */
			using ReadOnly::Read;

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
			 */
			inline void												ReadUntilEoF(WriteOnly& outBuffer) const noexcept override {
				const_cast<FIFO*>(this)->ReadUntilEoFInternal(outBuffer, Operation::Read);
			}

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param offset The offset value to apply.
			 * @param mode Absolute or Relative.
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 */
			virtual void 											Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Put the buffer into the permanent error state.
			 * @details Makes the buffer both unreadable and unwritable.
			 */
			inline void 											SetError() noexcept override {
				m_error = true;
			}

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
			 * @param data Byte vector to append.
			 * @return bool indicating success or failure (fails if closed/error).
			 */
			inline bool 											Write(const std::size_t& count, const DataType& data) noexcept override {
				return WriteInternal(count, data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append.
			 * @return bool indicating success or failure (fails if closed/error).
			 */
			inline bool	 											Write(const std::size_t& count, DataType&& data) noexcept override {
				return WriteInternal(count, std::move(data));
			}

			/**
			 * @brief Write bytes from a ReadOnly source.
			 * @param count Number of bytes to write.
			 * @param data Source buffer.
			 * @return bool indicating success or failure (fails if closed/error).
			 */
			inline bool 											Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return WriteInternal(count, data);
			}

			/**
			 * @brief Move bytes from a ReadOnly source.
			 * @param count Number of bytes to write.
			 * @param data Source buffer.
			 * @return bool indicating success or failure (fails if closed/error).
			 */
			inline bool 											Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return WriteInternal(count, std::move(data));
			}

			/** Expose the rest of overloads */
			using WriteOnly::Write;

		protected:
			/**
			 * @brief The owned contiguous buffer storage.
			 */
			DataType m_buffer;

			/**
			 * @brief Current read position for read operations.
			 *
			 * Tracks the offset from the start of the buffer for all read operations.
			 */
			mutable std::size_t m_position_offset {0};

			/**
			 * @brief Closed flag – once true, further writes fail.
			 */
			bool m_closed {false};

			/**
			 * @brief Permanent error flag – makes the buffer unreadable and unwritable.
			 */
			bool m_error {false};

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
			 */
			static std::string 											FormatHexLines(std::span<const std::byte>& data, std::size_t start_offset, std::size_t collumns) noexcept;

			/**
			 * @brief Produce a hexdump header with size, read position and status.
			 */
			virtual std::ostringstream 									HexDumpHeader() const noexcept;

			/**
			 * @brief Internal helper for read operations.
			 */
			virtual bool 												ReadInternal(const std::size_t& count, DataType& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for read operations into a WriteOnly.
			 */
			virtual bool 												ReadInternal(const std::size_t& count, WriteOnly& outBuffer, const Operation& flag) noexcept;
			
			/**
			 * @brief Internal helper for reading until end-of-file.
			 */
			virtual void 												ReadUntilEoFInternal(DataType& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for reading until end-of-file into a WriteOnly.
			 */
			virtual void 												ReadUntilEoFInternal(WriteOnly& outBuffer, const Operation& flag) noexcept;

			/**
			 * @brief Internal helper for write operations (copy).
			 */
			virtual bool 												WriteInternal(const std::size_t& count, const DataType& src) noexcept;
			
			/**
			 * @brief Internal helper for write operations (move).
			 */
			virtual bool 												WriteInternal(const std::size_t& count, DataType&& src) noexcept;

			/**
			 * @brief Internal helper for write operations from ReadOnly.
			 */
			virtual bool 												WriteInternal(const std::size_t& count, const ReadOnly& src) noexcept;

			/**
			 * @brief Internal helper for write operations from ReadOnly (move/extract).
			 */
			virtual bool 												WriteInternal(const std::size_t& count, ReadOnly&& src) noexcept;
	};
}
