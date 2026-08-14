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
	 * Provides a buffer-agnostic reading API so that pipeline stages (and other
	 * consumers) can work uniformly against any concrete source (file, socket,
	 * @ref Ring, @ref LockFreeRing, @ref FIFO, etc.).
	 *
	 * @par Design goals
	 * - Rich enough to replace direct use of @ref ReadOnly / @ref Consumer
	 *   inside @ref Pipeline stages.
	 * - Lets the Pipeline inject any concrete buffer implementation (including
	 *   private ones such as @ref LockFreeRing) without changing stage code.
	 *
	 * @note Implementations are expected to be lightweight adapters; the real
	 *       storage lives elsewhere.
	 *
	 * @see ExternalBufferReader, ExternalWriter, Pipeline
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalReader
		: public Clonable<ExternalReader, std::unique_ptr<ExternalReader>> {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */
			ExternalReader() noexcept = default;
			ExternalReader(const ExternalReader&) = default;
			ExternalReader(ExternalReader&&) noexcept = default;
			~ExternalReader() noexcept override = default;

			ExternalReader& operator=(const ExternalReader&) = default;
			ExternalReader& operator=(ExternalReader&&) noexcept = default;
			/** @} */

			/**
			 * @name Queries
			 * @{
			 */

			/**
			 * @brief Number of bytes currently available for reading.
			 * @return Bytes that can be read without blocking (or until EoF).
			 */
			virtual std::size_t AvailableBytes() const noexcept = 0;

			/**
			 * @brief Whether the underlying source holds no data.
			 * @return @c true if no data remains.
			 */
			virtual bool Empty() const noexcept = 0;

			/**
			 * @brief End-of-file / end-of-stream condition.
			 * @return @c true when the source is closed (or in error) and no more
			 *         bytes are available.
			 */
			virtual bool EoF() const noexcept = 0;

			/**
			 * @brief Whether the source can still be read.
			 * @return @c false when the source is in a permanent error state.
			 */
			virtual bool IsReadable() const noexcept = 0;

			/** @} */

			/**
			 * @name Reading
			 * @{
			 */

			/**
			 * @brief Non-destructive read (advances the logical position).
			 * @param count Number of bytes requested (0 = all available).
			 * @param out   Destination buffer (appended to).
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			virtual bool Read(std::size_t count, DataType& out) const noexcept = 0;

			/**
			 * @brief Convenience overload — read all available bytes.
			 * @param out Destination buffer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Read(DataType& out) const noexcept {
				return Read(0, out);
			}

			/**
			 * @brief Destructive read (consumes / erases data from the source).
			 * @param count Number of bytes requested (0 = all available).
			 * @param out   Destination buffer (appended to).
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			virtual bool Extract(std::size_t count, DataType& out) noexcept = 0;

			/**
			 * @brief Convenience overload — extract all available bytes.
			 * @param out Destination buffer.
			 * @return @c true on success, @c false on insufficient data or error.
			 */
			inline bool Extract(DataType& out) noexcept {
				return Extract(0, out);
			}

			/**
			 * @brief Non-destructive peek (does **not** advance the position).
			 * @param count Number of bytes requested (0 = all available).
			 * @param out   Destination buffer (appended to).
			 * @return @c true on success, @c false on insufficient data or error.
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

			/** @} */

			/**
			 * @name Positioning / maintenance
			 * @{
			 */

			/**
			 * @brief Move the logical read position.
			 * @param offset Offset value.
			 * @param mode   @ref Position::Absolute or @ref Position::Relative.
			 * @note Default implementation is a no-op. Concrete adapters that
			 *       support seeking should override it.
			 */
			virtual void Seek(std::ptrdiff_t offset, Position mode) const noexcept {
				(void)offset;
				(void)mode;
			}

			/**
			 * @brief Discard already-consumed data (from start up to the current
			 *        read position).
			 * @note Default implementation is a no-op.
			 */
			virtual void Clean() noexcept {}

			/** @} */
	};

	/**
	 * @class ExternalBufferReader
	 * @brief Adapter that turns any @ref ReadOnly buffer into an @ref ExternalReader.
	 *
	 * @note Does **not** take ownership of the referenced buffer. The caller must
	 *       guarantee that the @ref ReadOnly object outlives this adapter.
	 *
	 * @see ExternalReader, ReadOnly
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalBufferReader final : public ExternalReader {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Construct from a @ref ReadOnly reference.
			 * @param buffer Buffer to adapt (must outlive this object).
			 */
			explicit ExternalBufferReader(ReadOnly& buffer) noexcept
				: m_buffer(buffer) {}

			ExternalBufferReader(const ExternalBufferReader&) = default;
			ExternalBufferReader(ExternalBufferReader&&) noexcept = default;
			~ExternalBufferReader() noexcept override = default;

			ExternalBufferReader& operator=(const ExternalBufferReader&) = default;
			ExternalBufferReader& operator=(ExternalBufferReader&&) noexcept = default;

			/** @} */

			/**
			 * @name Clonable
			 * @{
			 */

			/**
			 * @brief Polymorphic copy.
			 * @return New adapter referring to the same @ref ReadOnly.
			 */
			PointerType Clone() const noexcept override {
				return MakePointer<ExternalBufferReader>(*this);
			}

			/**
			 * @brief Polymorphic move.
			 * @return New adapter taking over this instance’s reference.
			 */
			PointerType Move() noexcept override {
				return MakePointer<ExternalBufferReader>(std::move(*this));
			}

			/** @} */

			/**
			 * @name Queries
			 * @{
			 */
			std::size_t AvailableBytes() const noexcept override;
			bool        Empty() const noexcept override;
			bool        EoF() const noexcept override;
			bool        IsReadable() const noexcept override;
			/** @} */

			/**
			 * @name Reading
			 * @{
			 */
			bool Read(std::size_t count, DataType& out) const noexcept override;
			bool Extract(std::size_t count, DataType& out) noexcept override;
			bool Peek(std::size_t count, DataType& out) const noexcept override;

			void ReadUntilEoF(DataType& out) const noexcept override;
			void ExtractUntilEoF(DataType& out) noexcept override;
			/** @} */

			/**
			 * @name Positioning / maintenance
			 * @{
			 */
			void Seek(std::ptrdiff_t offset, Position mode) const noexcept override;
			void Clean() noexcept override;
			/** @} */

		private:
			std::reference_wrapper<ReadOnly> m_buffer; ///< Non-owning reference to the source buffer.
	};

	/**
	 * @class ExternalWriter
	 * @brief Abstract interface for writing data to an external or internal sink.
	 *
	 * @par Overview
	 * Provides a buffer-agnostic writing API (including end-of-stream signalling)
	 * so that pipeline stages can write uniformly to any concrete destination
	 * (file, socket, @ref Ring, @ref LockFreeRing, @ref Producer, etc.).
	 *
	 * @see ExternalBufferWriter, ExternalReader, Pipeline
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalWriter
		: public Clonable<ExternalWriter, std::unique_ptr<ExternalWriter>> {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */
			ExternalWriter() noexcept = default;
			ExternalWriter(const ExternalWriter&) = default;
			ExternalWriter(ExternalWriter&&) noexcept = default;
			~ExternalWriter() noexcept override = default;

			ExternalWriter& operator=(const ExternalWriter&) = default;
			ExternalWriter& operator=(ExternalWriter&&) noexcept = default;
			/** @} */

			/**
			 * @name Queries
			 * @{
			 */

			/**
			 * @brief Whether the sink still accepts writes.
			 * @return @c false when the sink has been closed or is in error state.
			 */
			virtual bool IsWritable() const noexcept = 0;

			/** @} */

			/**
			 * @name Writing (canonical overloads)
			 * @{
			 */

			/**
			 * @brief Write (copy) a full byte vector.
			 * @param data Data to append.
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			virtual bool Write(const DataType& data) noexcept = 0;

			/**
			 * @brief Write (move) a full byte vector.
			 * @param data Data to append (emptied on success when the implementation moves).
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			virtual bool Write(DataType&& data) noexcept = 0;

			/**
			 * @brief Write up to @p count bytes from a vector (copy).
			 * @param count Maximum bytes to write (0 = entire vector).
			 * @param data  Source vector.
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			virtual bool Write(std::size_t count, const DataType& data) noexcept = 0;

			/**
			 * @brief Write up to @p count bytes from a vector (move).
			 * @param count Maximum bytes to write (0 = entire vector).
			 * @param data  Source vector (may be modified on success).
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			virtual bool Write(std::size_t count, DataType&& data) noexcept = 0;

			/** @} */

			/**
			 * @name Writing (string conveniences)
			 * @brief Non-virtual helpers implemented in terms of the canonical overloads.
			 *
			 * These ensure @c std::string_view / C-string / string-literal calls bind to
			 * a stable API and avoid copying the trailing NUL of string literals.
			 * @{
			 */

			/**
			 * @brief Write a string view (no terminating NUL).
			 * @param sv Source characters.
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			bool Write(std::string_view sv) noexcept;

			/**
			 * @brief Write a null-terminated C string.
			 * @param s Source string (may be null → empty write).
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			bool Write(const char* s) noexcept;

			/**
			 * @brief Write up to @p count characters from a string view.
			 * @param count Maximum characters (0 = entire view).
			 * @param sv    Source characters.
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			bool Write(std::size_t count, std::string_view sv) noexcept;

			/**
			 * @brief Write a string literal without the trailing NUL.
			 * @tparam N Array extent (includes the NUL for literals).
			 * @param s  String literal / char array.
			 * @return @c true on success, @c false if the sink is closed / in error.
			 */
			template<std::size_t N>
			bool Write(const char (&s)[N]) noexcept {
				if (N == 0) return Write(DataType{});
				return Write(std::string_view(s, N > 0 ? N - 1 : 0));
			}

			/** @} */

			/**
			 * @name End-of-stream signalling
			 * @{
			 */

			/**
			 * @brief Mark the sink closed for further writes.
			 * @details Subsequent @c Write() calls must fail. Readers can still drain
			 *          remaining data. Implementations should wake any waiting readers.
			 */
			virtual void Close() noexcept = 0;

			/**
			 * @brief Put the sink into a permanent error state.
			 * @details Makes the sink unreadable and unwritable from the buffer’s
			 *          perspective. Implementations should wake any waiters.
			 */
			virtual void SetError() noexcept = 0;

			/** @} */
	};

	/**
	 * @class ExternalBufferWriter
	 * @brief Adapter that turns any @ref WriteOnly buffer into an @ref ExternalWriter.
	 *
	 * @note Does **not** take ownership of the referenced buffer. The caller must
	 *       guarantee that the @ref WriteOnly object outlives this adapter.
	 *
	 * @details @ref Close() and @ref SetError() are forwarded to the underlying
	 *          @ref WriteOnly (which declares those operations as pure virtuals).
	 *
	 * @see ExternalWriter, WriteOnly, Producer, Ring
	 */
	class STORMBYTE_BUFFER_PUBLIC ExternalBufferWriter final : public ExternalWriter {
		public:
			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Construct from a @ref WriteOnly reference.
			 * @param buffer Buffer to adapt (must outlive this object).
			 */
			explicit ExternalBufferWriter(WriteOnly& buffer) noexcept
				: m_buffer(buffer) {}

			ExternalBufferWriter(const ExternalBufferWriter&) = default;
			ExternalBufferWriter(ExternalBufferWriter&&) noexcept = default;
			~ExternalBufferWriter() noexcept override = default;

			ExternalBufferWriter& operator=(const ExternalBufferWriter&) = default;
			ExternalBufferWriter& operator=(ExternalBufferWriter&&) noexcept = default;

			/** @} */

			/**
			 * @name Clonable
			 * @{
			 */

			/**
			 * @brief Polymorphic copy.
			 * @return New adapter referring to the same @ref WriteOnly.
			 */
			PointerType Clone() const noexcept override {
				return MakePointer<ExternalBufferWriter>(*this);
			}

			/**
			 * @brief Polymorphic move.
			 * @return New adapter taking over this instance’s reference.
			 */
			PointerType Move() noexcept override {
				return MakePointer<ExternalBufferWriter>(std::move(*this));
			}

			/** @} */

			/**
			 * @name Queries / writing / lifecycle
			 * @brief All operations forward to the referenced @ref WriteOnly.
			 * @{
			 */
			bool IsWritable() const noexcept override;

			bool Write(const DataType& data) noexcept override;
			bool Write(DataType&& data) noexcept override;
			bool Write(std::size_t count, const DataType& data) noexcept override;
			bool Write(std::size_t count, DataType&& data) noexcept override;

			void Close() noexcept override;
			void SetError() noexcept override;
			/** @} */

		private:
			std::reference_wrapper<WriteOnly> m_buffer; ///< Non-owning reference to the sink buffer.
	};
}
