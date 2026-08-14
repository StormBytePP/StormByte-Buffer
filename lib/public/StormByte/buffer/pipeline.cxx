#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/buffer/lockfree_ring.hxx>   // private header
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/external.hxx>

#include <thread>
#include <vector>

using namespace StormByte::Buffer;

// ---------------------------------------------------------------------------
// PIMPL
// ---------------------------------------------------------------------------

/**
 * @brief Private implementation of Pipeline.
 *
 * Holds the list of stages, intermediate LockFreeRings, the final public
 * Producer and the (at most one) background thread used in Async mode.
 */
struct Pipeline::Impl {
	std::vector<PipeFunction>                          pipes;          ///< Ordered list of stages.
	mutable std::vector<std::unique_ptr<LockFreeRing>> intermediates;  ///< Buffers between stages.
	mutable Producer                                   final_producer; ///< Final public output.
	mutable std::vector<std::thread>                   threads;        ///< Background thread (size ≤ 1).

	/**
	 * @brief Join any running background thread and clear the container.
	 */
	void WaitForCompletion() const noexcept {
		for (auto& t : threads) {
			if (t.joinable())
				t.join();
		}
		threads.clear();
		threads.reserve(1);
	}
};

// ---------------------------------------------------------------------------
// Construction / destruction / assignment
// ---------------------------------------------------------------------------

Pipeline::Pipeline() noexcept
	: m_impl(std::make_unique<Impl>())
{
	m_impl->threads.reserve(1);
}

Pipeline::Pipeline(const Pipeline& other)
	: m_impl(std::make_unique<Impl>())
{
	m_impl->pipes = other.m_impl->pipes;
	m_impl->threads.reserve(1);
}

Pipeline::Pipeline(Pipeline&& other) noexcept
	: m_impl(std::move(other.m_impl))
{
}

Pipeline::~Pipeline() noexcept {
	if (m_impl)
		m_impl->WaitForCompletion();
}

Pipeline& Pipeline::operator=(const Pipeline& other) {
	if (this != &other) {
		if (m_impl)
			m_impl->WaitForCompletion();
		m_impl = std::make_unique<Impl>();
		m_impl->pipes = other.m_impl->pipes;
		m_impl->threads.reserve(1);
	}
	return *this;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
	if (this != &other) {
		if (m_impl)
			m_impl->WaitForCompletion();
		m_impl = std::move(other.m_impl);
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Stage management
// ---------------------------------------------------------------------------

void Pipeline::AddPipe(const PipeFunction& pipe) {
	m_impl->pipes.push_back(pipe);
}

void Pipeline::AddPipe(PipeFunction&& pipe) {
	m_impl->pipes.push_back(std::move(pipe));
}

// ---------------------------------------------------------------------------
// Error propagation
// ---------------------------------------------------------------------------

void Pipeline::SetError() const noexcept {
	for (auto& buf : m_impl->intermediates) {
		if (buf)
			buf->SetError();
	}
	m_impl->final_producer.SetError();
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

Consumer Pipeline::Process(Consumer buffer,
						const ExecutionMode& mode,
						std::shared_ptr<Logger::Log> log) const noexcept
{
	// Ensure any previous run has finished
	m_impl->WaitForCompletion();

	if (m_impl->pipes.empty()) {
		// Empty pipeline → pure passthrough
		return buffer;
	}

	const std::size_t num_stages = m_impl->pipes.size();

	// Create one LockFreeRing between each pair of stages
	m_impl->intermediates.clear();
	m_impl->intermediates.reserve(num_stages > 1 ? num_stages - 1 : 0);
	for (std::size_t i = 0; i + 1 < num_stages; ++i)
		m_impl->intermediates.emplace_back(std::make_unique<LockFreeRing>());

	// Final public output (Ring-backed Producer)
	m_impl->final_producer = Producer();

	m_impl->threads.clear();

	/**
	 * Lambda that runs all stages sequentially.
	 * Captures the input Consumer by move and the shared logger.
	 */
	auto run_stages = [this, buffer = std::move(buffer), log, num_stages]() mutable {
		for (std::size_t i = 0; i < num_stages; ++i) {
			// ---- Input side ----
			// First stage reads from the original Consumer;
			// subsequent stages read from the previous LockFreeRing.
			ExternalBufferReader in_adapter =
				(i == 0)
					? ExternalBufferReader(static_cast<ReadOnly&>(buffer))
					: ExternalBufferReader(*m_impl->intermediates[i - 1]);

			// ---- Output side ----
			if (i + 1 == num_stages) {
				// Last stage writes into the public final Producer
				ExternalBufferWriter out_adapter(m_impl->final_producer);
				m_impl->pipes[i](in_adapter, out_adapter, log);
				// Stage is expected to call out.Close()
			} else {
				// Intermediate stage writes into the next LockFreeRing
				ExternalBufferWriter out_adapter(*m_impl->intermediates[i]);
				m_impl->pipes[i](in_adapter, out_adapter, log);
				// Stage is expected to call out.Close()
			}
		}
	};

	if (mode == ExecutionMode::Async) {
		// Fire-and-forget single background thread
		m_impl->threads.emplace_back(std::move(run_stages));
	} else {
		// Synchronous execution in the caller thread
		run_stages();
	}

	// The final Consumer is available immediately (Async) or after the
	// last stage has finished (Sync).
	return m_impl->final_producer.Consumer();
}
