#pragma once

#include <StormByte/buffer/typedefs.hxx>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <string_view>
#include <utility>

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
			 * @see IsClosed()
			 */
			virtual ExpectedVoid<WriteError> 								Write(const std::size_t& count, const DataType& data) noexcept = 0;

			/**
			 * @note String convenience overloads
			 *
			 * These non-template overloads exist to ensure `std::string` / C-string
			 * inputs bind to a stable, well-defined implementation instead of the
			 * generic input-range template above. Without these entries, calls like
			 * `Write("abc")` or `Write(std::string{})` may select the range
			 * template which can lead to surprising template instantiation
			 * differences and SFINAE interactions. The overloads also avoid copying
			 * the trailing NUL for string-literal arrays by explicitly constructing a
			 * `std::string_view` of size `N-1`.
			 */

			/**
			 * @brief Write from a string view (does not include terminating NUL).
			 */
			ExpectedVoid<WriteError> 										Write(std::string_view sv) noexcept {
				DataType tmp;
				if (!sv.empty()) tmp.reserve(static_cast<typename DataType::size_type>(sv.size()));
				std::transform(sv.begin(), sv.end(), std::back_inserter(tmp), [] (char e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
			}

			/**
			 * @brief Write from a C string pointer (null-terminated). Uses `std::string_view`.
			 */
			ExpectedVoid<WriteError> 										Write(const char* s) noexcept {
				if (!s) return Write(DataType{});
				return Write(std::string_view(s));
			}

			/**
			 * @brief Write up-to `count` bytes from a string view (0 => all available).
			 */
			ExpectedVoid<WriteError> 										Write(const std::size_t& count, std::string_view sv) noexcept {
				size_t to_write = (count == 0) ? static_cast<size_t>(sv.size()) : std::min(count, static_cast<std::size_t>(sv.size()));
				DataType tmp;
				if (to_write > 0) tmp.reserve(static_cast<typename DataType::size_type>(to_write));
				std::transform(sv.begin(), sv.begin() + to_write, std::back_inserter(tmp), [] (char e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(to_write), std::move(tmp));
			}

			/**
			 * @brief Write up-to `count` bytes from a C string pointer (null-terminated).
			 */
			ExpectedVoid<WriteError> 										Write(const std::size_t& count, const char* s) noexcept {
				if (!s) return Write(count, DataType{});
				return Write(count, std::string_view(s));
			}

			/**
			 * @brief Write from a string literal (array) and avoid copying the trailing NUL.
			 */
			template<std::size_t N>
			ExpectedVoid<WriteError> 										Write(const char (&s)[N]) noexcept {
				// N includes the terminating NUL for string literals, so exclude it
				if (N == 0) return Write(DataType{});
				return Write(std::string_view(s, (N > 0) ? (N - 1) : 0));
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
			ExpectedVoid<WriteError> 								Write(const R& r) noexcept {
				DataType tmp;
				if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
					auto dist = std::ranges::distance(r);
					if (dist > 0) tmp.reserve(static_cast<typename DataType::size_type>(dist));
				}
				std::transform(std::ranges::begin(r), std::ranges::end(r), std::back_inserter(tmp),
						[] (auto&& e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
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
			ExpectedVoid<WriteError> 								Write(const std::size_t& count, const Rw& r) noexcept {
				if (count == 0) return Write(r);
				DataType tmp;
				if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
					auto dist = std::ranges::distance(r);
					if (dist > 0) tmp.reserve(static_cast<typename DataType::size_type>(std::min(dist, static_cast<decltype(dist)>(count))));
				}
				auto it = std::ranges::begin(r);
				auto end = std::ranges::end(r);
				std::size_t written = 0;
				for (; it != end && written < count; ++it, ++written) {
					tmp.push_back(static_cast<std::byte>(*it));
				}
				return Write(static_cast<std::size_t>(written), std::move(tmp));
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
			ExpectedVoid<WriteError> 								Write(const std::size_t& count, Rrw&& r) noexcept {
				using Dec = std::remove_cvref_t<Rrw>;
				if (count == 0) return Write(std::forward<Rrw>(r));
				if constexpr (std::same_as<Dec, DataType>) {
					// r is DataType; we can move and resize if needed
					DataType tmp = std::move(r);
					if (tmp.size() > count) tmp.resize(count);
					return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
				} else {
					DataType tmp;
					if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
						auto dist = std::ranges::distance(r);
						if (dist > 0) tmp.reserve(static_cast<typename DataType::size_type>(std::min(dist, static_cast<decltype(dist)>(count))));
					}
					auto it = std::ranges::begin(r);
					auto end = std::ranges::end(r);
					std::size_t written = 0;
					for (; it != end && written < count; ++it, ++written) {
						tmp.push_back(static_cast<std::byte>(*it));
					}
					return Write(static_cast<std::size_t>(written), std::move(tmp));
				}
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
			ExpectedVoid<WriteError> 								Write(Rr&& r) noexcept {
				using Dec = std::remove_cvref_t<Rr>;
				if constexpr (std::same_as<Dec, DataType>) {
					// r is already the library DataType (std::vector<std::byte>), forward to move overload
					return Write(static_cast<std::size_t>(r.size()), std::move(r));
				} else {
					DataType tmp;
					if constexpr (requires(DataType& d, typename DataType::size_type n) { d.reserve(n); }) {
						auto dist = std::ranges::distance(r);
						if (dist > 0) tmp.reserve(static_cast<typename DataType::size_type>(dist));
					}
					std::transform(std::ranges::begin(r), std::ranges::end(r), std::back_inserter(tmp),
						   [] (auto&& e) noexcept { return static_cast<std::byte>(e); });
					return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
				}
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
			ExpectedVoid<WriteError> 								Write(I first, S last) noexcept {
				DataType tmp;
				std::transform(first, last, std::back_inserter(tmp), [] (auto&& e) noexcept { return static_cast<std::byte>(e); });
				return Write(static_cast<std::size_t>(tmp.size()), std::move(tmp));
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
			ExpectedVoid<WriteError> 								Write(const std::size_t& count, I2 first, S2 last) noexcept {
				if (count == 0) return Write(first, last);
				DataType tmp;
				std::size_t written = 0;
				for (; first != last && written < count; ++first, ++written) {
					tmp.push_back(static_cast<std::byte>(*first));
				}
				return Write(static_cast<std::size_t>(written), std::move(tmp));
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
			virtual ExpectedVoid<WriteError> 								Write(const std::size_t& count, DataType&& data) noexcept = 0;

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param count Number of bytes to write.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			virtual ExpectedVoid<Error> 									Write(const std::size_t& count, const ReadOnly& data) noexcept = 0;

			/**
			 * @brief Write bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
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
			 * @see IsClosed()
			 */
			virtual ExpectedVoid<Error> 									Write(const std::size_t& count, ReadOnly&& data) noexcept = 0;

			/**
			 * @brief Move bytes from a vector to the buffer.
			 * @param data Byte vector to append to the WriteOnly.
			 * @return ExpectedVoid<WriteError> indicating success or failure.
			 * @details Appends data to the buffer, growing capacity automatically if needed.
			 *          Handles wrap-around efficiently. Ignores writes if buffer is closed.
			 * @see IsClosed()
			 */
			inline ExpectedVoid<Error> 										Write(ReadOnly&& data) noexcept {
				return Write(data.AvailableBytes(), std::move(data));
			}
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