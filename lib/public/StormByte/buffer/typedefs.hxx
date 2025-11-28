#pragma once

#include <StormByte/buffer/exception.hxx>
#include <StormByte/expected.hxx>

#include <cstddef>
#include <functional>
#include <vector>

/**
 * @namespace Buffer
 * @brief Namespace for buffer-related components in the StormByte library.
 *
 * The Buffer namespace provides classes and utilities for byte buffers,
 * including FIFO buffers, thread-safe shared buffers, producer-consumer
 * interfaces, and multi-stage processing pipelines.
 */
namespace StormByte::Buffer {
	/** @brief Forward declaration of Consumer class. */
	class Consumer;
	
	/** @brief Forward declaration of Producer class. */
	class Producer;

	/**
	 * @brief Type alias for Expected containing byte vector data.
	 * @tparam Exception The exception type to use for error cases.
	 * 
	 * @details This type represents the result of buffer read/extract operations.
	 *          It returns either a vector of bytes on success, or an exception
	 *          wrapped in std::unexpected on failure (e.g., InsufficientData).
	 * 
	 * @see Expected, InsufficientData
	 */
	template<class Exception>
	using ExpectedData = Expected<std::vector<std::byte>, Exception>;

	/**
	 * @brief Type alias for pipeline transformation functions.
	 * 
	 * @details Function signature for pipeline stage transformations that read
	 *          from a Consumer and write to a Producer, enabling data processing
	 *          in multi-stage pipelines.
	 * 
	 * @see Consumer, Producer, Pipeline
	 */
	using PipeFunction = std::function<void(Consumer, Producer)>;
}