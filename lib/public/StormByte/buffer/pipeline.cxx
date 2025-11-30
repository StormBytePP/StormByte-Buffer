#include <StormByte/buffer/consumer.hxx>
#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/buffer/producer.hxx>

using namespace StormByte::Buffer;

Pipeline::Pipeline(const Pipeline& other): m_pipes(other.m_pipes), m_producers(other.m_producers) {
	m_threads.reserve(m_pipes.size() + 1);
}

Pipeline::~Pipeline() noexcept {
	WaitForCompletion();
}

Pipeline& Pipeline::operator=(const Pipeline& other) {
	if (this != &other) {
		m_pipes = other.m_pipes;
		m_producers = other.m_producers;
		WaitForCompletion();
		m_threads.clear();
		m_threads.reserve(m_pipes.size());
	}
	return *this;
}

void Pipeline::AddPipe(const PipeFunction& pipe) {
	m_pipes.push_back(pipe);
	m_threads.reserve(m_pipes.size() + 1);
}

void Pipeline::AddPipe(PipeFunction&& pipe) {
	m_pipes.push_back(std::move(pipe));
	m_threads.reserve(m_pipes.size() + 1);
}

void Pipeline::SetError() noexcept {
	for (auto& producer : m_producers) {
		producer.SetError();
	}
}

Consumer Pipeline::Process(Consumer buffer, const ExecutionMode& mode, Logger::Log log) noexcept {
	// This guards double calls and do not harm in the first call
	WaitForCompletion();

	// Use pre-created producers corresponding to each pipe
	if (m_pipes.empty()) {
		// If there are not any stages, we do a passthrough
		return buffer;
	}

	// Reset producers to ensure a fresh run when reusing the pipeline
	m_producers.clear();
	m_producers.resize(m_pipes.size());
	for (auto& prod : m_producers) {
		prod = Producer();
	}

	// Prepare storage for worker threads. We'll create threads for the first
	// N-1 stages and store them in `m_threads` so we can join them later.
	// This avoids creating detached threads that can cause sanitizer-reported
	// leaks if they are still running at program exit.
	m_threads.clear();
	m_threads.reserve(m_pipes.size());

	for (std::size_t i = 0; i < m_pipes.size(); ++i) {
		Consumer stage_in = (i == 0) ? buffer : m_producers[i - 1].Consumer();
		Producer stage_out = m_producers[i];

		// First N-1 stages: create a background thread and store it.
		if (i < m_pipes.size() - 1) {
			m_threads.emplace_back([pipe = m_pipes[i], in = stage_in, out = stage_out, &log]() mutable {
				pipe(in, out, log);
			});
			continue;
		}

		// Last stage: detached/threaded only for Async; for Sync run inline.
		if (mode == ExecutionMode::Async) {
			m_threads.emplace_back([pipe = m_pipes[i], in = stage_in, out = stage_out, &log]() mutable {
				pipe(in, out, log);
			});
		} else {
			// Run last stage inline for Sync semantics. After returning from
			// this call we join all worker threads to ensure deterministic
			// completion.
			m_pipes[i](stage_in, stage_out, log);
			for (auto &t : m_threads) {
				if (t.joinable()) t.join();
			}
			m_threads.clear();
		}
	}

	return m_producers.back().Consumer();
}

void Pipeline::WaitForCompletion() {
	for (std::size_t i = 0; i < m_threads.size(); ++i) {
		if (m_threads[i].joinable()) {
			m_threads[i].join();
		}
	}
	m_threads.clear();
	m_threads.reserve(m_pipes.size());
}