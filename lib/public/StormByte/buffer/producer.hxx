#pragma once

#include <StormByte/buffer/consumer.hxx>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 */
namespace StormByte::Buffer {
	/**
	 * @class Producer
	 * @brief Write-only interface for producing data into a shared Ring buffer.
	 *
	 * Multiple Producer instances can share the same underlying Ring,
	 * allowing concurrent writes in a fully thread-safe manner.
	 *
	 * All operations are thread-safe and delegate to the shared Ring.
	 */
	class STORMBYTE_BUFFER_PUBLIC Producer final : public WriteOnly {
		public:
			inline Producer() noexcept : m_buffer(std::make_shared<Ring>()) {}

			inline explicit Producer(std::shared_ptr<Ring> buffer) noexcept : m_buffer(std::move(buffer)) {}

			inline Producer(const Consumer& consumer) noexcept : m_buffer(consumer.m_buffer) {}

			inline Producer(const Producer& other) noexcept : m_buffer(other.m_buffer) {}
			inline Producer(Producer&& other) noexcept : m_buffer(std::move(other.m_buffer)) {}
			~Producer() noexcept = default;

			inline Producer& operator=(const Producer& other) noexcept {
				if (this != &other) m_buffer = other.m_buffer;
				return *this;
			}
			inline Producer& operator=(Producer&& other) noexcept {
				if (this != &other) m_buffer = std::move(other.m_buffer);
				return *this;
			}

			inline bool operator==(const Producer& other) const noexcept {
				return m_buffer.get() == other.m_buffer.get();
			}
			inline bool operator!=(const Producer& other) const noexcept {
				return !(*this == other);
			}

			inline void Close() noexcept { m_buffer->Close(); }
			inline void SetError() noexcept { m_buffer->SetError(); }

			inline bool IsWritable() const noexcept override {
				return m_buffer->IsWritable();
			}

			inline bool Write(const std::size_t& count, const DataType& data) noexcept override {
				return m_buffer->Write(count, data);
			}
			inline bool Write(const DataType& data) noexcept {
				return Write(data.size(), data);
			}
			inline bool Write(const std::size_t& count, DataType&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}
			inline bool Write(DataType&& data) noexcept {
				return Write(data.size(), std::move(data));
			}
			inline bool Write(const std::size_t& count, const ReadOnly& data) noexcept override {
				return m_buffer->Write(count, data);
			}
			inline bool Write(const std::size_t& count, ReadOnly&& data) noexcept override {
				return m_buffer->Write(count, std::move(data));
			}

			using WriteOnly::Write;

			/**
			 * @brief Create a Consumer that shares this Producer’s Ring.
			 */
			inline class Consumer Consumer() {
				return StormByte::Buffer::Consumer{ m_buffer };
			}

		protected:
			std::shared_ptr<Ring> m_buffer;
	};
}
