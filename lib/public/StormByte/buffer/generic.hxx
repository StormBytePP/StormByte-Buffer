#pragma once

#include <StormByte/buffer/typedefs.hxx>

#include <sstream>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including Generic buffers, thread-safe shared buffers, and producer-consumer patterns.
 */
namespace StormByte::Buffer {
	/**
	* @class Generic
	* @brief Generic class to maintain common API guarantees across buffer types.
	* @par Overview
	*  Generic provides a common base class for buffer types, ensuring consistent
	*  API behavior and guarantees. It defines standard operations like reading,
	*  writing, seeking, and clearing buffers.
	*/
	class STORMBYTE_BUFFER_PUBLIC Generic {
		public:
			/**
			 * 	@brief Construct Generic.
			 */
			Generic() noexcept												= default;

			/**
			 * 	@brief Copy construct deleted
			 */
			Generic(const Generic&) noexcept 								= delete;
			
			/**
			 * 	@brief Move construct deleted
			 */
			Generic(Generic&&) noexcept										= delete;

			/**
			 * 	@brief Virtual destructor.
			 */
			virtual ~Generic() noexcept 									= 0;
			
			/**
			 * 	@brief Copy assign deleted
			 */
			Generic& operator=(const Generic& other) 						= delete;
		
			/**
			 * 	@brief Move assign deleted
			 */
			Generic& operator=(Generic&&) noexcept							= delete;
	};

	class WriteOnly; // Forward declaration

	/**
	 * @class ReadOnly
	 * @brief Generic class providing a buffer that can be read but not written to.
	 * @par Overview
	 *  ReadOnly extends Generic to provide a read-only buffer interface. It allows
	 *  reading data from the buffer while preventing any modifications. This is useful
	 *  for scenarios where data integrity must be maintained and only read access is
	 *  required.
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadOnly: virtual public Generic {
		public:
			/**
			 * 	@brief Construct ReadOnly.
			 */
			inline ReadOnly() noexcept: Generic() {};

			/**
			 * 	@brief Copy construct deleted
			 */
			ReadOnly(const ReadOnly&) noexcept 								= delete;
			
			/**
			 * 	@brief Move construct deleted
			 */
			ReadOnly(ReadOnly&&) noexcept									= delete;

			/**
			 * 	@brief Virtual destructor.
			 */
			virtual ~ReadOnly() noexcept 									= default;
			
			/**
			 * 	@brief Copy assign deleted
			 */
			ReadOnly& operator=(const ReadOnly&) 							= delete;
		
			/**
			 * 	@brief Move assign deleted
			 */
			ReadOnly& operator=(ReadOnly&&) noexcept						= delete;

			/**
			 * @brief Gets available bytes for reading.
			 * @return Number of bytes available from the current read position.
			 */
			virtual std::size_t 											AvailableBytes() const noexcept = 0;

			/**
			 * @brief Clean buffer data from start to read position.
			 * @see Size(), Empty()
			 */
			virtual void 													Clean() noexcept = 0;

			/**
			 * @brief Clear all buffer contents.
			 * @details Removes all data from the buffer, resets head/tail/read positions,
			 *          and restores capacity to the initial value requested in the constructor.
			 * @see Size(), Empty()
			 */
			virtual void 													Clear() noexcept = 0;

			/**
			 * @brief Drop bytes in the buffer
			 * @param count Number of bytes to drop.
			 * @see Read()
			 */
			virtual ExpectedVoid<WriteError> 								Drop(const std::size_t& count) noexcept = 0;

			/**
			 * @brief Check if the buffer is empty.
			 * @return true if the buffer contains no data, false otherwise.
			 * @see Size()
			 */
			virtual bool 													Empty() const noexcept = 0;

			/**
			 * @brief Check if the reader has reached end-of-file.
			 * @return true if buffer is closed or in error state and no bytes available.
			 * @details Returns true when the buffer has been closed or set to error
			 *          and there are no available bytes remaining.
			 */
			virtual bool 													EoF() const noexcept = 0;

			/**
			 * @brief Destructive read that removes data from the buffer into an existing vector.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer Vector to fill with extracted bytes; resized as needed.
			 * @return ExpectedVoid<ReadError> indicating success or failure.
			 * @note For base class is the same than Read
			 */
			inline virtual ExpectedVoid<ReadError> 							Extract(const std::size_t& count, DataType& outBuffer) noexcept = 0;

			/**
			 * @brief Destructive read that removes data from the buffer into a FIFO.
			 * @param count Number of bytes to extract; 0 extracts all available.
			 * @param outBuffer WriteOnly to fill with extracted bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			inline virtual ExpectedVoid<Error> 								Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept = 0;

			/**
			 * @brief Read all bytes until end-of-file into an existing buffer.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 */
			virtual void													ExtractUntilEoF(DataType& outBuffer) noexcept = 0;

			/**
			 * @brief Read all bytes until end-of-file into a WriteOnly buffer.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			virtual ExpectedVoid<Error>										ExtractUntilEoF(WriteOnly& outBuffer) noexcept = 0;

			/**
			 * @brief Check if the buffer is readable.
			 * @return true if the buffer can be read from, false otherwise.
			 */
			virtual bool 													IsReadable() const noexcept = 0;

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
			virtual ExpectedVoid<ReadError> 								Peek(const std::size_t& count, DataType& outBuffer) const noexcept = 0;

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
			virtual ExpectedVoid<Error> 									Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept = 0;

			/**
			 * @brief Read bytes into an existing buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 * @return ExpectedVoid<ReadError> indicating success or failure.
			 */
			virtual ExpectedVoid<ReadError> 								Read(const std::size_t& count, DataType& outBuffer) const noexcept = 0;

			/**
			 * @brief Read bytes into a WriteOnly buffer.
			 * @param count Number of bytes to read; 0 reads all available from read position.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			virtual ExpectedVoid<Error> 									Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept = 0;

			/**
			 * @brief Read all bytes until end-of-file into an existing buffer.
			 * @param outBuffer Vector to fill with read bytes; resized as needed.
			 */
			virtual void													ReadUntilEoF(DataType& outBuffer) const noexcept = 0;

			/**
			 * @brief Read all bytes until end-of-file into a WriteOnly buffer.
			 * @param outBuffer WriteOnly to fill with read bytes; resized as needed.
			 * @return ExpectedVoid<Error> indicating success or failure.
			 */
			virtual ExpectedVoid<Error>										ReadUntilEoF(WriteOnly& outBuffer) const noexcept = 0;

			/**
			 * @brief Move the read position for non-destructive reads.
			 * @param position The offset value to apply.
			 * @param mode Unused for base class; included for API consistency.
			 * @details Changes where subsequent Read() operations will start reading from.
			 *          Position is clamped to [0, Size()]. Does not affect stored data.
			 * @see Read(), Position
			 * If Position is set to Absolute and offset is negative the operation is noop
			 */
			virtual void 													Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept = 0;

			/**
			 * @brief Get the current number of bytes stored in the buffer.
			 * @return The total number of bytes available for reading.
			 * @see Capacity(), Empty()
			 */
			virtual std::size_t 											Size() const noexcept = 0;
	};

	/**
	 * @class WriteOnly
	 * @brief Generic class providing a buffer that can be written to but not read from.
	 * @par Overview
	 *  WriteOnly extends Generic to provide a write-only buffer interface. It allows
	 *  writing data to the buffer while preventing any read operations. This is useful
	 *  for scenarios where data needs to be collected or buffered without exposing it
	 *  for reading.
	 */
	class STORMBYTE_BUFFER_PUBLIC WriteOnly: virtual public Generic {
		public:
			/**
			 * 	@brief Construct WriteOnly.
			 */
			inline WriteOnly() noexcept: Generic() {};

			/**
			 * 	@brief Copy construct deleted
			 */
			WriteOnly(const WriteOnly&) 									= delete;
			
			/**
			 * 	@brief Move construct deleted
			 */
			WriteOnly(WriteOnly&&) noexcept									= delete;

			/**
			 * 	@brief Virtual destructor.
			 */
			virtual ~WriteOnly() noexcept 									= default;
			
			/**
			 * 	@brief Copy assign deleted
			 */
			WriteOnly& operator=(const WriteOnly&) 							= delete;
		
			/**
			 * 	@brief Move assign deleted
			 */
			WriteOnly& operator=(WriteOnly&&) noexcept						= delete;

			/**
			 * @brief Check if the buffer is writable.
			 * @return true if the buffer can accept write operations, false if closed or in error state.
			 */
			virtual bool 													IsWritable() const noexcept = 0;

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<WriteError> 								Write(const std::size_t& count, const DataType& data) noexcept = 0;

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<WriteError> 								Write(const DataType& data) noexcept {
				return Write(data.size(), data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<WriteError> 								Write(DataType&& data) noexcept {
				return Write(data.size(), std::move(data));
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<WriteError> 								Write(const std::size_t& count, DataType&& data) noexcept = 0;

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<Error> 									Write(const std::size_t& count, const ReadOnly& data) noexcept = 0;

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<Error> 										Write(const ReadOnly& data) noexcept {
				return Write(data.AvailableBytes(), data);
			}

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			virtual ExpectedVoid<Error> 									Write(const std::size_t& count, ReadOnly&& data) noexcept = 0;

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::string&), IsClosed()
			 */
			inline ExpectedVoid<Error> 										Write(ReadOnly&& data) noexcept {
				return Write(data.AvailableBytes(), std::move(data));
			}

			/**
			 * @brief Write a string to the buffer.
			 * @param data String to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see Write(const std::size_t&, const DataType&), IsClosed()
			 */
			virtual ExpectedVoid<WriteError> 								Write(const std::string& data) noexcept = 0;
	};

	/**
	 * @class ReadWrite
	 * @brief Generic class providing a buffer that can be both read from and written to.
	 * @par Overview
	 *  ReadWrite extends both ReadOnly and WriteOnly to provide a full read-write
	 *  buffer interface. It allows reading and writing data to the buffer, supporting
	 *  scenarios where data needs to be both consumed and produced.
	 */
	class STORMBYTE_BUFFER_PUBLIC ReadWrite: public ReadOnly, public WriteOnly {
		public:
			/**
			 * 	@brief Construct ReadWrite.
			 */
			inline ReadWrite() noexcept: Generic() {};

			/**
			 * 	@brief Copy construct deleted
			 */
			ReadWrite(const ReadWrite& other) noexcept 						= delete;
			
			/**
			 * 	@brief Move construct deleted
			 */
			ReadWrite(ReadWrite&& other) noexcept							= delete;

			/**
			 * 	@brief Virtual destructor.
			 */
			virtual ~ReadWrite() noexcept 									= default;
			
			/**
			 * 	@brief Copy assign deleted
			 */
			ReadWrite& operator=(const ReadWrite& other) 					= delete;
		
			/**
			 * 	@brief Move assign deleted
			 */
			ReadWrite& operator=(ReadWrite&& other) noexcept				= delete;
	};
}