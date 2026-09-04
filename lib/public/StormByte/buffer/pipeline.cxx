/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * or later, as published by the Free Software Foundation.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 */

// ============================================================================
// lib/public/StormByte/buffer/pipeline.cxx
// ============================================================================
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
struct Pipeline::Impl {
	std::vector<PipeFunction>                          pipes;          ///< Ordered list of stages.
	mutable std::vector<std::unique_ptr<LockFreeRing>> intermediates;  ///< Buffers between stages.
	mutable Producer                                   final_producer; ///< Final public output.
	mutable std::vector<std::thread>                   threads;        ///< Background workers.

	/**
	 * @brief Join any running background threads and clear the container.
	 */
	void WaitForCompletion() const noexcept {
		for (auto& t : threads) {
			if (t.joinable())
				t.join();
		}
		threads.clear();
	}
};

// ---------------------------------------------------------------------------
// Construction / destruction / assignment
// ---------------------------------------------------------------------------

Pipeline::Pipeline() noexcept
	: m_impl(std::make_unique<Impl>())
{
}

Pipeline::Pipeline(const Pipeline& other)
	: m_impl(std::make_unique<Impl>())
{
	m_impl->pipes = other.m_impl->pipes;
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

	// One LockFreeRing between each pair of consecutive stages (SPSC)
	m_impl->intermediates.clear();
	m_impl->intermediates.reserve(num_stages > 1 ? num_stages - 1 : 0);
	for (std::size_t i = 0; i + 1 < num_stages; ++i)
		m_impl->intermediates.emplace_back(std::make_unique<LockFreeRing>());

	// Final public output (Ring-backed Producer)
	m_impl->final_producer = Producer();
	m_impl->threads.clear();

	const bool parallel = HasExecutionFlag(mode, ExecutionMode::Parallel);
	const bool async    = HasExecutionFlag(mode, ExecutionMode::Async);

	/**
	 * @brief Run a single stage @p i.
	 * @param i     Stage index in @c pipes.
	 * @param input Original input Consumer (used only when @p i == 0).
	 */
	auto run_one_stage = [this, log, num_stages](std::size_t i, Consumer& input) {
		// Input: first stage ← original Consumer; others ← previous LockFreeRing
		ExternalBufferReader in_adapter =
			(i == 0)
				? ExternalBufferReader(static_cast<ReadOnly&>(input))
				: ExternalBufferReader(*m_impl->intermediates[i - 1]);

		if (i + 1 == num_stages) {
			// Last stage writes into the public final Producer
			ExternalBufferWriter out_adapter(m_impl->final_producer);
			m_impl->pipes[i](in_adapter, out_adapter, log);
			// Stage is expected to call out.Close() or out.SetError()
		} else {
			// Intermediate stage writes into the next LockFreeRing
			ExternalBufferWriter out_adapter(*m_impl->intermediates[i]);
			m_impl->pipes[i](in_adapter, out_adapter, log);
		}
	};

	/**
	 * @brief Run all stages sequentially on the current thread.
	 */
	auto run_stages_sequential =
		[run_one_stage, buffer = buffer, num_stages]() mutable {
			for (std::size_t i = 0; i < num_stages; ++i)
				run_one_stage(i, buffer);
		};

	if (parallel) {
		// One thread per stage (pipeline parallelism).
		// Each intermediate remains SPSC: stage i is the sole writer of
		// intermediates[i], stage i+1 the sole reader.
		Consumer input = buffer; // shared handle to the same Ring
		m_impl->threads.reserve(num_stages);
		for (std::size_t i = 0; i < num_stages; ++i) {
			m_impl->threads.emplace_back(
				[run_one_stage, i, input]() mutable {
					run_one_stage(i, input);
				});
		}
		if (!async) {
			// Parallel without Async → block until all stages complete
			m_impl->WaitForCompletion();
		}
	} else if (async) {
		// Single background worker; stages run in order; non-blocking return
		m_impl->threads.emplace_back(std::move(run_stages_sequential));
	} else {
		// Sync (0): sequential on the caller’s thread
		run_stages_sequential();
	}

	// Final Consumer is available immediately when Async is set, or after
	// completion when Process blocks (Sync / Parallel-only).
	return m_impl->final_producer.Consumer();
}
