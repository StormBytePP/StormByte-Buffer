#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/clonable.hxx>

#include <functional>
#include <string_view>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class ExternalReader
	 * @brief Abstract interface for reading data from an external or internal source.
	 *
	 * @par Overview
	 * Provides a rich, buffer-agnostic reading API so that pipeline stages
	 * (and other consumers) can work uniformly against any concrete source
	 * (file, socket, Ring, LockFreeRing, FIFO, etc.).
	 *
	 * @par Design goals
	 * - Sufficiently powerful to replace direct use of ReadOnly/Consumer
	 *   inside Pipeline stages.
	 * - Allows the Pipeline to inject any concrete buffer implementation
	 *   (including private ones such as LockFreeRing) without changing
	 *   stage code.
	 *
	 * @note Implementations are expected to be lightweight adapters;
	 *       the real storage lives elsewhere.
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalReader: public Clonable<ExternalReader, std::unique_ptr<ExternalReader>> {
		public:
			ExternalReader() noexcept = default;
			ExternalReader(const ExternalReader&) = default;
			ExternalReader(ExternalReader&&) noexcept = default;
			~ExternalReader() noexcept override = default;

			ExternalReader& operator=(const ExternalReader&) = default;
			ExternalReader& operator=(ExternalReader&&) noexcept = default;

			// -----------------------------------------------------------------
			// Queries
			// -----------------------------------------------------------------

			/**
			 * @brief Number of bytes currently available for reading.
			 * @return Bytes that can be read without blocking (or until EoF).
			 */
			virtual std::size_t AvailableBytes() const noexcept = 0;

			/**
			 * @brief Check whether the underlying source is empty.
			 * @return true if no data remains.
			 */
			virtual bool Empty() const noexcept = 0;

			/**
			 * @brief Check end-of-file / end-of-stream condition.
			 * @return true when the source is closed (or in error) and no more
			 *         bytes are available.
			 */
			virtual bool EoF() const noexcept = 0;

			/**
			 * @brief Check whether the source can still be read.
			 * @return false when the source is in a permanent error state.
			 */
			virtual bool IsReadable() const noexcept = 0;

			// -----------------------------------------------------------------
			// Reading
			// -----------------------------------------------------------------

			/**
			 * @brief Non-destructive read (advances logical position).
			 * @param count Number of bytes requested (0 = all available).
			 * @param out   Destination buffer (appended to).
			 * @return true on success, false on insufficient data or error.
			 */
			virtual bool Read(std::size_t count, DataType& out) const noexcept = 0;

			/**
			 * @brief Destructive read (consumes / erases data).
			 * @param count Number of bytes requested (0 = all available).
			 * @param out   Destination buffer (appended to).
			 * @return true on success, false on insufficient data or error.
			 */
			virtual bool Extract(std::size_t count, DataType& out) noexcept = 0;

			/**
			 * @brief Non-destructive peek (does **not** advance position).
			 * @param count Number of bytes requested (0 = all available).
			 * @param out   Destination buffer (appended to).
			 * @return true on success, false on insufficient data or error.
			 */
			virtual bool Peek(std::size_t count, DataType& out) const noexcept = 0;

			/**
			 * @brief Read everything until EoF (non-destructive).
			 * @param out Destination buffer.
			 */
			virtual void ReadUntilEoF(DataType& out) const noexcept = 0;

			/**
			 * @brief Extract everything until EoF (destructive).
			 * @param out Destination buffer.
			 */
			virtual void ExtractUntilEoF(DataType& out) noexcept = 0;

			// -----------------------------------------------------------------
			// Positioning / maintenance (optional but useful)
			// -----------------------------------------------------------------

			/**
			 * @brief Move the logical read position.
			 * @param offset Offset value.
			 * @param mode   Absolute or Relative.
			 * @note Default implementation is a no-op. Concrete adapters that
			 *       support seeking should override it.
			 */
			virtual void Seek(std::ptrdiff_t offset, Position mode) const noexcept {
				(void)offset;
				(void)mode;
			}

			/**
			 * @brief Discard already-consumed data (from start up to current
			 *        read position). Default is a no-op.
			 */
			virtual void Clean() noexcept {}

			/**
			 * @brief Convenience overload – extract all available bytes.
			 */
			inline bool Extract(DataType& out) noexcept {
				return Extract(0, out);
			}

			/**
			 * @brief Convenience overload – read all available bytes.
			 */
			inline bool Read(DataType& out) const noexcept {
				return Read(0, out);
			}
	};

	/**
	 * @class ExternalBufferReader
	 * @brief Adapter that turns any ReadOnly buffer into an ExternalReader.
	 *
	 * @note Does **not** take ownership of the referenced buffer.
	 *       The caller must guarantee that the ReadOnly object outlives
	 *       this adapter.
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalBufferReader final: public ExternalReader {
		public:
			/**
			 * @brief Construct from a ReadOnly reference.
			 * @param buffer Buffer to adapt (must outlive this object).
			 */
			explicit ExternalBufferReader(ReadOnly& buffer) noexcept
				: m_buffer(buffer) {}

			ExternalBufferReader(const ExternalBufferReader&) = default;
			ExternalBufferReader(ExternalBufferReader&&) noexcept = default;
			~ExternalBufferReader() noexcept override = default;

			ExternalBufferReader& operator=(const ExternalBufferReader&) = default;
			ExternalBufferReader& operator=(ExternalBufferReader&&) noexcept = default;

			PointerType Clone() const noexcept override {
				return MakePointer<ExternalBufferReader>(*this);
			}
			PointerType Move() noexcept override {
				return MakePointer<ExternalBufferReader>(std::move(*this));
			}

			// -----------------------------------------------------------------
			// Queries
			// -----------------------------------------------------------------
			std::size_t AvailableBytes() const noexcept override;
			bool        Empty() const noexcept override;
			bool        EoF() const noexcept override;
			bool        IsReadable() const noexcept override;

			// -----------------------------------------------------------------
			// Reading
			// -----------------------------------------------------------------
			bool Read(std::size_t count, DataType& out) const noexcept override;
			bool Extract(std::size_t count, DataType& out) noexcept override;
			bool Peek(std::size_t count, DataType& out) const noexcept override;

			void ReadUntilEoF(DataType& out) const noexcept override;
			void ExtractUntilEoF(DataType& out) noexcept override;

			void Seek(std::ptrdiff_t offset, Position mode) const noexcept override;
			void Clean() noexcept override;

		private:
			std::reference_wrapper<ReadOnly> m_buffer;
	};

	/**
	 * @class ExternalWriter
	 * @brief Abstract interface for writing data to an external or internal sink.
	 *
	 * @par Overview
	 * Provides a rich, buffer-agnostic writing API (including end-of-stream
	 * signalling) so that pipeline stages can write uniformly to any concrete
	 * destination (file, socket, Ring, LockFreeRing, etc.).
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalWriter: public Clonable<ExternalWriter, std::unique_ptr<ExternalWriter>> {
		public:
			ExternalWriter() noexcept = default;
			ExternalWriter(const ExternalWriter&) = default;
			ExternalWriter(ExternalWriter&&) noexcept = default;
			~ExternalWriter() noexcept override = default;

			ExternalWriter& operator=(const ExternalWriter&) = default;
			ExternalWriter& operator=(ExternalWriter&&) noexcept = default;

			// -----------------------------------------------------------------
			// Queries
			// -----------------------------------------------------------------

			/**
			 * @brief Check whether the sink still accepts writes.
			 * @return false when the sink has been closed or is in error state.
			 */
			virtual bool IsWritable() const noexcept = 0;

			// -----------------------------------------------------------------
			// Writing
			// -----------------------------------------------------------------

			/**
			 * @brief Write (copy) a byte vector.
			 * @param data Data to append.
			 * @return true on success, false if the sink is closed/error.
			 */
			virtual bool Write(const DataType& data) noexcept = 0;

			/**
			 * @brief Write (move) a byte vector.
			 * @param data Data to append (will be emptied on success).
			 * @return true on success, false if the sink is closed/error.
			 */
			virtual bool Write(DataType&& data) noexcept = 0;

			/**
			 * @brief Write up to @p count bytes from a vector.
			 * @param count Maximum bytes to write (0 = entire vector).
			 * @param data  Source vector.
			 * @return true on success.
			 */
			virtual bool Write(std::size_t count, const DataType& data) noexcept = 0;

			/**
			 * @brief Write up to @p count bytes, moving from the source.
			 * @param count Maximum bytes to write (0 = entire vector).
			 * @param data  Source vector (modified on success).
			 * @return true on success.
			 */
			virtual bool Write(std::size_t count, DataType&& data) noexcept = 0;

			// Convenience overloads (implemented in terms of the pure virtuals)
			bool Write(std::string_view sv) noexcept;
			bool Write(const char* s) noexcept;
			bool Write(std::size_t count, std::string_view sv) noexcept;

			template<std::size_t N>
			bool Write(const char (&s)[N]) noexcept {
				if (N == 0) return Write(DataType{});
				return Write(std::string_view(s, N > 0 ? N - 1 : 0));
			}

			// -----------------------------------------------------------------
			// End-of-stream signalling
			// -----------------------------------------------------------------

			/**
			 * @brief Mark the sink closed for further writes.
			 * @details Subsequent Write() calls must fail. Readers can still
			 *          drain remaining data. Implementations should wake any
			 *          waiting readers.
			 */
			virtual void Close() noexcept = 0;

			/**
			 * @brief Put the sink into a permanent error state.
			 * @details Makes the sink both unreadable and unwritable.
			 *          Implementations should wake any waiters.
			 */
			virtual void SetError() noexcept = 0;
	};

	/**
	 * @class ExternalBufferWriter
	 * @brief Adapter that turns any WriteOnly buffer into an ExternalWriter.
	 *
	 * @note Does **not** take ownership of the referenced buffer.
	 *       The caller must guarantee that the WriteOnly object outlives
	 *       this adapter.
	 *
	 * @warning Close() / SetError() require the concrete WriteOnly
	 *          implementation to support those operations (Ring, LockFreeRing,
	 *          Producer, etc.). Pure WriteOnly without those methods will
	 *          need a thin wrapper or the methods added to the base.
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalBufferWriter final: public ExternalWriter {
		public:
			/**
			 * @brief Construct from a WriteOnly reference.
			 * @param buffer Buffer to adapt (must outlive this object).
			 */
			explicit ExternalBufferWriter(WriteOnly& buffer) noexcept
				: m_buffer(buffer) {}

			ExternalBufferWriter(const ExternalBufferWriter&) = default;
			ExternalBufferWriter(ExternalBufferWriter&&) noexcept = default;
			~ExternalBufferWriter() noexcept override = default;

			ExternalBufferWriter& operator=(const ExternalBufferWriter&) = default;
			ExternalBufferWriter& operator=(ExternalBufferWriter&&) noexcept = default;

			PointerType Clone() const noexcept override {
				return MakePointer<ExternalBufferWriter>(*this);
			}
			PointerType Move() noexcept override {
				return MakePointer<ExternalBufferWriter>(std::move(*this));
			}

			bool IsWritable() const noexcept override;

			bool Write(const DataType& data) noexcept override;
			bool Write(DataType&& data) noexcept override;
			bool Write(std::size_t count, const DataType& data) noexcept override;
			bool Write(std::size_t count, DataType&& data) noexcept override;

			void Close() noexcept override;
			void SetError() noexcept override;

		private:
			std::reference_wrapper<WriteOnly> m_buffer;
	};
}
