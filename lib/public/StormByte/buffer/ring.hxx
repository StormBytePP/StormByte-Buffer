#pragma once

#include <StormByte/buffer/generic.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <StormByte/string.hxx>

#include <condition_variable>
#include <deque>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <utility>

namespace StormByte::Buffer {

/**
 * @class Ring
 * @brief High-performance, highly-concurrent thread-safe ring buffer.
 *
 * Uses std::shared_mutex so multiple readers can proceed concurrently
 * while writers and destructive operations remain exclusive.
 * Critical sections are kept as short as possible.
 */
class STORMBYTE_BUFFER_PUBLIC Ring : public ReadWrite {
public:
	Ring() noexcept = default;

	inline explicit Ring(const DataType& data) noexcept
		: m_buffer(data.begin(), data.end()) {}

	inline explicit Ring(DataType&& data) noexcept
		: m_buffer(std::make_move_iterator(data.begin()),
				std::make_move_iterator(data.end())) {}

	template<std::ranges::input_range R>
	requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<R>>>) &&
			requires(std::ranges::range_value_t<R> v) { static_cast<std::byte>(v); } &&
			(!std::same_as<std::remove_cvref_t<R>, DataType>)
	inline explicit Ring(const R& r) noexcept {
		auto converted = DataConvert(r);
		m_buffer.assign(converted.begin(), converted.end());
	}

	template<std::ranges::input_range Rr>
	requires (!std::is_class_v<std::remove_cv_t<std::ranges::range_value_t<Rr>>>) &&
			requires(std::ranges::range_value_t<Rr> v) { static_cast<std::byte>(v); }
	inline explicit Ring(Rr&& r) noexcept {
		auto converted = DataConvert(std::forward<Rr>(r));
		m_buffer.assign(std::make_move_iterator(converted.begin()),
						std::make_move_iterator(converted.end()));
	}

	inline explicit Ring(std::string_view sv) noexcept {
		auto converted = DataConvert(sv);
		m_buffer.assign(converted.begin(), converted.end());
	}

	inline explicit Ring(const char* s) noexcept
		: Ring(s ? std::string_view(s) : std::string_view{}) {}

	Ring(const Ring&) = delete;
	Ring(Ring&& other) noexcept;
	virtual ~Ring() noexcept = default;

	Ring& operator=(const Ring&) = delete;
	Ring& operator=(Ring&& other) noexcept;

	bool operator==(const Ring& other) const noexcept;
	inline bool operator!=(const Ring& other) const noexcept { return !(*this == other); }

	std::size_t AvailableBytes() const noexcept override;
	void Clean() noexcept override;
	void Clear() noexcept override;
	void Close() noexcept;
	const DataType& Data() const noexcept override;
	bool Drop(const std::size_t& count) noexcept override;
	bool Empty() const noexcept override;
	bool EoF() const noexcept override;
	bool HasError() const noexcept;
	std::string HexDump(const std::size_t& columns = 16,
						const std::size_t& byte_limit = 0) const noexcept;
	bool IsReadable() const noexcept override;
	bool IsWritable() const noexcept override;

	bool Peek(const std::size_t& count, DataType& outBuffer) const noexcept override;
	bool Peek(const std::size_t& count, WriteOnly& outBuffer) const noexcept override;
	bool Read(const std::size_t& count, DataType& outBuffer) const noexcept override;
	bool Read(const std::size_t& count, WriteOnly& outBuffer) const noexcept override;
	void ReadUntilEoF(DataType& outBuffer) const noexcept override;
	void ReadUntilEoF(WriteOnly& outBuffer) const noexcept override;

	bool Extract(const std::size_t& count, DataType& outBuffer) noexcept override;
	bool Extract(const std::size_t& count, WriteOnly& outBuffer) noexcept override;
	void ExtractUntilEoF(DataType& outBuffer) noexcept override;
	void ExtractUntilEoF(WriteOnly& outBuffer) noexcept override;

	void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override;
	void SetError() noexcept;
	std::size_t Size() const noexcept override;

	bool Write(const std::size_t& count, const DataType& data) noexcept override;
	bool Write(const std::size_t& count, DataType&& data) noexcept override;
	bool Write(const std::size_t& count, const ReadOnly& data) noexcept override;
	bool Write(const std::size_t& count, ReadOnly&& data) noexcept override;

	using WriteOnly::Write;

protected:
	enum class Operation { Extract, Read, Peek };

	static std::string FormatHexLines(std::span<const std::byte> data,
									std::size_t start_offset,
									std::size_t columns) noexcept;

	virtual std::ostringstream HexDumpHeader() const noexcept;

	virtual bool ReadInternal(const std::size_t& count, DataType& outBuffer, Operation flag) noexcept;
	virtual bool ReadInternal(const std::size_t& count, WriteOnly& outBuffer, Operation flag) noexcept;
	virtual void ReadUntilEoFInternal(DataType& outBuffer, Operation flag) noexcept;
	virtual void ReadUntilEoFInternal(WriteOnly& outBuffer, Operation flag) noexcept;

	virtual bool WriteInternal(const std::size_t& count, const DataType& src) noexcept;
	virtual bool WriteInternal(const std::size_t& count, DataType&& src) noexcept;

private:
	std::deque<std::byte>               m_buffer;
	mutable std::size_t                 m_position_offset{0};
	bool                                m_closed{false};
	bool                                m_error{false};
	std::string                         m_error_message;

	mutable DataType                    m_data_cache;
	mutable std::shared_mutex           m_mutex;          // ← shared_mutex
	mutable std::condition_variable_any m_cv;

	void Wait(const std::size_t& n, std::unique_lock<std::shared_mutex>& lock) const;
};

}
