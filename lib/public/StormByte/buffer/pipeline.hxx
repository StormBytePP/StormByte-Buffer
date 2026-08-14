#pragma once

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/typedefs.hxx>

#include <functional>
#include <memory>
#include <thread>
#include <vector>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, external I/O adapters and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	// Forward declaration – LockFreeRing is private and not installed
	class LockFreeRing;

	/**
	 * @class Pipeline
	 * @brief High-performance multi-stage data-processing pipeline.
	 *
	 * @par Overview
	 * Pipeline manages a sequence of transformation functions (stages).
	 * Each stage receives abstract @ref ExternalReader / @ref ExternalWriter
	 * interfaces, allowing the Pipeline to freely choose the concrete buffer
	 * implementation for every intermediate step.
	 *
	 * @par Buffer strategy
	 * - **Intermediate stages** use the private high-performance
	 *   @c LockFreeRing (SPSC lock-free circular buffer).
	 * - **Final stage** writes into a normal public @ref Producer (backed by
	 *   @ref Ring), so the @ref Consumer returned to the caller keeps the
	 *   full public API and can be shared safely.
	 *
	 * @par Execution modes
	 * - @c ExecutionMode::Async (recommended for production):
	 *   A **single background thread** executes all stages sequentially.
	 *   @ref Process() returns immediately with the final @ref Consumer.
	 *   Extremely low overhead even with dozens of stages.
	 * - @c ExecutionMode::Sync:
	 *   All stages run sequentially in the caller’s thread.
	 *   Useful for deterministic debugging.
	 *
	 * @par Stage signature
	 * @code{.cpp}
	 * void stage(ExternalReader& in, ExternalWriter& out,
	 *            std::shared_ptr<Logger::Log> log);
	 * @endcode
	 *
	 * @par Best practices
	 * - Always call @c out.Close() (or @c out.SetError()) at the end of every stage.
	 * - Prefer Async mode for real workloads.
	 * - The returned @ref Consumer is the only synchronization point the
	 *   caller needs.
	 *
	 * @see ExternalReader, ExternalWriter, Producer, Consumer, LockFreeRing
	 */
	class STORMBYTE_BUFFER_PUBLIC Pipeline final {
		public:
			/**
			 * @brief Signature of a pipeline stage.
			 *
			 * Stages receive abstract reader/writer interfaces so the Pipeline
			 * can freely choose the concrete buffer implementation (LockFreeRing
			 * for intermediates, Ring for the final output, etc.).
			 *
			 * @param in  Abstract reader for the stage input.
			 * @param out Abstract writer for the stage output.
			 * @param log Optional logger (may be null).
			 */
			using PipeFunction = std::function<void(
				ExternalReader&,
				ExternalWriter&,
				std::shared_ptr<Logger::Log>
			)>;

			/**
			 * @brief Default constructor.
			 */
			Pipeline() noexcept;

			/**
			 * @brief Copy constructor.
			 * @param other Source pipeline (only the list of stages is copied).
			 */
			Pipeline(const Pipeline& other);

			/**
			 * @brief Move constructor.
			 * @param other Source pipeline (left in a valid but unspecified state).
			 */
			Pipeline(Pipeline&& other) noexcept;

			/**
			 * @brief Destructor.
			 * @details Waits for any running Async execution to finish.
			 */
			~Pipeline() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Source pipeline.
			 * @return Reference to this pipeline.
			 */
			Pipeline& operator=(const Pipeline& other);

			/**
			 * @brief Move assignment.
			 * @param other Source pipeline.
			 * @return Reference to this pipeline.
			 */
			Pipeline& operator=(Pipeline&& other) noexcept;

			/**
			 * @brief Add a processing stage (copy).
			 * @param pipe Stage function.
			 */
			void AddPipe(const PipeFunction& pipe);

			/**
			 * @brief Add a processing stage (move).
			 * @param pipe Stage function.
			 */
			void AddPipe(PipeFunction&& pipe);

			/**
			 * @brief Propagate error state to all internal buffers.
			 * @details Calls @c SetError() on every intermediate LockFreeRing and
			 *          on the final Producer. Waiting stages will wake up and
			 *          observe the error condition.
			 */
			void SetError() const noexcept;

			/**
			 * @brief Execute the pipeline.
			 * @param buffer Input Consumer for the first stage.
			 * @param mode   @c ExecutionMode::Async or @c ExecutionMode::Sync.
			 * @param log    Optional logger passed to every stage (may be null).
			 * @return Consumer of the final stage.
			 *         In Async mode the Consumer is available immediately;
			 *         the background thread continues processing.
			 *
			 * @note Any previous Async run is joined before starting a new one.
			 */
			Consumer Process(Consumer buffer,
							const ExecutionMode& mode,
							std::shared_ptr<Logger::Log> log) const noexcept;

		private:
			struct Impl;                       ///< Private implementation (PIMPL).
			std::unique_ptr<Impl> m_impl;      ///< Opaque pointer to implementation.
	};
}
