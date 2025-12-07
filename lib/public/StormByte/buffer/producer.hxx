#pragma once

#include <StormByte/buffer/consumer.hxx>

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
	 * @class Producer
	 * @brief Producer interface for writing data to a shared FIFO buffer.
	 *
	 * @par Overview
	 *  Producer provides a write-only interface to a SharedFIFO buffer.
	 *  Multiple Producer instances can share the same underlying buffer,
	 *  allowing multiple producers to write data concurrently in a thread-safe manner.
	 *
	 * @par Thread safety
	 *  All write operations are thread-safe as they delegate to the underlying
	 *  SharedFIFO which is fully thread-safe.
	 */
	class STORMBYTE_BUFFER_PUBLIC Producer final: public WriteOnly {
		public:
			/**
			 * @brief Construct a Producer with a new SharedFIFO buffer.
			 * @details Creates a new Producer instance with its own underlying
			 *          SharedFIFO buffer for writing data.
			 */
			inline Producer() noexcept: m_buffer(std::make_shared<SharedFIFO>()) {};

			/**
			 * @brief Construct a Producer with an existing SharedFIFO buffer.
			 * @param buffer Shared pointer to the SharedFIFO buffer to produce to.
			 * @details Private constructor only accessible by Consumer (friend class).
			 *          Creates a new Producer instance that shares the given buffer.
			 */
			inline Producer(std::shared_ptr<SharedFIFO> buffer): m_buffer(buffer) {}

			/**
			 * @brief Construct a Producer from a Consumer's buffer.
			 * @details Creates a new Producer instance sharing the same underlying
			 *          SharedFIFO buffer as the provided Consumer.
			 */
			inline Producer(const Consumer& consumer): m_buffer(consumer.m_buffer) {}

			/**
			 * @brief Copy constructor.
			 * @param other Source Producer to copy from.
			 * @details Copies the Producer instance, sharing the same underlying buffer.
			 *          Both instances will write to the same SharedFIFO.
			 */
			inline Producer(const Producer& other) noexcept {
				m_buffer = other.m_buffer;
			}

			/**
			 * @brief Move constructor.
			 * @param other Source Producer to move from.
			 * @details Transfers ownership of the buffer from the moved-from Producer.
			 */
			inline Producer(Producer&& other) noexcept {
				m_buffer = std::move(other.m_buffer);
			}

			/**
			 * @brief Destructor.
			 */
			~Producer() noexcept										= default;

			/**
			 * @brief Copy assignment operator.
			 * @param other Source Producer to copy from.
			 * @return Reference to this Producer.
			 */
			inline Producer& operator=(const Producer& other) noexcept {
				if (this != &other)
					m_buffer = other.m_buffer;
				return *this;
			}

			/**
			 * @brief Move assignment operator.
			 * @return Reference to this Producer.
			 */
			inline Producer& operator=(Producer&& other) noexcept {
				if (this != &other)
					m_buffer = std::move(other.m_buffer);
				return *this;
			}

			/**
			 * @brief Equality comparison.
			 * @return true if both Producers share the same underlying buffer.
			 */
			inline bool operator==(const Producer& other) const noexcept {
				return m_buffer.get() == other.m_buffer.get();
			}

			/**
			 * @brief Inequality comparison.
			 * @return true if Producers have different underlying buffers.
			 */
			inline bool operator!=(const Producer& other) const noexcept {
				return !(*this == other);
			}

			/**
			 * @brief Thread-safe close for further writes.
			 * @details Marks buffer as closed, notifies all waiting threads. Subsequent writes
			 *          are ignored. The buffer remains readable until all data is consumed.
			 * @see FIFO::Close(), IsWritable()
			 */
			inline void 												Close() noexcept {
				m_buffer->Close();
			}

			inline bool 												IsWritable() const noexcept override {
				return m_buffer->IsWritable();
			}

			/**
			 * @brief Thread-safe error state setting.
			 * @details Marks buffer as erroneous (unreadable and unwritable), notifies all
			 *          waiting threads. Subsequent writes are ignored and reads will fail.
			 * @see FIFO::SetError(), IsReadable(), IsWritable()
			 */
			inline void 												SetError() noexcept {
				m_buffer->SetError();
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
			inline ExpectedVoid<WriteError> 							Write(const std::size_t& count, const DataType& data) noexcept override {
				return m_buffer->Write(count, data);
			}

			/**
			 * @brief Write all bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<WriteError> 							Write(const DataType& data) noexcept {
				return Write(data.size(), data);
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
			inline ExpectedVoid<WriteError> 							Write(const std::size_t& count, DataType&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}

			/**
			 * @brief Move all bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<WriteError> 							Write(DataType&& data) noexcept {
				return Write(data.size(), std::move(data));
			}

			/**
			 * @brief Write all elements from an input range to the buffer.
			 * @tparam R An input range whose element type is convertible to `std::byte`.
			 * @param r The input range to write from. Elements are converted
			 *          element-wise using `static_cast<std::byte>`.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details The entire range is converted into a `DataType` (the
			 *          library's `std::vector<std::byte>`) and then forwarded to the
			 *          canonical `Write(count, DataType)` entry point.
			 */
			template<std::ranges::input_range R>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
				requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); }
			ExpectedVoid<WriteError> 									Write(const R& r) noexcept {
				return m_buffer->Write(r);
			}

			/**
			 * @brief Write up-to `count` elements from an input range.
			 * @tparam Rw An input range whose value_type is convertible to `std::byte`.
			 * @param count Maximum number of elements to write; pass `0` to write
			 *              the entire range.
			 * @param r The input range to read from.
			 * @return ExpectedVoid<WriteError> with the number of bytes actually written
			 *         (may be less than `count` if the range is shorter).
			 * @details Elements are copied and converted into an internal `DataType`
			 *          buffer before invoking the canonical `Write(count, DataType)`.
			 */
			template<std::ranges::input_range Rw>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rw>>>) &&
				requires(std::ranges::range_value_t<Rw> v) { static_cast<std::byte>(v); }
			ExpectedVoid<WriteError> 									Write(const std::size_t& count, const Rw& r) noexcept {
				return m_buffer->Write(count, r);
			}

			/**
			 * @brief Write up-to `count` elements from an rvalue range.
			 * @tparam Rrw An input range type that may be an rvalue `DataType`.
			 * @param count Maximum number of elements to write; pass `0` to write
			 *              the entire range.
			 * @param r The rvalue range to consume. If `r` is a `DataType` rvalue the
			 *          implementation will move it into the write fast-path and trim
			 *          it to `count` if necessary. Otherwise elements are converted
			 *          and copied into a temporary `DataType`.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 */
			template<std::ranges::input_range Rrw>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rrw>>>) &&
				requires(std::ranges::range_value_t<Rrw> v) { static_cast<std::byte>(v); }
			ExpectedVoid<WriteError> 									Write(const std::size_t& count, Rrw&& r) noexcept {
				return m_buffer->Write(count, std::forward<Rrw>(r));
			}

			/**
			 * @brief Write from an rvalue input range.
			 * @tparam Rr Input range type; this overload prefers moving when the
			 *            range type is the library `DataType` (i.e. `std::vector<std::byte>`).
			 * @param r The rvalue range to write from. If `r` is a `DataType` rvalue
			 *          it will be forwarded into the move-write fast-path; otherwise
			 *          elements are converted and copied into a temporary `DataType`.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 */
			template<std::ranges::input_range Rr>
				requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
				requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
			ExpectedVoid<WriteError> 									Write(Rr&& r) noexcept {
				return m_buffer->Write(std::forward<Rr>(r));
			}

			/**
			 * @brief Write from iterator pair whose value_type is convertible to `std::byte`.
			 */
			/**
			 * @brief Write all elements from an iterator pair.
			 * @tparam I Input iterator type whose `value_type` is convertible to `std::byte`.
			 * @tparam S Corresponding sentinel type for `I`.
			 * @param first Iterator to the beginning of the sequence.
			 * @param last  Sentinel/iterator marking the end of the sequence.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Elements are converted via `static_cast<std::byte>` into an
			 *          internal `DataType` buffer which is then forwarded to the
			 *          canonical `Write(count, DataType)`.
			 */
			template<std::input_iterator I, std::sentinel_for<I> S>
				requires (!std::is_class_v<std::remove_cv_t<std::iter_value_t<I>>>) &&
				requires(std::iter_value_t<I> v) { static_cast<std::byte>(v); }
			ExpectedVoid<WriteError> 									Write(I first, S last) noexcept {
				return m_buffer->Write(first, last);
			}

			/**
			 * @brief Write up-to `count` bytes from an iterator pair (0 => all available).
			 */
			/**
			 * @brief Write up-to `count` elements from an iterator pair.
			 * @tparam I2 Input iterator type whose `value_type` is convertible to `std::byte`.
			 * @tparam S2 Corresponding sentinel type for `I2`.
			 * @param count Maximum number of elements to write; pass `0` to write all.
			 * @param first Iterator to the beginning of the sequence.
			 * @param last  Sentinel/iterator marking the end of the sequence.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Iterates until `count` elements are consumed or `first == last`.
			 */
			template<std::input_iterator I2, std::sentinel_for<I2> S2>
				requires (!std::is_class_v<std::remove_cv_t<std::iter_value_t<I2>>>) &&
				requires(std::iter_value_t<I2> v) { static_cast<std::byte>(v); }
			ExpectedVoid<WriteError> 									Write(const std::size_t& count, I2 first, S2 last) noexcept {
				return m_buffer->Write(count, first, last);
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
			inline ExpectedVoid<Error> 									Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return m_buffer->Write(count, data);
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
			inline ExpectedVoid<Error> 									Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}
			
			/**
			 * @brief Create a Consumer for reading from this Producer's buffer.
			 * @return A Consumer instance sharing the underlying buffer.
			 * @details Enables producer-consumer pattern. Consumer has read-only access
			 *          to the same SharedFIFO buffer this Producer writes to.
			 * @see Consumer
			 */
			inline class Consumer										Consumer() {
				return { m_buffer };
			}
			
		protected:
			std::shared_ptr<SharedFIFO> m_buffer;
	};
}
