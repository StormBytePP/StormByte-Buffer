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
     * @brief Multi-stage data processing pipeline with concurrent execution.
     *
     * @par Overview
     *  Pipeline orchestrates a sequence of transformation functions (PipeFunction) that process
     *  data through multiple stages. Each stage runs concurrently in its own detached thread,
     *  enabling parallel processing of data as it flows through the pipeline.
     *
     * @par Pipeline Functions
     *  Each pipeline function (PipeFunction) has the signature:
     *  @code
     *  void function(Consumer input, Producer output)
     *  @endcode
     *  - @b input: Consumer to read data from the previous stage (or initial input)
     *  - @b output: Producer to write processed data for the next stage
     *
     *  Functions should:
     *  - Read data from the input Consumer using Read() or Extract()
     *  - Process the data according to their transformation logic
     *  - Write results to the output Producer using Write()
     *  - Close the output Producer when finished to signal completion
     *
    * @par Execution Mode
    *  Behavior depends on @ref ExecutionMode passed to Process():
    *  - ExecutionMode::Async: (default concurrent style) Each pipeline function is launched
    *    in a separate detached thread. Stages execute concurrently, limited only by data
    *    availability and scheduling.
    *  - ExecutionMode::Sync: All pipeline functions execute sequentially in the caller's
    *    thread. Each stage must finish before the next begins; no detached threads created.
    *
    *  Async mode steps:
    *   1. Each pipeline function is launched in a separate detached thread
    *   2. All stages execute concurrently, limited only by data availability
    *   3. Data flows from stage to stage through thread-safe SharedFIFO buffers
    *   4. Each stage blocks on Read/Extract until data is available from the previous stage
    *   5. Stages can process data incrementally as it becomes available
     *
     * @par Thread Safety and Synchronization
     *  - All intermediate buffers are thread-safe SharedFIFO instances
     *  - Buffer lifetime is managed automatically via std::shared_ptr
     *  - Threads synchronize implicitly through blocking Read/Extract operations
     *  - No explicit synchronization primitives are needed in pipeline functions
     *
    * @par Data Flow Example (Async mode shown)
     *  @code
     *  Pipeline pipeline;
     *  
    *  // Stage 1: Read raw data and uppercase it
    *  pipeline.AddPipe([](Consumer in, Producer out) {
    *      while (!in.EoF()) {
     *          auto data = in.Extract(1024);
     *          if (data) {
     *              std::string str(reinterpret_cast<const char*>(data->data()), data->size());
     *              for (auto& c : str) c = std::toupper(c);
     *              out.Write(str);
     *          }
     *      }
     *      out.Close();
     *  });
     *  
    *  // Stage 2: Filter and write result
    *  pipeline.AddPipe([](Consumer in, Producer out) {
    *      while (!in.EoF()) {
     *          auto data = in.Extract(0);
     *          if (data && !data->empty()) {
     *              // Process and write filtered data
     *              out.Write(*data);
     *          }
     *      }
     *      out.Close();
     *  });
     *  
     *  // Process data through pipeline
     *  Producer input;
     *  input.Write("hello world");
     *  input.Close();
     *  
    *  Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async);
     *  auto final_data = result.Extract(0);
     *  @endcode
     *
    * @par Error Handling
    *  - Functions should handle errors internally
    *  - To signal errors, a stage can Close() or SetError() its output buffer
    *  - Subsequent stages detect completion via EoF() and can handle accordingly
    *  - Functions must not throw exceptions (undefined behavior)
     *
    * @par Best Practices
    *  - Always Close() the output Producer when a stage completes (or SetError() on failure)
    *  - Check EoF() on input Consumer to detect when previous stage finished
    *  - Use Extract(0) to read all available data without blocking
    *  - Use Extract(count) with count > 0 to block until specific amount available
    *  - Keep pipeline functions simple and focused on one transformation
    *  - Prefer Sync mode for debugging; Async for throughput
     *
    * @par Performance Considerations
    *  - Async: All stages run concurrently, maximizing throughput on multi-core systems
    *  - Sync : No thread creation overhead; deterministic ordering
    *  - Buffers grow automatically to accommodate data flow
    *  - Blocking operations minimize busy-waiting
    *  - Async detached threads mean pipeline setup returns immediately
     *
    * @warning Async: Pipeline functions run in detached threads. Ensure all captured data
    *          remains valid for the thread's lifetime (use value capture or shared_ptr).
    *          Sync: Functions run inline; standard lifetimes apply.
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

			// Sets error on all internal pipes which which make them to stop being writable and thus exit prematurely
			void 													SetError() noexcept;

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
            Consumer												Process(Consumer buffer, const ExecutionMode& mode, Logger::Log& log) noexcept;

        private:
            std::vector<PipeFunction> m_pipes;						///< Vector of pipe functions
			std::vector<Producer> m_producers;						///< Vector of intermediate consumers
			std::vector<std::thread> m_threads;						///< Vector of threads for execution

			/**
			 * @brief Wait for all pipeline threads to complete.
			 * @details Joins the last thread (if async mode) to ensure pipeline completion and clear threads for next run
			 */
			void 													WaitForCompletion();
    };
}