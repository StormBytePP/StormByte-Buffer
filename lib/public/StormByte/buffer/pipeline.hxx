#pragma once

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/typedefs.hxx>

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
	/**
	 * @class Pipeline
	 * @brief Multi-stage data-processing pipeline with optional concurrent execution.
	 *
	 * @par Overview
	 * Pipeline manages a sequence of transformation functions (PipeFunction) that move
	 * data through multiple stages. Each stage may run concurrently (Async mode) or
	 * sequentially (Sync mode). Intermediate buffers between stages are thread-safe
	 * SharedFIFO instances and are managed automatically by the pipeline.
	 *
	 * @par Pipe function signature
	 * A pipeline stage must be a callable with the signature:
	 * @code{.cpp}
	 * void stage_fn(Consumer input, Producer output);
	 * @endcode
	 * - Read from `input` using `Read()` / `Extract()`
	 * - Write processed bytes to `output` using `Write()`
	 * - Close or `SetError()` the `output` when the stage finishes
	 *
	 * @par Execution modes
	 * - `ExecutionMode::Async` (default): each stage is launched in its own detached
	 *   thread. Stages process data concurrently and communicate via SharedFIFO buffers.
	 * - `ExecutionMode::Sync` : stages run sequentially in the caller's thread; no
	 *   detached threads are created.
	 *
	 * @par Example (conceptual, Async mode)
	 * @code{.cpp}
	 * Pipeline pipeline;
	 * pipeline.AddPipe([](Consumer in, Producer out) {
	 *     while (!in.EoF()) {
	 *         auto data = in.Extract(1024);
	 *         if (data && !data->empty()) {
	 *             // transform data
	 *             out.Write(*data);
	 *         }
	 *     }
	 *     out.Close();
	 * });
	 * // Process input -> returns Consumer for final output
	 * Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logger);
	 * @endcode
	 *
	 * @par Error handling
	 * Stages should catch and handle errors locally. To propagate failure, a stage
	 * may call `SetError()` on its output buffer; downstream stages will observe
	 * the buffer's unreadable/closed state via `EoF()` and can react accordingly.
	 *
	 * @par Best practices
	 * - Always `Close()` your stage's `output` when finished (or call `SetError()` on failure).
	 * - Prefer simple, focused transformations per stage.
	 * - Use Sync mode for deterministic debugging; use Async for throughput on multi-core systems.
	 * - When using Async mode, ensure any captured data remains valid for the lifetime
	 *   of the detached thread (use value captures or `std::shared_ptr`).
	 *
	 * @see PipeFunction, Consumer, Producer, SharedFIFO, ExecutionMode
	 */
	class STORMBYTE_BUFFER_PUBLIC Pipeline final {
		public:
			/**
			 * @brief Default constructor
			 * Initializes an empty pipeline buffer.
			 */
			Pipeline() noexcept										= default;

			/**
			 * @brief Copy constructor
			 * Creates a new `Pipeline` that shares the same underlying buffer as the original.
			 * @param other Pipeline to copy
			 */
			Pipeline(const Pipeline& other);

			/**
			 * @brief Move constructor
			 * Moves the contents of another `Pipeline` into this instance.
			 * @param other Pipeline to move
			 */
			Pipeline(Pipeline&& other) noexcept 					= default;

			/**
			 * @brief Destructor
			 */
			~Pipeline() noexcept;

			/**
			 * @brief Copy assignment operator
			 * @param other `Pipeline` instance to copy from
			 * @return Reference to the updated `Pipeline` instance
			 */
			Pipeline& operator=(const Pipeline& other);

			/**
			 * @brief Move assignment operator
			 * @param other `Pipeline` instance to move from
			 * @return Reference to the updated `Pipeline` instance
			 */
			Pipeline& operator=(Pipeline&& other) noexcept			= default;

			/**
			 * @brief Add a processing stage to the pipeline.
			 * @param pipe Function to execute as a pipeline stage.
			 * @details Stages are executed in the order they are added. Each stage runs
			 *          in its own thread when Process() is called.
			 * @see PipeFunction, Process()
			 */
			void 													AddPipe(const PipeFunction& pipe);

			/**
			 * @brief Add a processing stage to the pipeline (move version).
			 * @param pipe Function to move into the pipeline.
			 * @details More efficient than copy when passing temporary functions or lambdas.
			 * @see AddPipe(const PipeFunction&)
			 */
			void 													AddPipe(PipeFunction&& pipe);

			/**
			 * @brief Mark all internal pipeline stages as errored, causing them to stop accepting writes.
			 *
			 * @details This notifies every internal pipe so they transition to an error state and become
			 *          unwritable; readers will observe end-of-data once existing buffered data is consumed.
			 *          The method is `const` because it performs thread-safe signaling on shared state, not
			 *          logical mutation of the pipeline object itself.
			 *
			 * @note The operation is thread-safe and may be called concurrently.
			 */
			void 													SetError() const noexcept;

			/**
			 * @brief Execute the pipeline on input data.
			 * @param buffer Consumer providing input data to the first pipeline stage.
			 * @param mode Execution mode: ExecutionMode::Async (concurrent detached threads) or
			 *             ExecutionMode::Sync (sequential execution in caller thread).
			 * @param log Logger instance for logging within pipeline stages.
			 * @return Consumer for reading the final output from the last pipeline stage.
			 * 
			 * @details Async: Launches all pipeline stages concurrently in detached threads. Sync:
			 *          Executes each stage sequentially; no new threads are created. Each stage:
			 *          - Reads data from the previous stage (or the input buffer for the first stage)
			 *          - Processes the data according to its transformation logic
			 *          - Writes results to a SharedFIFO buffer that feeds the next stage
			 *          The returned Consumer represents final stage output and can be read as data
			 *          becomes available (Async) or after preceding stages finish (Sync).
			 *
			 * @par Thread Execution
			 *          Async: Parallel processing across stages; implicit cleanup.
			 *          Sync : Deterministic ordering; no thread coordination required.
			 *
			 * @par Data Availability
			 *          Data becomes available in the output Consumer as the pipeline processes it:
			 *          - Extract(0) returns currently available data without blocking
			 *          - Extract(count) blocks until count bytes available or buffer unreadable
			 *          - EoF() returns true when no more data can be produced (unwritable & empty)
			 *
			 * @par Multiple Invocations
			 *          Each call creates new buffers; Async spawns threads, Sync reuses caller thread.
			 *          Executing Process more than once on the same Pipeline object while a previous
			 *          execution is still running is undefined behavior. Always wait until the prior
			 *          run has completed (e.g., the returned Consumer reaches EoF) before invoking
			 *          Process again on the same instance.
			 *
			 * @warning Async: Captured variables must remain valid for thread lifetime (use value capture/shared_ptr).
			 *          Sync : Standard lifetimes apply.
			 *
			 * @see AddPipe(), Consumer, Producer, ExecutionMode
			 */
			Consumer												Process(Consumer buffer, const ExecutionMode& mode, Logger::Log log) const noexcept;

		private:
			std::vector<PipeFunction> m_pipes;						///< Vector of pipe functions
			mutable std::vector<Producer> m_producers;				///< Vector of intermediate consumers
			mutable std::vector<std::thread> m_threads;				///< Vector of threads for execution

			/**
			 * @brief Wait for all pipeline threads to complete.
			 * @details Joins the last thread (if async mode) to ensure pipeline completion and clear threads for next run
			 */
			void 													WaitForCompletion() const noexcept;
	};
}