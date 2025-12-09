#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/clonable.hxx>

#include <functional>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class ExternalReader
	 * @brief Interface for reading data from an external source.
	 * @details This class defines the interface for reading data from an external source
	 *          such as a file, network socket, or other I/O stream.
	 *          It provides methods for reading data into a buffer.
	 * @note This class is intended to be used as a base class for specific
	 *       implementations that handle different types of external data sources.
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalReader: public Clonable<ExternalReader, std::unique_ptr<ExternalReader>> {
		public:
			/**
			 * @brief Construct ExternalReader.
			 */
			ExternalReader() noexcept 												= default;

			/**
			 * @brief Construct ExternalReader.
			 * @param other ExternalReader to copy from.
			 */
			ExternalReader(const ExternalReader& other) 							= default;

			/**
			 * @brief Move constructor.
			 * @param other ExternalReader to move from.
			 */
			ExternalReader(ExternalReader&& other) noexcept 						= default;

			/**
			 * @brief Destructor.
			 */
			virtual ~ExternalReader() noexcept 										= default;

			/**
			 * @brief Assignment operator.
			 * @param other ExternalReader to copy from.
			 * @return Reference to this ExternalReader.
			 */
			ExternalReader& operator=(const ExternalReader& other) 					= default;

			/**
			 * @brief Move assignment operator.
			 * @param other ExternalReader to move from.
			 * @return Reference to this ExternalReader.
			 */
			ExternalReader& operator=(ExternalReader&& other) noexcept 				= default;

			/**
			 * @brief Read data into the provided buffer.
			 * @param bytes Number of bytes to read.
			 * @param out DataType to fill with read data.
			 * @return true if data was successfully read, false otherwise.
			 */
			virtual bool 															Read(std::size_t bytes, DataType& out) const noexcept = 0;

			/**
			 * @brief Read data into the provided buffer.
			 * @param bytes Number of bytes to read.
			 * @param out DataType to fill with read data.
			 * @return true if data was successfully read, false otherwise.
			 * @note Non-const version for mutable readers or destructive read operations.
			 */
			virtual bool 															Read(std::size_t bytes, DataType& out) noexcept = 0;
	};

	/**
	 * @class ExternalBufferReader
	 * @brief Implementation of ExternalReader that reads from a ReadOnly buffer.
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalBufferReader final: public ExternalReader {
		public:
			/**
			 * @brief Construct ExternalBufferReader with a ReadOnly buffer.
			 * @param buffer ReadOnly buffer to read from.
			 * @note The `ExternalBufferReader` does NOT take ownership of `buffer`.
			 *       The caller is responsible for ensuring that `buffer` outlives
			 *       this `ExternalBufferReader` instance. The reader stores a
			 *       reference wrapper to the provided `ReadOnly` object.
			 */
			inline ExternalBufferReader(ReadOnly& buffer) noexcept:
				m_buffer(buffer) {}

			/**
			 * @brief Copy constructor (deleted).
			 * @param other ExternalBufferReader to copy from.
			 */
			ExternalBufferReader(const ExternalBufferReader& other) 				= default;

			/**
			 * @brief Move constructor.
			 * @param other ExternalBufferReader to move from.
			 */
			ExternalBufferReader(ExternalBufferReader&& other) noexcept 			= default;

			/**
			 * @brief Destructor.
			 */
			~ExternalBufferReader() noexcept 										= default;

			/**
			 * @brief Copy assignment (deleted).
			 * @param other ExternalBufferReader to copy from.
			 * @return Reference to this ExternalBufferReader.
			 */
			ExternalBufferReader& operator=(const ExternalBufferReader& other)		= default;

			/**
			 * @brief Move assignment.
			 * @param other ExternalBufferReader to move from.
			 * @return Reference to this ExternalBufferReader.
			 */
			ExternalBufferReader& operator=(ExternalBufferReader&& other) noexcept 	= default;

			/**
			 * @brief Clone this ExternalBufferReader.
			 * @return Pointer to the cloned ExternalBufferReader.
			 */
			inline PointerType 														Clone() const noexcept override {
				return MakePointer<ExternalBufferReader>(*this);
			}

			/**
			 * @brief Move this ExternalBufferReader.
			 * @return Pointer to the moved ExternalBufferReader.
			 */
			inline PointerType 														Move() noexcept override {
				return MakePointer<ExternalBufferReader>(std::move(*this));
			}

			/**
			 * @brief Read data into the provided buffer.
			 * @param bytes Number of bytes to read.
			 * @param out DataType to fill with read data.
			 * @return true if data was successfully read, false otherwise.
			 */
			bool 																	Read(std::size_t bytes, DataType& out) const noexcept override;

			/**
			 * @brief Destructively read data into the provided buffer.
			 * @param bytes Number of bytes to read.
			 * @param out DataType to fill with read data.
			 * @return true if data was successfully read, false otherwise.
			 */
			bool 																	Read(std::size_t bytes, DataType& out) noexcept override;

		private:
			std::reference_wrapper<ReadOnly> m_buffer;								///< Internal read-only buffer reference.
	};

	/**
	 * @class ExternalWriter
	 * @brief Interface for writing data to an external source.
	 * @details This class defines the interface for writing data to an external source
	 *          such as a file, network socket, or other I/O stream.
	 * @note This class is intended to be used as a base class for specific
	 *       implementations that handle different types of external data sources.
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalWriter: public Clonable<ExternalWriter> {
		public:
			/**
			 * @brief Construct ExternalWriter.
			 */
			ExternalWriter() noexcept 												= default;

			/**
			 * @brief Copy constructor.
			 * @param other ExternalWriter to copy from.
			 */
			ExternalWriter(const ExternalWriter& other) 							= default;

			/**
			 * @brief Move constructor.
			 * @param other ExternalWriter to move from.
			 */
			ExternalWriter(ExternalWriter&& other) noexcept 						= default;

			/**
			 * @brief Destructor.
			 */
			virtual ~ExternalWriter() noexcept 										= default;

			/**
			 * @brief Copy assignment operator.
			 * @param other ExternalWriter to copy from.
			 * @return Reference to this ExternalWriter.
			 */
			ExternalWriter& operator=(const ExternalWriter& other) 					= default;
			/**
			 * @brief Move assignment operator.
			 * @param other ExternalWriter to move from.
			 * @return Reference to this ExternalWriter.
			 */
			ExternalWriter& operator=(ExternalWriter&& other) noexcept 				= default;

			/**
			 * @brief Move data from the provided buffer.
			 * @param in DataType containing data to move.
			 * @return true if data was successfully written, false otherwise.
			 */
			virtual bool 															Write(DataType&& in) noexcept = 0;
	};

	class STORMBYTE_BUFFER_PUBLIC ExternalBufferWriter final: public ExternalWriter {
		public:
			/**
			 * @brief Construct ExternalBufferWriter with a WriteOnly buffer.
			 * @param buffer WriteOnly buffer to write to.
			 * @note The `ExternalBufferWriter` does NOT take ownership of `buffer`.
			 *       The caller is responsible for ensuring that `buffer` outlives
			 *       this `ExternalBufferWriter` instance. The writer stores a
			 *       reference wrapper to the provided `WriteOnly` object.
			 */
			inline ExternalBufferWriter(WriteOnly& buffer) noexcept:
				m_buffer(buffer) {}

			/**
			 * @brief Copy constructor (deleted).
			 * @param other ExternalBufferWriter to copy from.
			 */
			ExternalBufferWriter(const ExternalBufferWriter& other) 				= default;

			/**
			 * @brief Move constructor.
			 * @param other ExternalBufferWriter to move from.
			 */
			ExternalBufferWriter(ExternalBufferWriter&& other) noexcept 			= default;

			/**
			 * @brief Destructor.
			 */
			~ExternalBufferWriter() noexcept 										= default;

			/**
			 * @brief Copy assignment (deleted).
			 * @param other ExternalBufferWriter to copy from.
			 * @return Reference to this ExternalBufferWriter.
			 */
			ExternalBufferWriter& operator=(const ExternalBufferWriter& other) 		= default;

			/**
			 * @brief Move assignment.
			 * @param other ExternalBufferWriter to move from.
			 * @return Reference to this ExternalBufferWriter.
			 */
			ExternalBufferWriter& operator=(ExternalBufferWriter&& other) noexcept 	= default;

			/**
			 * @brief Clone this ExternalBufferWriter.
			 * @return Pointer to the cloned ExternalBufferWriter.
			 */
			inline PointerType 														Clone() const noexcept override {
				return MakePointer<ExternalBufferWriter>(*this);
			}

			/**
			 * @brief Move this ExternalBufferWriter.
			 * @return Pointer to the moved ExternalBufferWriter.
			 */
			inline PointerType 														Move() noexcept override {
				return MakePointer<ExternalBufferWriter>(std::move(*this));
			}

			/**
			 * @brief Move data from the provided buffer.
			 * @param in DataType containing data to move.
			 * @return true if data was successfully written, false otherwise.
			 */
			bool 																	Write(DataType&& in) noexcept override;

		private:
			std::reference_wrapper<WriteOnly> m_buffer;								///< Internal write-only buffer reference.
	};
}