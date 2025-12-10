#pragma once

#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/fifo.hxx>

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
	 * @class Bridge
	 * @brief Pass-through adapter that forwards bytes from an `ExternalReader` to an `ExternalWriter` in chunks.
	 *
	 * @details
	 * The `Bridge` connects an `ExternalReader` (source) and an `ExternalWriter` (sink). It reads data
	 * from the reader and forwards it to the writer in fixed-size chunks specified by `chunk_size`.
	 * The class keeps a small internal `SharedFIFO` buffer (`m_buffer`) that accumulates data between calls.
	 *
	 * Key behaviour and guarantees:
	 * - Reads are requested from the configured `ExternalReader` (const or non-const overload is selected
	 *   depending on whether the `Bridge` instance itself is const). The const overload is expected to
	 *   perform non-destructive/peek-style reads while the non-const overload may destructively consume
	 *   data from the reader.
	 * - When enough bytes are available (>= `chunk_size`) the bridge forwards whole chunks to the
	 *   `ExternalWriter` via `ExternalWriter::Write()`.
	 * - If `chunk_size` is zero then chunking is disabled: the bridge will attempt to forward all
	 *   bytes read from the `ExternalReader` to the `ExternalWriter` immediately (no accumulation
	 *   into fixed-size chunks). In this mode the internal buffer is not used for normal accumulation
	 *   of leftovers; callers should still handle writer failures appropriately.
	 * - To avoid losing data on writer failure the implementation copies the head (the bytes given to the
	 *   writer) and only moves the remaining tail into the internal buffer. This preserves the original
	 *   data in case the writer rejects the chunk.
	 * - After passthrough operations the internal buffer will contain at most `chunk_size - 1` bytes (the
	 *   leftover tail). `Flush()` will write any remaining bytes in a single call.
	 * - The destructor calls `Flush()` to attempt sending pending bytes.
	 *
	 * Thread-safety and constness:
	 * - The `Bridge` is NOT thread-safe and is intended for single-thread usage only. It must not be
	 *   shared concurrently across threads without external synchronization. The class does not provide
	 *   internal locking or other synchronization primitives.
	 * - The internal `mutable` buffer permits logically-const operations to update internal state (for
	 *   example, const passthrough overloads may internally accumulate data). This facility is provided
	 *   solely for convenience in single-threaded scenarios and does not imply safety for concurrent use.
	 * - The `ExternalReader` and `ExternalWriter` objects are stored behind `std::shared_ptr`; callers
	 *   remain responsible for ensuring those handlers are not concurrently accessed in an unsafe manner.
	 */
	class STORMBYTE_BUFFER_PUBLIC Bridge {
		public:
			/**
			 * @brief Construct a Bridge with both read and write handlers (copying handlers).
			 * @param in External reader used for reads.
			 * @param out External writer used for writes.
			 * @param chunk_size Size of each write chunk. If `chunk_size` is zero, chunking is
			 *        disabled and the bridge will attempt to passthrough all read bytes immediately.
			 */
			inline Bridge(const ExternalReader& in, const ExternalWriter& out, std::size_t chunk_size = 4096) noexcept:
				m_buffer(),
				m_read_handler(in.Clone()),
				m_write_handler(out.Clone()),
				m_chunk_size(chunk_size) {}

			/**
			 * @brief Construct a Bridge with both read and write handlers (moving handlers).
			 * @param in External reader used for reads (will be moved).
			 * @param out External writer used for writes (will be moved).
			 * @param chunk_size Size of each write chunk.
			 */
			inline Bridge(ExternalReader&& in, ExternalWriter&& out, std::size_t chunk_size = 4096) noexcept:
				m_buffer(),
				m_read_handler(in.Move()),
				m_write_handler(out.Move()),
				m_chunk_size(chunk_size) {}

			/**
			 * @name Special member functions
			 *
			 * The Bridge is non-copyable but movable. The destructor attempts to
			 * flush any pending bytes by calling `Flush()`.
			 * @{
			 */
			/** @brief Copy constructor (deleted) — Bridge is non-copyable. */
			Bridge(const Bridge& other) 								= delete;

			/** @brief Move constructor (defaulted) — transfers handlers. */
			inline Bridge(Bridge&& other) noexcept						= default;

			/** @brief Destructor — attempts to flush pending bytes. */
			inline ~Bridge() noexcept {
				Flush();
			}

			/** @brief Copy assignment (deleted). */
			Bridge& operator=(const Bridge& other) 						= delete;

			/** @brief Move assignment (defaulted). */
			Bridge& operator=(Bridge&& other) noexcept					= default;

			/** @} */

			/**
			 * @brief Gets the configured chunk size for passthrough operations.
			 * @return Configured chunk size in bytes.
			 */
			inline std::size_t 											ChunkSize() const noexcept {
				return m_chunk_size;
			}

			/**
			 * @brief Flushes any pending bytes in the internal buffer to the write handler.
			 * @return true if write succeeded, false otherwise.
			 */
			bool 														Flush() const noexcept;

			/**
			 * @brief Passthrough bytes from read handler to write handler (const overload).
			 * @param bytes Number of bytes to read. If `bytes` is zero, no bytes are requested.
			 * @return true if the operation succeeded, false on failure.
			 *
			 * @details The method requests up to `bytes` from the configured `ExternalReader` and
			 * forwards data to the `ExternalWriter` in fixed-size blocks of `chunk_size` bytes.
			 * Full chunks are written immediately; any remaining bytes that are fewer than
			 * `chunk_size` are retained in the bridge's internal `m_buffer` as leftovers. These
			 * leftovers will be combined with data from subsequent `Passthrough()` calls or flushed
			 * via `Flush()` which will attempt to write any remaining bytes in a single call.
			 */
			bool 														Passthrough(std::size_t bytes) const noexcept;

			/**
			 * @brief Passthrough bytes from read handler to write handler (non-const overload).
			 * @param bytes Number of bytes to read. If `bytes` is zero, no bytes are requested.
			 * @return true if the operation succeeded, false on failure.
			 *
			 * @details Behaviour mirrors the const overload: data is forwarded in `chunk_size`
			 * increments. Any tail shorter than `chunk_size` is left in `m_buffer` and will be
			 * sent on the next passthrough or when `Flush()` is invoked. This design keeps the
			 * bridge invariant that `m_buffer` holds at most `chunk_size - 1` bytes of pending data.
			 */
			bool														Passthrough(std::size_t bytes) noexcept;

			/**
			 * @brief Gets the number of pending bytes in the internal buffer.
			 * @return Number of bytes currently stored in the internal FIFO buffer.
			 */
			inline std::size_t 											PendingBytes() const noexcept {
				return m_buffer.Size();
			}

		private:
			mutable Buffer::FIFO m_buffer;								///< Internal FIFO buffer used for passthrough operations.
			ExternalReader::PointerType m_read_handler;					///< External reader callable for reading data.
			ExternalWriter::PointerType m_write_handler;				///< External writer callable for writing data.
			std::size_t m_chunk_size;									///< Size of each write chunk (will accumulate).

			/**
			 * @brief Internal helper to passthrough write operation.
			 * @param data DataType to fill with read data.
			 * @return true if data was successfully processed, false otherwise.
			 */
			bool 														PassthroughWrite(Buffer::DataType&& data) const noexcept;
	};
}
