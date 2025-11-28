#pragma once

#include <StormByte/buffer/consumer.hxx>
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/typedefs.hxx>

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
     * @par Execution Model
     *  When Process() is called:
     *  1. Each pipeline function is launched in a separate detached thread
     *  2. All stages execute concurrently, limited only by data availability
     *  3. Data flows from stage to stage through thread-safe SharedFIFO buffers
     *  4. Each stage blocks on Read/Extract until data is available from the previous stage
     *  5. Stages can process data incrementally as it becomes available
     *
     * @par Thread Safety and Synchronization
     *  - All intermediate buffers are thread-safe SharedFIFO instances
     *  - Buffer lifetime is managed automatically via std::shared_ptr
     *  - Threads synchronize implicitly through blocking Read/Extract operations
     *  - No explicit synchronization primitives are needed in pipeline functions
     *
     * @par Data Flow Example
     *  @code
     *  Pipeline pipeline;
     *  
     *  // Stage 1: Read raw data and uppercase it
     *  pipeline.AddPipe([](Consumer in, Producer out) {
     *      while (!in.IsClosed()) {
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
     *      while (!in.IsClosed()) {
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
     *  Consumer result = pipeline.Process(input.Consumer());
     *  auto final_data = result.Extract(0);
     *  @endcode
     *
     * @par Error Handling
     *  - Functions should handle errors internally
     *  - To signal errors, functions can close their output buffer early
     *  - Subsequent stages will detect closure via IsClosed() and can handle accordingly
     *  - Functions must not throw exceptions (undefined behavior)
     *
     * @par Best Practices
     *  - Always close the output Producer when a stage completes
     *  - Check IsClosed() on input Consumer to detect when previous stage finished
     *  - Use Extract(0) to read all available data without blocking
     *  - Use Extract(count) with count > 0 to block until specific amount available
     *  - Keep pipeline functions simple and focused on one transformation
     *
     * @par Performance Considerations
     *  - All stages run concurrently, maximizing throughput on multi-core systems
     *  - Buffers grow automatically to accommodate data flow
     *  - Blocking operations minimize busy-waiting
     *  - Detached threads mean pipeline setup returns immediately
     *
     * @warning Pipeline functions run in detached threads. Ensure all captured data
     *          remains valid for the thread's lifetime. Use value capture or shared_ptr
     *          for safety.
     *
     * @see PipeFunction, Consumer, Producer, SharedFIFO
     */
    class STORMBYTE_BUFFER_PUBLIC Pipeline final {
        public:
            /**
             * @brief Default constructor
             * Initializes an empty pipeline buffer.
             */
            Pipeline() noexcept												= default;

            /**
             * @brief Copy constructor
             * Creates a new `Pipeline` that shares the same underlying buffer as the original.
             * @param other Pipeline to copy
             */
            Pipeline(const Pipeline& other) 								= default;

            /**
             * @brief Move constructor
             * Moves the contents of another `Pipeline` into this instance.
             * @param other Pipeline to move
             */
            Pipeline(Pipeline&& other) noexcept 							= default;

            /**
             * @brief Destructor
             */
            ~Pipeline() noexcept 											= default;

            /**
             * @brief Copy assignment operator
             * @param other `Pipeline` instance to copy from
             * @return Reference to the updated `Pipeline` instance
             */
            Pipeline& operator=(const Pipeline& other)						= default;

            /**
             * @brief Move assignment operator
             * @param other `Pipeline` instance to move from
             * @return Reference to the updated `Pipeline` instance
             */
            Pipeline& operator=(Pipeline&& other) noexcept					= default;

            /**
             * @brief Add a processing stage to the pipeline.
             * @param pipe Function to execute as a pipeline stage.
             * @details Stages are executed in the order they are added. Each stage runs
             *          in its own thread when Process() is called.
             * @see PipeFunction, Process()
             */
            void 															AddPipe(const PipeFunction& pipe);

            /**
             * @brief Add a processing stage to the pipeline (move version).
             * @param pipe Function to move into the pipeline.
             * @details More efficient than copy when passing temporary functions or lambdas.
             * @see AddPipe(const PipeFunction&)
             */
            void 															AddPipe(PipeFunction&& pipe);

            /**
             * @brief Execute the pipeline on input data.
             * @param buffer Consumer providing input data to the first pipeline stage.
             * @return Consumer for reading the final output from the last pipeline stage.
             * 
             * @details This method launches all pipeline stages concurrently in separate detached threads.
             *          Each stage:
             *          - Reads data from the previous stage (or the input buffer for the first stage)
             *          - Processes the data according to its transformation logic
             *          - Writes results to a SharedFIFO buffer that feeds the next stage
             *          
             *          The method returns immediately after launching all threads. The returned Consumer
             *          represents the output of the final pipeline stage. Callers can read from this
             *          Consumer to retrieve processed results as they become available.
             *
             * @par Thread Execution
             *          All stages run concurrently in detached threads. This means:
             *          - The pipeline processes data in parallel across all stages
             *          - Earlier stages can continue producing while later stages consume
             *          - Threads are automatically cleaned up when they complete
             *          - No explicit thread joining is required
             *
             * @par Data Availability
             *          Data becomes available in the output Consumer as the pipeline processes it:
             *          - Extract(0) returns all currently available data without blocking
             *          - Extract(count) blocks until count bytes are available or the buffer closes
             *          - IsClosed() returns true when the final stage has completed
             *
             * @par Multiple Invocations
             *          Process() can be called multiple times with different inputs. Each call creates
             *          a new set of threads and buffers, allowing independent pipeline executions.
             *
             * @warning Captured variables in pipeline functions must remain valid for the thread's
             *          lifetime. Use value capture or shared_ptr to avoid dangling references.
             *
             * @see AddPipe(), Consumer, Producer
             */
            Consumer														Process(Consumer buffer) const noexcept;

        private:
            std::vector<PipeFunction> m_pipes;								///< Vector of pipe functions
    };
}