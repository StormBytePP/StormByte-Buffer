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

#pragma once

#include <StormByte/buffer/exception.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/expected.hxx>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <vector>

/**
 * @namespace StormByte::Buffer
 * @brief Buffer module of the StormByte suite.
 */
namespace StormByte::Buffer {
	class Consumer;		///< Forward declaration of the Consumer class.
	class Producer;		///< Forward declaration of the Producer class.
	class ReadOnly;		///< Forward declaration of the ReadOnly interface.
	class WriteOnly;	///< Forward declaration of the WriteOnly interface.

	/**
	 * @enum Position
	 * @brief Positioning mode for buffer seek / non-destructive read operations.
	 *
	 * Defines how offset values are interpreted by @c Seek() and related APIs.
	 *
	 * @see ReadOnly::Seek()
	 */
	enum class STORMBYTE_BUFFER_PUBLIC Position {
		Absolute,	///< Offset from the beginning of the buffer (position 0).
		Relative	///< Offset from the current logical read position.
	};

	/**
	 * @brief Primary byte storage type used throughout the buffer API.
	 *
	 * Alias for @c std::vector&lt;std::byte&gt;. Serves as the fundamental
	 * container for byte-oriented buffer implementations
	 * (@ref FIFO, adapters, extract/read destinations, etc.).
	 */
	using DataType = std::vector<std::byte>;

	/**
	 * @enum ExecutionMode
	 * @brief Bitmask controlling how @ref Pipeline::Process schedules work.
	 *
	 * Flags are orthogonal and may be combined with @c operator|.
	 *
	 * - @c Sync (0) — no flags: stages run sequentially on the **caller’s** thread;
	 *   @ref Pipeline::Process blocks until completion.
	 * - @c Async — run work on background thread(s); @ref Pipeline::Process
	 *   returns immediately with the final @ref Consumer.
	 * - @c Parallel — one thread **per stage** (pipeline parallelism via SPSC
	 *   intermediate rings). Without @c Async, @ref Process still waits for all
	 *   stages to finish before returning.
	 *
	 * Typical combinations:
	 * | Expression                    | Stages              | Process()      |
	 * |-------------------------------|---------------------|----------------|
	 * | @c Sync (or @c 0)             | sequential, caller  | blocks         |
	 * | @c Async                      | sequential, 1 worker| returns now    |
	 * | @c Parallel                   | 1 thread per stage  | blocks         |
	 * | @c Async \| Parallel          | 1 thread per stage  | returns now    |
	 *
	 * @note Prefer @c Async | Parallel for multi-stage streaming production.
	 *       Use @c Sync for deterministic debugging.
	 *
	 * @see Pipeline::Process(), HasExecutionFlag()
	 */
	enum class STORMBYTE_BUFFER_PUBLIC ExecutionMode : unsigned {
		Sync     = 0,			///< Sequential on caller thread; Process blocks.
		Async    = 1u << 0,		///< Background execution; Process returns immediately.
		Parallel = 1u << 1		///< One thread per stage (pipeline parallelism).
	};

	/**
	 * @brief Bitwise OR of execution flags.
	 * @param a Left-hand flags.
	 * @param b Right-hand flags.
	 * @return Combined flag set.
	 */
	inline constexpr ExecutionMode operator|(ExecutionMode a, ExecutionMode b) noexcept {
		return static_cast<ExecutionMode>(
			static_cast<unsigned>(a) | static_cast<unsigned>(b));
	}

	/**
	 * @brief Bitwise AND of execution flags.
	 * @param a Left-hand flags.
	 * @param b Right-hand flags.
	 * @return Intersection of flag sets.
	 */
	inline constexpr ExecutionMode operator&(ExecutionMode a, ExecutionMode b) noexcept {
		return static_cast<ExecutionMode>(
			static_cast<unsigned>(a) & static_cast<unsigned>(b));
	}

	/**
	 * @brief Bitwise OR-assignment of execution flags.
	 * @param a Flags to update.
	 * @param b Flags to add.
	 * @return Reference to @p a.
	 */
	inline constexpr ExecutionMode& operator|=(ExecutionMode& a, ExecutionMode b) noexcept {
		a = a | b;
		return a;
	}

	/**
	 * @brief Whether @p mode includes all bits of @p flag.
	 * @param mode Combined mode value.
	 * @param flag Flag (or flag set) to test.
	 * @return @c true if every bit in @p flag is set in @p mode.
	 */
	inline constexpr bool HasExecutionFlag(ExecutionMode mode, ExecutionMode flag) noexcept {
		return (static_cast<unsigned>(mode) & static_cast<unsigned>(flag)) ==
			static_cast<unsigned>(flag);
	}
}
