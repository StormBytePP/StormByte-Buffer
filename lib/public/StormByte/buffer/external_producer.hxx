#pragma once

#include <StormByte/buffer/producer.hxx>

#include <thread>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	class STORMBYTE_BUFFER_PUBLIC ExternalProducer final: protected Producer {
		public:
			ExternalProducer(const ExternalReader& reader) noexcept;
			ExternalProducer(const ExternalProducer&) = delete;
			ExternalProducer(ExternalProducer&&) = default;
			~ExternalProducer();
			ExternalProducer& operator=(const ExternalProducer&) = delete;
			ExternalProducer& operator=(ExternalProducer&&) = default;
			bool operator==(const ExternalProducer& other) const noexcept = delete;
			bool operator!=(const ExternalProducer& other) const noexcept = delete;

			using Producer::SetError;
			using Producer::IsWritable;
			using Producer::Consumer;

		private:
			ExternalReader m_external_reader;
			std::thread m_rw_thread;
	};
}