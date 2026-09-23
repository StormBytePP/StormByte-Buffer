/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Buffer source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-Logger tree and
 * the rest of the StormByte suite it vendors), which remains under its own
 * license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

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
 * @namespace StormByte::Buffer
 * @brief Buffer module of the StormByte suite.
 */
namespace StormByte::Buffer {
	/**
	 * @brief Forward declaration of the private SPSC ring used between stages.
	 * @note @ref LockFreeRing is not installed as a public header.
	 */
	class LockFreeRing;

	/**
	 * @class Pipeline
	 * @brief High-performance multi-stage data-processing pipeline.
	 *
	 * @par Overview
	 * Pipeline manages a sequence of transformation functions (stages).
	 * Each stage receives abstract @ref ExternalReader / @ref ExternalWriter
	 * interfaces, allowing the Pipeline to choose the concrete buffer
	 * implementation for every intermediate step without changing stage code.
	 *
	 * @par Buffer strategy
	 * - **Intermediate stages** use the private high-performance
	 *   @ref LockFreeRing (SPSC lock-free circular buffer).
	 * - **Final stage** writes into a public @ref Producer (backed by @ref Ring),
	 *   so the @ref Consumer returned to the caller keeps the full public API
	 *   and can be shared safely.
	 *
	 * @par Execution modes (@ref ExecutionMode bitmask)
	 * Flags are orthogonal and combinable with @c operator|:
	 * - @c Sync (0): stages sequential on the caller’s thread; @ref Process blocks.
	 * - @c Async: work runs in background; @ref Process returns immediately.
	 * - @c Parallel: one thread per stage (SPSC intermediates); without @c Async,
	 *   @ref Process still joins workers before returning.
	 * - @c Async | Parallel: concurrent stages and non-blocking @ref Process.
	 *
	 * @par Stage signature
	 * @code{.cpp}
	 * void stage(ExternalReader& in, ExternalWriter& out,
	 *            std::shared_ptr<Logger::Log> log);
	 * @endcode
	 *
	 * When @p log is not null, @ref Process passes
	 * @c log->Scope("Buffer/Pipeline") so @c %c identifies this module
	 * without touching the thread-local component stack. Nested
	 * @c log->Scope("Decode") inside a stage becomes
	 * @c Buffer/Pipeline/Decode (or @c Multimedia/Buffer/Pipeline/Decode
	 * if the caller already scoped the parent module).
	 *
	 * @par Best practices
	 * - Always call @c out.Close() (or @c out.SetError()) at the end of every stage.
	 * - Prefer @c Async | Parallel for multi-stage streaming production workloads.
	 * - Use @c Sync for deterministic debugging.
	 * - The returned @ref Consumer is the only synchronization point the caller needs
	 *   (wait on @ref Consumer::EoF() / @ref Consumer::IsWritable() as appropriate).
	 * - Pass the application or parent-module logger to @ref Process; do not
	 *   pre-scope @c Buffer/Pipeline or the path will be duplicated.
	 *
	 * @see ExternalReader, ExternalWriter, Producer, Consumer, LockFreeRing, ExecutionMode
	 */
	class STORMBYTE_BUFFER_PUBLIC Pipeline final {
		public:
			/**
			 * @brief Signature of a pipeline stage.
			 *
			 * Stages receive abstract reader/writer interfaces so the Pipeline
			 * can inject @ref LockFreeRing for intermediates and @ref Ring for
			 * the final output without changing stage code.
			 *
			 * @param in Abstract reader for the stage input.
			 * @param out Abstract writer for the stage output.
			 * @param log Optional logger (may be null). When set, this is
			 *            already @c Scope("Buffer/Pipeline") relative to the
			 *            logger passed to @ref Process.
			 */
			using PipeFunction = std::function<void(
				ExternalReader&,
				ExternalWriter&,
				std::shared_ptr<Logger::Log>
			)>;

			/**
			 * @name Constructors / destructor / assignment
			 * @{
			 */

			/**
			 * @brief Default construct an empty pipeline (no stages).
			 */
			Pipeline() noexcept;

			/**
			 * @brief Copy construct.
			 * @param other Source pipeline (only the list of stages is copied;
			 *              no running background work is shared).
			 */
			Pipeline(const Pipeline& other);

			/**
			 * @brief Move construct.
			 * @param other Source pipeline (left in a valid but unspecified state).
			 */
			Pipeline(Pipeline&& other) noexcept;

			/**
			 * @brief Destructor.
			 * @details Joins any running background execution before destroying state.
			 */
			~Pipeline() noexcept;

			/**
			 * @brief Copy assignment.
			 * @param other Source pipeline (stages only).
			 * @return Reference to this pipeline.
			 */
			Pipeline& operator=(const Pipeline& other);

			/**
			 * @brief Move assignment.
			 * @param other Source pipeline.
			 * @return Reference to this pipeline.
			 */
			Pipeline& operator=(Pipeline&& other) noexcept;

			/** @} */

			/**
			 * @name Stage registration
			 * @{
			 */

			/**
			 * @brief Append a processing stage (copy).
			 * @param pipe Stage function matching @ref PipeFunction.
			 */
			void AddPipe(const PipeFunction& pipe);

			/**
			 * @brief Append a processing stage (move).
			 * @param pipe Stage function matching @ref PipeFunction.
			 */
			void AddPipe(PipeFunction&& pipe);

			/** @} */

			/**
			 * @name Execution / error
			 * @{
			 */

			/**
			 * @brief Propagate error state to all internal buffers.
			 * @details Calls @c SetError() on every intermediate @ref LockFreeRing
			 *          and on the final @ref Producer. Waiting stages wake and
			 *          observe the error condition.
			 */
			void SetError() const noexcept;

			/**
			 * @brief Execute the pipeline.
			 * @param buffer Input @ref Consumer for the first stage.
			 * @param mode Bitmask of @ref ExecutionMode flags
			 *              (@c Sync, @c Async, @c Parallel, or combinations).
			 * @param log Optional logger (may be null). When set, every stage
			 *            receives @c log->Scope("Buffer/Pipeline").
			 * @return @ref Consumer of the final stage.
			 *         When @c Async is set, the Consumer is available immediately
			 *         while background work continues; otherwise @ref Process
			 *         returns only after all stages have finished.
			 *
			 * @note Any previous background run is joined before starting a new one.
			 * @see ExecutionMode, HasExecutionFlag(), Consumer, Producer
			 */
			Consumer Process(Consumer buffer,
							const ExecutionMode& mode,
							std::shared_ptr<Logger::Log> log) const noexcept;

			/** @} */

		private:
			struct Impl;					///< Private implementation (PIMPL)
			std::unique_ptr<Impl> m_impl;	///< Opaque pointer to implementation
	};
}
