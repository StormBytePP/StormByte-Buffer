#pragma once

#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/fifo.hxx>

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
	 * @class Bridge
	 * @brief Pass-through adapter that forwards bytes from an @ref ExternalReader
	 *        to an @ref ExternalWriter in fixed-size chunks.
	 *
	 * @par Overview
	 * The Bridge connects an ExternalReader (source) and an ExternalWriter (sink).
	 * It reads data from the reader and forwards it to the writer in blocks of
	 * @c chunk_size bytes. A small internal @ref FIFO accumulates leftovers
	 * between calls.
	 *
	 * @par Key behaviour
	 * - When enough bytes are available (≥ @c chunk_size) the bridge forwards
	 *   whole chunks via @ref ExternalWriter::Write().
	 * - If @c chunk_size is zero, chunking is disabled: every read is written
	 *   immediately (no accumulation of leftovers).
	 * - After a successful passthrough the internal buffer contains at most
	 *   @c chunk_size - 1 bytes. @ref Flush() writes any remaining bytes in a
	 *   single call.
	 * - The destructor automatically calls @ref Flush().
	 *
	 * @par End-of-stream and error handling
	 * - @ref FlushAndClose() flushes pending data and then calls
	 *   @c out.Close() on the writer.
	 * - @ref SetError() propagates the error state to the writer.
	 * - @ref EoF() and @ref IsReadable() / @ref IsWritable() delegate to the
	 *   underlying reader/writer.
	 *
	 * @par Thread safety
	 * The Bridge is **not thread-safe**. It is intended for single-threaded use
	 * only. Concurrent access requires external synchronization.
	 *
	 * @note The internal buffer is marked @c mutable so that logically-const
	 *       operations (const overloads of Passthrough/Flush) may update it.
	 *       This does **not** imply thread safety.
	 *
	 * @see ExternalReader, ExternalWriter, FIFO, Pipeline
	 */
	class STORMBYTE_BUFFER_PUBLIC Bridge {
	public:
		/**
		 * @brief Construct a Bridge by cloning the supplied handlers.
		 * @param in         External reader used as source.
		 * @param out        External writer used as sink.
		 * @param chunk_size Size of each write chunk.  
		 *                   If zero, chunking is disabled and every read is
		 *                   forwarded immediately.
		 */
		inline Bridge(const ExternalReader& in,
					const ExternalWriter& out,
					std::size_t chunk_size = 4096) noexcept
			: m_buffer()
			, m_read_handler(in.Clone())
			, m_write_handler(out.Clone())
			, m_chunk_size(chunk_size)
		{}

		/**
		 * @brief Construct a Bridge by moving the supplied handlers.
		 * @param in         External reader (will be moved).
		 * @param out        External writer (will be moved).
		 * @param chunk_size Size of each write chunk (0 = no chunking).
		 */
		inline Bridge(ExternalReader&& in,
					ExternalWriter&& out,
					std::size_t chunk_size = 4096) noexcept
			: m_buffer()
			, m_read_handler(in.Move())
			, m_write_handler(out.Move())
			, m_chunk_size(chunk_size)
		{}

		/** @brief Copy constructor (deleted – Bridge is non-copyable). */
		Bridge(const Bridge&) = delete;

		/** @brief Move constructor. */
		Bridge(Bridge&&) noexcept = default;

		/**
		 * @brief Destructor.
		 * @details Automatically attempts to flush any pending bytes.
		 */
		inline ~Bridge() noexcept {
			Flush();
		}

		/** @brief Copy assignment (deleted). */
		Bridge& operator=(const Bridge&) = delete;

		/** @brief Move assignment. */
		Bridge& operator=(Bridge&&) noexcept = default;

		/**
		 * @brief Configured chunk size.
		 * @return Chunk size in bytes (0 means “no chunking”).
		 */
		inline std::size_t ChunkSize() const noexcept {
			return m_chunk_size;
		}

		/**
		 * @brief Number of bytes currently waiting in the internal buffer.
		 * @return Pending bytes (always < chunk_size when chunking is enabled).
		 */
		inline std::size_t PendingBytes() const noexcept {
			return m_buffer.Size();
		}

		/**
		 * @brief Check whether the source has reached end-of-stream.
		 * @return true when the underlying reader reports EoF.
		 */
		inline bool EoF() const noexcept {
			return m_read_handler->EoF();
		}

		/**
		 * @brief Check whether the source is still readable.
		 */
		inline bool IsReadable() const noexcept {
			return m_read_handler->IsReadable();
		}

		/**
		 * @brief Check whether the sink still accepts writes.
		 */
		inline bool IsWritable() const noexcept {
			return m_write_handler->IsWritable();
		}

		/**
		 * @brief Flush any pending bytes to the writer.
		 * @return true on success (or if there was nothing to flush), false on write error.
		 */
		bool Flush() const noexcept;

		/**
		 * @brief Flush pending bytes and then close the writer.
		 * @return true if the flush (and subsequent close) succeeded.
		 * @details Equivalent to @c Flush() followed by @c out.Close().
		 *          After this call the writer will reject further writes.
		 */
		bool FlushAndClose() const noexcept;

		/**
		 * @brief Propagate a permanent error to the writer.
		 * @details Calls @c SetError() on the underlying ExternalWriter.
		 *          Subsequent writes will fail and readers may observe the error.
		 */
		void SetError() const noexcept;

		/**
		 * @brief Read up to @p bytes from the source and forward them to the sink
		 *        (const overload – non-destructive read when the reader supports it).
		 * @param bytes Number of bytes to request (0 = none).
		 * @return true on success, false on read or write failure.
		 *
		 * @details Data is written in blocks of @c chunk_size. Any tail shorter
		 *          than @c chunk_size is kept in the internal buffer and will be
		 *          sent on a later call or by @ref Flush().
		 */
		bool Passthrough(std::size_t bytes) const noexcept;

		/**
		 * @brief Read up to @p bytes from the source and forward them to the sink
		 *        (non-const overload – may consume data destructively).
		 * @param bytes Number of bytes to request (0 = none).
		 * @return true on success, false on read or write failure.
		 */
		bool Passthrough(std::size_t bytes) noexcept;

	private:
		mutable FIFO                     m_buffer;        ///< Leftover bytes (< chunk_size).
		ExternalReader::PointerType      m_read_handler;  ///< Owned reader.
		ExternalWriter::PointerType      m_write_handler; ///< Owned writer.
		std::size_t                      m_chunk_size;    ///< 0 = write everything immediately.

		/**
		 * @brief Internal helper that writes @p data (plus any previous leftovers)
		 *        to the sink in chunks of @c m_chunk_size.
		 * @param data Newly read data (will be moved from).
		 * @return true on success, false if any write failed.
		 *
		 * @details On failure the remaining unwritten bytes are preserved in
		 *          @c m_buffer so they are not lost.
		 */
		bool PassthroughWrite(DataType&& data) const noexcept;
	};

}
