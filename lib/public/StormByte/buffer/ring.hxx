#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/string.hxx>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <span>
#include <sstream>
#include <string>
#include <utility>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, ring buffers,
 * producer-consumer interfaces and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/**
	 * @class Ring
	 * @brief High-performance, thread-safe ring (FIFO) buffer built on @c std::deque.
	 *
	 * @par Overview
	 *  Ring is a grow-on-demand, fully thread-safe byte buffer that implements
	 *  the pure @ref ReadWrite contract. Internally it stores data in a
	 *  @c std::deque<std::byte>, which provides amortized O(1) insertion at the back
	 *  and amortized O(1) destructive removal from the front. This makes it ideal
	 *  for workloads dominated by writes + destructive reads/extracts, avoiding the
	 *  massive data movements that a contiguous @c std::vector would incur on
	 *  front-erasure.
	 *
	 *  A logical read position is maintained so that non-destructive operations
	 *  (Peek / Read) and destructive ones (Extract / Drop / Clean) coexist cleanly.
	 *
	 * @par Thread safety
	 *  Every public method acquires an internal mutex. Blocking reads and extracts
	 *  wait on a condition variable until the requested number of bytes become
	 *  available or the buffer is closed / set into the error state.
	 *
	 * @par Blocking semantics
	 *  - @ref Read and @ref Extract block until at least @c count bytes are available
	 *    (or the buffer becomes unreadable). When @c count == 0 the call returns
	 *    immediately with all currently available bytes.
	 *  - @ref Write never blocks; it appends data or fails if the buffer is closed
	 *    or in error.
	 *
	 * @par Close / Error behaviour
	 *  @ref Close() marks the buffer closed for further writes and wakes all waiters.
	 *  @ref SetError() makes the buffer permanently unreadable and unwritable.
	 *  Subsequent writes are ignored; subsequent reads fail once no data remains.
	 *
	 * @see FIFO, SharedFIFO, Producer, Consumer
	 */
	class STORMBYTE_BUFFER_PUBLIC Ring : public ReadWrite {
		public:
			/**
			 * @brief Default constructor.
			 */
			Ring() noexcept = default;

			/**
			 * @brief Construct with initial data (copy).
			 * @param data Initial byte vector.
			 */
			inline explicit Ring(const DataType& data) noexcept
				: m_buffer(data.begin(), data.end()) {}

			/**
			 * @brief Construct with initial data (move).
			 * @param data Initial byte vector (moved into the deque).
			 */
			inline explicit Ring(DataType&& data) noexcept
				: m_buffer(std::make_move_iterator(data.begin()),
						std::make_move_iterator(data.end())) {}

			/**
			 * @brief Construct from an input range.
			 * @tparam R Input range whose elements are convertible to @c std::byte.
			 */
			template<std::ranges::input_range R>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
					requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
					(!std::same_as<std::remove_cvref_t<R>, DataType>)
			inline explicit Ring(const R& r) noexcept {
				auto converted = DataConvert(r);
				m_buffer.assign(converted.begin(), converted.end());
			}

			/**
			 * @brief Construct from an rvalue range (moves when possible).
			 */
			template<std::ranges::input_range Rr>
			requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
					requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			inline explicit Ring(Rr&& r) noexcept {
				auto converted = DataConvert(std::forward<Rr>(r));
				m_buffer.assign(std::make_move_iterator(converted.begin()),
								std::make_move_iterator(converted.end()));
			}

			/**
			 * @brief Construct from a string view (terminating NUL not included).
			 */
			inline explicit Ring(std::string_view sv) noexcept {
				auto converted = DataConvert(sv);
				m_buffer.assign(converted.begin(), converted.end());
			}

			/**
			 * @brief Construct from a null-terminated C string.
			 */
			inline explicit Ring(const char* s) noexcept
				: Ring(s ? std::string_view(s) : std::string_view{}) {}

			/**
			 * @brief Copy construction is deleted (mutex / condition_variable).
			 */
			Ring(const Ring&) = delete;

			/**
			 * @brief Move constructor.
			 * @param other Source ring buffer; left in a valid empty state.
			 */
			Ring(Ring&& other) noexcept;

			/**
			 * @brief Destructor.
			 */
			virtual ~Ring() noexcept = default;

			/**
			 * @brief Copy assignment is deleted.
			 */
			Ring& operator=(const Ring&) = delete;

			/**
			 * @brief Move assignment.
			 */
			Ring& operator=(Ring&& other) noexcept;

			/**
			 * @brief Equality comparison (content + read position).
			 */
			bool operator==(const Ring& other) const noexcept;

			/**
			 * @brief Inequality comparison.
			 */
			inline bool operator!=(const Ring& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Number of bytes available from the current read position.
			 */
			std::size_t AvailableBytes() const noexcept override;

			/**
			 * @brief Discard data from the front up to the current read position.
			 */
			void Clean() noexcept override;

			/**
			 * @brief Remove all data and reset the read position.
			 */
			void Clear() noexcept override;

			/**
			 * @brief Mark the buffer closed for further writes and wake all waiters.
			 */
			void Close() noexcept;

			/**
			 * @brief Snapshot of the whole buffer as a @c DataType (vector).
			 * @note The returned reference is valid only until the next mutating
			 *       call. Not intended for concurrent use without external locking.
			 */
			const DataType& Data() const noexcept override;

			/**
			 * @brief Discard @p count bytes starting at the current read position.
			 * @return true on success.
			 */
			bool Drop(const std::size_t& count) noexcept override;

			/**
			 * @brief true when the underlying deque contains no data.
			 */
			bool Empty() const noexcept override;

			/**
			 * @brief true when the buffer is closed or in error and no bytes remain.
			 */
			bool EoF() const noexcept override;

			/**
			 * @brief true if the buffer is in the permanent error state.
			 */
			bool HasError() const noexcept;

			/**
			 * @brief Formatted hex-dump of the unread portion.
			 * @param columns Bytes per line (0 → default 16).
			 * @param byte_limit Maximum number of bytes to include (0 → unlimited).
			 */
			std::string HexDump(const std::size_t& columns = 16,
								const std::size_t& byte_limit = 0) const noexcept;

			/**
			 * @brief true while the buffer is not in the error state.
			 */
			bool IsReadable() const noexcept override;

			/**
			 * @brief true while the buffer is neither closed nor in error.
			 */
			bool IsWritable() const noexcept override;

			/**
			 * @brief Non-destructive peek.
			 */
			bool Peek(const std::size_t& count, DataType& outBuffer) const noexcept override;
			bool Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override;

			/**
			 * @brief Non-destructive read (advances the logical read position).
			 */
			bool Read(const std::size_t& count, DataType& outBuffer) const noexcept override;
			bool Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override;

			/**
			 * @brief Read everything until end-of-file.
			 */
			void ReadUntilEoF(DataType& outBuffer) const noexcept override;
			void ReadUntilEoF(WriteOnly& outBuffer) const noexcept override;

			/**
			 * @brief Destructive extract (removes data from the deque).
			 */
			bool Extract(const std::size_t& count, DataType& outBuffer) noexcept override;
			bool Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept override;

			/**
			 * @brief Extract everything until end-of-file.
			 */
			void ExtractUntilEoF(DataType& outBuffer) noexcept override;
			void ExtractUntilEoF(WriteOnly& outBuffer) noexcept override;

			/**
			 * @brief Move the logical read position.
			 */
			void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;

			/**
			 * @brief Put the buffer into the permanent error state and wake waiters.
			 */
			void SetError() noexcept;

			/**
			 * @brief Total number of bytes currently stored.
			 */
			std::size_t Size() const noexcept override;

			/**
			 * @brief Append bytes (copy).
			 */
			bool Write(const std::size_t& count, const DataType& data) noexcept override;

			/**
			 * @brief Append bytes (move).
			 */
			bool Write(const std::size_t& count, DataType&& data) noexcept override;

			/**
			 * @brief Append from another readable buffer.
			 */
			bool Write(const std::size_t& count, const ReadOnly& data) noexcept override;
			bool Write(const std::size_t& count, ReadOnly&& data) noexcept override;

			/** Expose the convenience overloads provided by WriteOnly */
			using WriteOnly::Write;

		protected:
			/**
			 * @brief Enumeration of read operation kinds.
			 */
			enum class Operation {
				Extract, ///< Destructive removal
				Read,    ///< Non-destructive, advances position
				Peek     ///< Non-destructive, keeps position
			};

			/**
			 * @brief Format a span of bytes into classic hex + ASCII lines.
			 */
			static std::string FormatHexLines(std::span<const std::byte> data,
											std::size_t start_offset,
											std::size_t columns) noexcept;

			/**
			 * @brief Build the common header lines for HexDump.
			 */
			virtual std::ostringstream HexDumpHeader() const noexcept;

			/**
			 * @brief Core read implementation (caller already holds the lock when needed).
			 */
			virtual bool ReadInternal(const std::size_t& count,
									DataType& outBuffer,
									Operation flag) noexcept;

			virtual bool ReadInternal(const std::size_t& count,
									WriteOnly& outBuffer,
									Operation flag) noexcept;

			/**
			 * @brief Loop reading until end-of-file.
			 */
			virtual void ReadUntilEoFInternal(DataType& outBuffer, Operation flag) noexcept;
			virtual void ReadUntilEoFInternal(WriteOnly& outBuffer, Operation flag) noexcept;

			/**
			 * @brief Core write implementation.
			 */
			virtual bool WriteInternal(const std::size_t& count, const DataType& src) noexcept;
			virtual bool WriteInternal(const std::size_t& count, DataType&& src) noexcept;

		private:
			std::deque<std::byte>               m_buffer;             ///< Circular storage
			mutable std::size_t                 m_position_offset{0}; ///< Logical read cursor
			bool                                m_closed{false};
			bool                                m_error{false};
			std::string                         m_error_message;

			mutable DataType                    m_data_cache;         ///< Materialised snapshot for Data()
			mutable std::mutex                  m_mutex;
			mutable std::condition_variable_any m_cv;

			/**
			 * @brief Wait until at least @p n bytes are available (or closed/error).
			 */
			void Wait(const std::size_t& n, std::unique_lock<std::mutex>& lock) const;
	};
}
