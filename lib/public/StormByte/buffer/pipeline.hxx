#pragma once

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <thread>
#include <vector>

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
	 * @class Pipeline
	 * @brief High-performance multi-stage data-processing pipeline.
	 *
	 * @par Overview
	 * Pipeline manages a sequence of transformation functions (PipeFunction).
	 * Intermediate buffers between stages are thread-safe Ring instances
	 * managed automatically by the pipeline.
	 *
	 * @par Execution modes
	 * - `ExecutionMode::Async` (recommended for production):
	 *   A **single background thread** executes all stages sequentially.
	 *   `Process()` returns immediately with the final Consumer.
	 *   Extremely low overhead even with dozens of stages. Ideal for
	 *   producer/consumer patterns where only the final result matters.
	 *
	 * - `ExecutionMode::Sync`:
	 *   All stages run sequentially in the caller’s thread.
	 *   Useful for deterministic debugging.
	 *
	 * @par Pipe function signature
	 * @code{.cpp}
	 * void stage_fn(Consumer input, Producer output, std::shared_ptr<Logger::Log> log);
	 * @endcode
	 *
	 * @par Best practices
	 * - Always `Close()` (or `SetError()`) the output of every stage.
	 * - Prefer Async mode for real workloads.
	 * - The returned Consumer is the only synchronization point you need.
	 */
	class STORMBYTE_BUFFER_PUBLIC Pipeline final {
		public:
			Pipeline() noexcept = default;

			Pipeline(const Pipeline& other);
			Pipeline(Pipeline&& other) noexcept = default;
			~Pipeline() noexcept;

			Pipeline& operator=(const Pipeline& other);
			Pipeline& operator=(Pipeline&& other) noexcept = default;

			void AddPipe(const PipeFunction& pipe);
			void AddPipe(PipeFunction&& pipe);

			/**
			 * @brief Propagate error state to all internal producers.
			 */
			void SetError() const noexcept;

			/**
			 * @brief Execute the pipeline.
			 * @param buffer Input Consumer for the first stage.
			 * @param mode   Async (single background thread) or Sync (caller thread).
			 * @param log    Optional logger passed to every stage.
			 * @return Consumer of the final stage (available immediately in Async mode).
			 */
			Consumer Process(Consumer buffer,
							const ExecutionMode& mode,
							std::shared_ptr<Logger::Log> log) const noexcept;

		private:
			std::vector<PipeFunction>          m_pipes;
			mutable std::vector<Producer>      m_producers;
			mutable std::vector<std::thread>   m_threads;   // only used in Async (size ≤ 1)

			void WaitForCompletion() const noexcept;
	};
}
