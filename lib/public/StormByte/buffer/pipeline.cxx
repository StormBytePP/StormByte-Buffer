#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/buffer/producer.hxx>

using namespace StormByte::Buffer;

Pipeline::Pipeline(const Pipeline& other)
	: m_pipes(other.m_pipes)
	, m_producers(other.m_producers)
{
	m_threads.reserve(1);
}

Pipeline::~Pipeline() noexcept {
	WaitForCompletion();
}

Pipeline& Pipeline::operator=(const Pipeline& other) {
	if (this != &other) {
		WaitForCompletion();
		m_pipes     = other.m_pipes;
		m_producers = other.m_producers;
		m_threads.clear();
		m_threads.reserve(1);
	}
	return *this;
}

void Pipeline::AddPipe(const PipeFunction& pipe) {
	m_pipes.push_back(pipe);
}

void Pipeline::AddPipe(PipeFunction&& pipe) {
	m_pipes.push_back(std::move(pipe));
}

void Pipeline::SetError() const noexcept {
	for (auto& producer : m_producers) {
		producer.SetError();
	}
}

Consumer Pipeline::Process(Consumer buffer,
						const ExecutionMode& mode,
						std::shared_ptr<Logger::Log> log) const noexcept
{
	// Ensure any previous run has finished
	WaitForCompletion();

	if (m_pipes.empty()) {
		return buffer; // passthrough
	}

	// Create one Producer (Ring) per stage
	m_producers.clear();
	m_producers.resize(m_pipes.size());
	for (auto& p : m_producers) {
		p = Producer();
	}

	m_threads.clear();

	if (mode == ExecutionMode::Async) {
		// ---------------------------------------------------------------
		// Extremely efficient path: single background thread runs
		// the entire pipeline sequentially.
		// ---------------------------------------------------------------
		m_threads.emplace_back([this, buffer = std::move(buffer), log]() mutable {
			for (std::size_t i = 0; i < m_pipes.size(); ++i) {
				Consumer in  = (i == 0) ? buffer : m_producers[i - 1].Consumer();
				Producer out = m_producers[i];
				m_pipes[i](in, out, log);
			}
		});
	}
	else {
		// ---------------------------------------------------------------
		// Sync path: run everything in the caller thread
		// ---------------------------------------------------------------
		for (std::size_t i = 0; i < m_pipes.size(); ++i) {
			Consumer in  = (i == 0) ? buffer : m_producers[i - 1].Consumer();
			Producer out = m_producers[i];
			m_pipes[i](in, out, log);
		}
	}

	// The final Consumer is available immediately (Async) or after
	// the last stage finished (Sync).
	return m_producers.back().Consumer();
}

void Pipeline::WaitForCompletion() const noexcept {
	for (auto& t : m_threads) {
		if (t.joinable()) {
			t.join();
		}
	}
	m_threads.clear();
	m_threads.reserve(1);
}
