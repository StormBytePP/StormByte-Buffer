#pragma once

#include <StormByte/buffer/ring.hxx>

#include <memory>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 */
namespace StormByte::Buffer {
	/**
	 * @class Consumer
	 * @brief Read-only interface for consuming data from a shared Ring buffer.
	 *
	 * Multiple Consumer instances can share the same underlying Ring,
	 * allowing concurrent reads in a fully thread-safe manner.
	 * Consumers can only be created through a Producer.
	 *
	 * All operations are thread-safe and delegate to the shared Ring.
	 * Blocking semantics are identical to Ring (Read/Extract/Peek block
	 * until data is available or the buffer is closed/error).
	 */
	class STORMBYTE_BUFFER_PUBLIC Consumer final : public ReadOnly {
		friend class Producer;
	public:
		inline Consumer(const Consumer& other) noexcept : m_buffer(other.m_buffer) {}
		inline Consumer(Consumer&& other) noexcept : m_buffer(std::move(other.m_buffer)) {}
		~Consumer() noexcept = default;

		inline Consumer& operator=(const Consumer& other) noexcept {
			if (this != &other) m_buffer = other.m_buffer;
			return *this;
		}
		inline Consumer& operator=(Consumer&& other) noexcept {
			if (this != &other) m_buffer = std::move(other.m_buffer);
			return *this;
		}

		inline bool operator==(const Consumer& other) const noexcept {
			return m_buffer.get() == other.m_buffer.get();
		}
		inline bool operator!=(const Consumer& other) const noexcept {
			return !(*this == other);
		}

		inline std::size_t AvailableBytes() const noexcept override {
			return m_buffer->AvailableBytes();
		}
		inline void Clean() noexcept override { m_buffer->Clean(); }
		inline void Clear() noexcept override { m_buffer->Clear(); }
		inline void Close() noexcept { m_buffer->Close(); }

		inline const DataType& Data() const noexcept override {
			return m_buffer->Data();
		}

		inline bool Drop(const std::size_t& count) noexcept override {
			return m_buffer->Drop(count);
		}
		inline bool Empty() const noexcept override { return m_buffer->Empty(); }
		inline bool EoF() const noexcept override { return m_buffer->EoF(); }

		inline bool Extract(const std::size_t& count, DataType& out) noexcept override {
			return m_buffer->Extract(count, out);
		}
		inline bool Extract(const std::size_t& count, WriteOnly& out) noexcept override {
			return m_buffer->Extract(count, out);
		}
		inline void ExtractUntilEoF(DataType& out) noexcept override {
			m_buffer->ExtractUntilEoF(out);
		}
		inline void ExtractUntilEoF(WriteOnly& out) noexcept override {
			m_buffer->ExtractUntilEoF(out);
		}

		inline bool IsReadable() const noexcept override { return m_buffer->IsReadable(); }
		inline bool IsWritable() const noexcept { return m_buffer->IsWritable(); }
		inline bool HasError() const noexcept { return m_buffer->HasError(); }

		inline bool Read(const std::size_t& count, DataType& out) const noexcept override {
			return m_buffer->Read(count, out);
		}
		inline bool Read(const std::size_t& count, WriteOnly& out) const noexcept override {
			return m_buffer->Read(count, out);
		}
		inline void ReadUntilEoF(DataType& out) const noexcept override {
			m_buffer->ReadUntilEoF(out);
		}
		inline void ReadUntilEoF(WriteOnly& out) const noexcept override {
			m_buffer->ReadUntilEoF(out);
		}

		inline bool Peek(const std::size_t& count, DataType& out) const noexcept override {
			return m_buffer->Peek(count, out);
		}
		inline bool Peek(const std::size_t& count, WriteOnly& out) const noexcept override {
			return m_buffer->Peek(count, out);
		}

		inline void Seek(const std::ptrdiff_t& offset, const Position& mode) const noexcept override {
			m_buffer->Seek(offset, mode);
		}
		inline std::size_t Size() const noexcept override { return m_buffer->Size(); }

	private:
		std::shared_ptr<Ring> m_buffer;

		inline explicit Consumer(std::shared_ptr<Ring> buffer) noexcept : m_buffer(std::move(buffer)) {}
	};
}
