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

#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/buffer/external.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
using StormByte::Buffer::Consumer;
using StormByte::Buffer::DataType;
using StormByte::Buffer::ExecutionMode;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::Pipeline;
using StormByte::Buffer::Producer;
// Configure the size of large data test (in kilobytes)
#define LARGE_TEST_SIZE_KB 1024
// Toggle between Read (non-destructive) and Extract (destructive)
// Comment out to use Extract instead of Read
#define USE_READ
#ifdef USE_READ
	#define CONSUME(reader, count, buff) (reader).Read(count, buff)
#else
	#define CONSUME(reader, count, buff) (reader).Extract(count, buff)
#endif
// Non-blocking multi-stage production default for long pipelines
static constexpr ExecutionMode kAsyncParallel =
	ExecutionMode::Async | ExecutionMode::Parallel;
// Use an in-memory stream for tests to avoid slow console I/O.
static std::ostringstream logging_stream;
std::shared_ptr<StormByte::Logger::Log> logging =
	std::make_shared<StormByte::Logger::Log>(logging_stream, StormByte::Logger::Level::Info);
// Helper to wait for pipeline completion without arbitrary sleeps
void wait_for_pipeline_completion(Consumer& consumer) {
	while (consumer.IsWritable()) {
		std::this_thread::yield();
	}
}
// ---------------------------------------------------------------------------
// Basic correctness
// ---------------------------------------------------------------------------
int test_pipeline_empty() {
	Pipeline pipeline;
	Producer input;
	(void)input.Write("TEST");
	input.Close();
	// Empty pipeline should just pass through
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("empty pipeline has data", res);
	ASSERT_EQUAL("empty pipeline content",
				StormByte::String::FromByteVector(data),
				std::string("TEST"));
	RETURN_TEST("test_pipeline_empty", 0);
}
int test_pipeline_single_stage() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				for (auto& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("hello world");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("single stage has data", res);
	ASSERT_EQUAL("single stage uppercase",
				StormByte::String::FromByteVector(data),
				std::string("HELLO WORLD"));
	RETURN_TEST("test_pipeline_single_stage", 0);
}
int test_pipeline_two_stages() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				for (auto& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				std::replace(str.begin(), str.end(), ' ', '_');
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("hello world test");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("two stages has data", res);
	ASSERT_EQUAL("two stages transformation",
				StormByte::String::FromByteVector(data),
				std::string("HELLO_WORLD_TEST"));
	RETURN_TEST("test_pipeline_two_stages", 0);
}
int test_pipeline_three_stages() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				for (auto& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				std::replace(str.begin(), str.end(), ' ', '-');
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		(void)out.Write("[");
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				(void)out.Write(StormByte::String::FromByteVector(data));
			}
		}
		(void)out.Write("]");
		out.Close();
	});
	Producer input;
	(void)input.Write("test data");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("three stages has data", res);
	ASSERT_EQUAL("three stages transformation",
				StormByte::String::FromByteVector(data),
				std::string("[TEST-DATA]"));
	RETURN_TEST("test_pipeline_three_stages", 0);
}
int test_pipeline_incremental_processing() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 1, data);
			if (res && !data.empty()) {
				char c = static_cast<char>(data[0]);
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(std::string(1, c));
			}
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("abc");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("incremental has data", res);
	ASSERT_EQUAL("incremental processing",
				StormByte::String::FromByteVector(data),
				std::string("ABC"));
	RETURN_TEST("test_pipeline_incremental_processing", 0);
}
int test_pipeline_filter_stage() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				std::string filtered;
				for (char c : str) {
					if (std::isalpha(static_cast<unsigned char>(c)))
						filtered += c;
				}
				if (!filtered.empty())
					(void)out.Write(filtered);
			}
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("Hello123World456!");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("filter has data", res);
	ASSERT_EQUAL("filter stage",
				StormByte::String::FromByteVector(data),
				std::string("HelloWorld"));
	RETURN_TEST("test_pipeline_filter_stage", 0);
}
int test_pipeline_multiple_writes() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				(void)out.Write(data);
				(void)out.Write(data);
			}
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("AB");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("multiple writes has data", res);
	ASSERT_EQUAL("multiple writes",
				StormByte::String::FromByteVector(data),
				std::string("ABAB"));
	RETURN_TEST("test_pipeline_multiple_writes", 0);
}
int test_pipeline_empty_input() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty())
				(void)out.Write(data);
		}
		out.Close();
	});
	Producer input;
	input.Close(); // Close without writing
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	if (!res) {
		ASSERT_TRUE("empty pipeline reached EOF", result.EoF());
	} else {
		ASSERT_EQUAL("empty input size", data.size(), static_cast<std::size_t>(0));
	}
	RETURN_TEST("test_pipeline_empty_input", 0);
}
int test_pipeline_large_data() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		std::size_t count = 0;
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty())
				count += data.size();
		}
		(void)out.Write(std::to_string(count));
		out.Close();
	});
	Producer input;
	std::string large_data(10000, 'A');
	(void)input.Write(large_data);
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("large data has result", res);
	ASSERT_EQUAL("large data count",
				StormByte::String::FromByteVector(data),
				std::string("10000"));
	RETURN_TEST("test_pipeline_large_data", 0);
}
int test_pipeline_reuse() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		(void)out.Write(">");
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty())
				(void)out.Write(data);
		}
		out.Close();
	});
	{
		Producer input1;
		(void)input1.Write("TEST1");
		input1.Close();
		Consumer result1 = pipeline.Process(input1.Consumer(), ExecutionMode::Async, logging);
		wait_for_pipeline_completion(result1);
		DataType data1;
		auto res1 = CONSUME(result1, 0, data1);
		ASSERT_TRUE("reuse first has data", res1);
		ASSERT_EQUAL("reuse first result",
					StormByte::String::FromByteVector(data1),
					std::string(">TEST1"));
	}
	{
		Producer input2;
		(void)input2.Write("TEST2");
		input2.Close();
		Consumer result2 = pipeline.Process(input2.Consumer(), ExecutionMode::Async, logging);
		wait_for_pipeline_completion(result2);
		DataType data2;
		auto res2 = CONSUME(result2, 0, data2);
		ASSERT_TRUE("reuse second has data", res2);
		ASSERT_EQUAL("reuse second result",
					StormByte::String::FromByteVector(data2),
					std::string(">TEST2"));
	}
	RETURN_TEST("test_pipeline_reuse", 0);
}
int test_pipeline_copy_constructor() {
	Pipeline pipeline1;
	pipeline1.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				for (auto& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	Pipeline pipeline2 = pipeline1;
	Producer input;
	(void)input.Write("test");
	input.Close();
	Consumer result = pipeline2.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("copy constructor has data", res);
	ASSERT_EQUAL("copy constructor works",
				StormByte::String::FromByteVector(data),
				std::string("TEST"));
	RETURN_TEST("test_pipeline_copy_constructor", 0);
}
int test_pipeline_move_constructor() {
	Pipeline pipeline1;
	pipeline1.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				for (auto& c : str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	Pipeline pipeline2 = std::move(pipeline1);
	Producer input;
	(void)input.Write("TEST");
	input.Close();
	Consumer result = pipeline2.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("move constructor has data", res);
	ASSERT_EQUAL("move constructor works",
				StormByte::String::FromByteVector(data),
				std::string("test"));
	RETURN_TEST("test_pipeline_move_constructor", 0);
}
int test_pipeline_addpipe_move() {
	Pipeline pipeline;
	Pipeline::PipeFunction func =
		[](ExternalReader& in, ExternalWriter& out,
		std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				DataType data;
				auto res = CONSUME(in, 0, data);
				if (res && !data.empty())
					(void)out.Write(data);
			}
			out.Close();
		};
	pipeline.AddPipe(std::move(func));
	Producer input;
	(void)input.Write("MOVE");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("addpipe move has data", res);
	ASSERT_EQUAL("addpipe move works",
				StormByte::String::FromByteVector(data),
				std::string("MOVE"));
	RETURN_TEST("test_pipeline_addpipe_move", 0);
}
int test_pipeline_word_count() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		std::size_t word_count = 0;
		std::string buffer;
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty())
				buffer += StormByte::String::FromByteVector(data);
		}
		bool in_word = false;
		for (char c : buffer) {
			if (std::isspace(static_cast<unsigned char>(c))) {
				in_word = false;
			} else if (!in_word) {
				in_word = true;
				++word_count;
			}
		}
		(void)out.Write(std::to_string(word_count));
		out.Close();
	});
	Producer input;
	(void)input.Write("Hello world this is a test");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("word count has data", res);
	ASSERT_EQUAL("word count result",
				StormByte::String::FromByteVector(data),
				std::string("6"));
	RETURN_TEST("test_pipeline_word_count", 0);
}
int test_pipeline_reverse_string() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		std::string buffer;
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty())
				buffer += StormByte::String::FromByteVector(data);
		}
		std::reverse(buffer.begin(), buffer.end());
		(void)out.Write(buffer);
		out.Close();
	});
	Producer input;
	(void)input.Write("ABCDEF");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("reverse has data", res);
	ASSERT_EQUAL("reverse result",
				StormByte::String::FromByteVector(data),
				std::string("FEDCBA"));
	RETURN_TEST("test_pipeline_reverse_string", 0);
}
int test_pipeline_streaming_data() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty())
				(void)out.Write(data);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		out.Close();
	});
	Producer input;
	std::thread writer([&input]() {
		(void)input.Write("Part1");
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		(void)input.Write("Part2");
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		(void)input.Write("Part3");
		input.Close();
	});
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	writer.join();
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("streaming has data", res);
	ASSERT_EQUAL("streaming result",
				StormByte::String::FromByteVector(data),
				std::string("Part1Part2Part3"));
	RETURN_TEST("test_pipeline_streaming_data", 0);
}
int test_pipeline_byte_arithmetic() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				DataType result;
				result.reserve(data.size());
				for (const auto& byte : data)
					result.push_back(static_cast<std::byte>(static_cast<int>(byte) + 1));
				(void)out.Write(result);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				DataType result;
				result.reserve(data.size());
				for (const auto& byte : data)
					result.push_back(static_cast<std::byte>(static_cast<int>(byte) * 2));
				(void)out.Write(result);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				DataType result;
				result.reserve(data.size());
				for (const auto& byte : data)
					result.push_back(static_cast<std::byte>(static_cast<int>(byte) / 2));
				(void)out.Write(result);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				DataType result;
				result.reserve(data.size());
				for (const auto& byte : data)
					result.push_back(static_cast<std::byte>(static_cast<int>(byte) - 1));
				(void)out.Write(result);
			}
		}
		out.Close();
	});
	std::vector<std::byte> input_data = {
		std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}
	};
	Producer input;
	(void)input.Write(input_data);
	input.Close();
	// 4 stages: Parallel can overlap without changing the transform chain
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("byte arithmetic has data", res);
	ASSERT_EQUAL("byte arithmetic size", data.size(), static_cast<std::size_t>(5));
	ASSERT_EQUAL("byte 0", static_cast<int>(data[0]), 1);
	ASSERT_EQUAL("byte 1", static_cast<int>(data[1]), 2);
	ASSERT_EQUAL("byte 2", static_cast<int>(data[2]), 3);
	ASSERT_EQUAL("byte 3", static_cast<int>(data[3]), 4);
	ASSERT_EQUAL("byte 4", static_cast<int>(data[4]), 5);
	RETURN_TEST("test_pipeline_byte_arithmetic", 0);
}
int test_pipeline_large_concurrent_stress() {
	Pipeline pipeline;
	auto xor55 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data) r.push_back(b ^ std::byte{0x55});
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto add17 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data)
					r.push_back(static_cast<std::byte>(static_cast<uint8_t>(b) + 17));
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto bnot = [](ExternalReader& in, ExternalWriter& out,
				std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data) r.push_back(~b);
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto xorAA = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data) r.push_back(b ^ std::byte{0xAA});
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto mul3 = [](ExternalReader& in, ExternalWriter& out,
				std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data)
					r.push_back(static_cast<std::byte>(static_cast<uint8_t>(b) * 3));
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto rotl3 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data) {
					uint8_t v = static_cast<uint8_t>(b);
					r.push_back(static_cast<std::byte>((v << 3) | (v >> 5)));
				}
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto sub42 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data)
					r.push_back(static_cast<std::byte>(static_cast<uint8_t>(b) - 42));
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto xor33 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data) r.push_back(b ^ std::byte{0x33});
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto mul171 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data)
					r.push_back(static_cast<std::byte>(static_cast<uint8_t>(b) * 171));
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto rotr3 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data) {
					uint8_t v = static_cast<uint8_t>(b);
					r.push_back(static_cast<std::byte>((v >> 3) | (v << 5)));
				}
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto add42 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data)
					r.push_back(static_cast<std::byte>(static_cast<uint8_t>(b) + 42));
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	auto sub17 = [](ExternalReader& in, ExternalWriter& out,
					std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, 0, data) && !data.empty()) {
				DataType r; r.reserve(data.size());
				for (auto b : data)
					r.push_back(static_cast<std::byte>(static_cast<uint8_t>(b) - 17));
				(void)out.Write(std::move(r));
			}
		}
		out.Close();
	};
	// 8 transforms + 8 inverses — ideal for Parallel pipeline overlap
	pipeline.AddPipe(xor55);
	pipeline.AddPipe(add17);
	pipeline.AddPipe(bnot);
	pipeline.AddPipe(xorAA);
	pipeline.AddPipe(mul3);
	pipeline.AddPipe(rotl3);
	pipeline.AddPipe(sub42);
	pipeline.AddPipe(xor33);
	pipeline.AddPipe(xor33);   // undo
	pipeline.AddPipe(add42);   // undo sub42
	pipeline.AddPipe(rotr3);   // undo rotl3
	pipeline.AddPipe(mul171);  // undo mul3
	pipeline.AddPipe(xorAA);   // undo
	pipeline.AddPipe(bnot);    // undo
	pipeline.AddPipe(sub17);   // undo add17
	pipeline.AddPipe(xor55);   // undo
	const std::size_t data_size = LARGE_TEST_SIZE_KB * 1024;
	std::vector<std::byte> input_data;
	input_data.reserve(data_size);
	for (std::size_t i = 0; i < data_size; ++i)
		input_data.push_back(static_cast<std::byte>((i * 31 + 17) % 256));
	Producer input;
	std::thread writer([&input, &input_data]() {
		const std::size_t chunk_size = 4096;
		std::size_t offset = 0;
		while (offset < input_data.size()) {
			std::size_t to_write = std::min(chunk_size, input_data.size() - offset);
			std::vector<std::byte> chunk(input_data.begin() + static_cast<std::ptrdiff_t>(offset),
										input_data.begin() + static_cast<std::ptrdiff_t>(offset + to_write));
			(void)input.Write(std::move(chunk));
			offset += to_write;
			std::this_thread::yield();
		}
		input.Close();
	});
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	writer.join();
	wait_for_pipeline_completion(result);
	std::vector<std::byte> output_data;
	output_data.reserve(data_size);
	while (result.AvailableBytes() > 0) {
		DataType chunk;
		if (CONSUME(result, 2048, chunk) && !chunk.empty())
			output_data.insert(output_data.end(), chunk.begin(), chunk.end());
		std::this_thread::yield();
	}
	ASSERT_EQUAL("large stress test size", output_data.size(), data_size);
	bool data_matches = true;
	std::size_t first_mismatch = 0;
	for (std::size_t i = 0; i < data_size; ++i) {
		if (input_data[i] != output_data[i]) {
			data_matches = false;
			first_mismatch = i;
			break;
		}
	}
	if (!data_matches) {
		std::cout << "Data mismatch at byte " << first_mismatch
				<< ": expected " << static_cast<int>(input_data[first_mismatch])
				<< ", got " << static_cast<int>(output_data[first_mismatch]) << std::endl;
	}
	ASSERT_TRUE("large stress test data integrity", data_matches);
	RETURN_TEST("test_pipeline_large_concurrent_stress", 0);
}
int test_pipeline_sync_execution() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				for (auto& c : str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			auto res = CONSUME(in, 0, data);
			if (res && !data.empty()) {
				std::string str = StormByte::String::FromByteVector(data);
				std::replace(str.begin(), str.end(), ' ', '-');
				(void)out.Write(str);
			}
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("sync mode test");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Sync, logging);
	ASSERT_FALSE("sync result writable", result.IsWritable());
	DataType data;
	auto res = CONSUME(result, 0, data);
	ASSERT_TRUE("sync has data", res);
	ASSERT_EQUAL("sync transformation",
				StormByte::String::FromByteVector(data),
				std::string("SYNC-MODE-TEST"));
	RETURN_TEST("test_pipeline_sync_execution", 0);
}
int test_pipeline_interrupted_by_seterror() {
	Pipeline pipeline;
	for (int i = 0; i < 8; ++i) {
		pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
							std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				DataType data;
				auto res = CONSUME(in, 0, data);
				if (res && !data.empty()) {
					for (int k = 0; k < 200; ++k) {
						if (!out.IsWritable()) return;
						std::this_thread::yield();
					}
					if (!out.IsWritable()) return;
					(void)out.Write(data);
				}
			}
			if (out.IsWritable()) out.Close();
		});
	}
	Producer input;
	std::string payload(50000, 'X');
	(void)input.Write(payload);
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	pipeline.SetError();
	wait_for_pipeline_completion(result);
	ASSERT_FALSE("interrupted not writable", result.IsWritable());
	ASSERT_TRUE("interrupted eof", result.EoF());
	ASSERT_EQUAL("interrupted size zero", result.AvailableBytes(), static_cast<std::size_t>(0));
	RETURN_TEST("test_pipeline_interrupted_by_seterror", 0);
}
int test_pipeline_large_async_many_stages() {
	Pipeline pipeline;
	for (int i = 0; i < 25; ++i) {
		pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
							std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				DataType data;
				if (CONSUME(in, 0, data) && !data.empty()) {
					std::string s = StormByte::String::FromByteVector(data);
					for (char& c : s)
						c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
					(void)out.Write(s);
				}
			}
			out.Close();
		});
	}
	Producer input;
	const std::string payload(8192, 'a');
	(void)input.Write(payload);
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	ASSERT_TRUE("large async has data", CONSUME(result, 0, data));
	ASSERT_EQUAL("content",
				StormByte::String::FromByteVector(data),
				std::string(8192, 'A'));
	RETURN_TEST("test_pipeline_large_async_many_stages", 0);
}
int test_pipeline_async_reuse_many_times() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType d;
			if (CONSUME(in, 0, d) && !d.empty())
				(void)out.Write(d);
		}
		out.Close();
	});
	for (int i = 0; i < 50; ++i) {
		Producer input;
		std::string msg = "RUN-" + std::to_string(i);
		(void)input.Write(msg);
		input.Close();
		Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
		wait_for_pipeline_completion(result);
		DataType data;
		ASSERT_TRUE("reuse has data", CONSUME(result, 0, data));
		ASSERT_EQUAL("reuse content", StormByte::String::FromByteVector(data), msg);
	}
	RETURN_TEST("test_pipeline_async_reuse_many_times", 0);
}
int test_pipeline_async_seterror_interrupts_quickly() {
	Pipeline pipeline;
	for (int i = 0; i < 12; ++i) {
		pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
							std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				if (!out.IsWritable()) return;
				DataType d;
				if (CONSUME(in, 0, d) && !d.empty()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
					if (!out.IsWritable()) return;
					(void)out.Write(d);
				}
			}
			if (out.IsWritable()) out.Close();
		});
	}
	Producer input;
	(void)input.Write(std::string(100000, 'X'));
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	pipeline.SetError();
	wait_for_pipeline_completion(result);
	ASSERT_FALSE("interrupted not writable", result.IsWritable());
	ASSERT_TRUE("interrupted reaches EoF", result.EoF());
	RETURN_TEST("test_pipeline_async_seterror_interrupts_quickly", 0);
}
// ---------------------------------------------------------------------------
// Additional coverage
// ---------------------------------------------------------------------------
int test_pipeline_stage_must_close() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		DataType d;
		while (!in.EoF()) {
			if (CONSUME(in, 0, d) && !d.empty())
				(void)out.Write(d);
		}
		out.Close(); // required
	});
	Producer input;
	(void)input.Write("close-me");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Sync, logging);
	ASSERT_TRUE("closed after sync", result.EoF() || !result.IsWritable());
	DataType data;
	ASSERT_TRUE("has data", CONSUME(result, 0, data));
	ASSERT_EQUAL("content", StormByte::String::FromByteVector(data), std::string("close-me"));
	RETURN_TEST("test_pipeline_stage_must_close", 0);
}
int test_pipeline_identity_many_stages() {
	Pipeline pipeline;
	for (int i = 0; i < 10; ++i) {
		pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
							std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				DataType d;
				if (CONSUME(in, 0, d) && !d.empty())
					(void)out.Write(d);
			}
			out.Close();
		});
	}
	Producer input;
	const std::string msg = "identity-chain";
	(void)input.Write(msg);
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	ASSERT_TRUE("identity has data", CONSUME(result, 0, data));
	ASSERT_EQUAL("identity content", StormByte::String::FromByteVector(data), msg);
	RETURN_TEST("test_pipeline_identity_many_stages", 0);
}
int test_pipeline_null_logger() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log> /*log*/) {
		while (!in.EoF()) {
			DataType d;
			if (CONSUME(in, 0, d) && !d.empty())
				(void)out.Write(d);
		}
		out.Close();
	});
	Producer input;
	(void)input.Write("null-log");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Sync, nullptr);
	DataType data;
	ASSERT_TRUE("null log has data", CONSUME(result, 0, data));
	ASSERT_EQUAL("null log content",
				StormByte::String::FromByteVector(data),
				std::string("null-log"));
	RETURN_TEST("test_pipeline_null_logger", 0);
}
int test_pipeline_available_bytes_during_process() {
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType d;
			if (CONSUME(in, 0, d) && !d.empty())
				(void)out.Write(d);
		}
		out.Close();
	});
	Producer input;
	(void)input.Write(std::string(1000, 'Z'));
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Async, logging);
	wait_for_pipeline_completion(result);
	ASSERT_TRUE("has available or eof",
				result.AvailableBytes() > 0 || result.EoF());
	DataType data;
	ASSERT_TRUE("drain ok", CONSUME(result, 0, data));
	ASSERT_EQUAL("size", data.size(), static_cast<std::size_t>(1000));
	RETURN_TEST("test_pipeline_available_bytes_during_process", 0);
}
int test_pipeline_parallel_blocking() {
	// Parallel without Async: Process must block until all stages finish.
	Pipeline pipeline;
	for (int i = 0; i < 4; ++i) {
		pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
							std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				DataType d;
				if (CONSUME(in, 0, d) && !d.empty()) {
					for (auto& b : d)
						b = static_cast<std::byte>(static_cast<std::uint8_t>(b) + 1);
					(void)out.Write(std::move(d));
				}
			}
			out.Close();
		});
	}
	Producer input;
	DataType payload;
	for (int i = 0; i < 32; ++i)
		payload.push_back(static_cast<std::byte>(i));
	(void)input.Write(payload);
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), ExecutionMode::Parallel, logging);
	// Blocking Parallel ⇒ already finished when Process returns
	ASSERT_FALSE("parallel blocking not writable", result.IsWritable());
	DataType data;
	ASSERT_TRUE("parallel blocking has data", CONSUME(result, 0, data));
	ASSERT_EQUAL("parallel blocking size", data.size(), static_cast<std::size_t>(32));
	for (int i = 0; i < 32; ++i)
		ASSERT_EQUAL("parallel blocking value", static_cast<int>(data[static_cast<std::size_t>(i)]), i + 4);
	RETURN_TEST("test_pipeline_parallel_blocking", 0);
}
int test_pipeline_parallel_async_correctness() {
	// Same transform under Async|Parallel (non-blocking Process).
	Pipeline pipeline;
	for (int i = 0; i < 6; ++i) {
		pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
							std::shared_ptr<StormByte::Logger::Log>) {
			while (!in.EoF()) {
				DataType d;
				if (CONSUME(in, 0, d) && !d.empty()) {
					std::string s = StormByte::String::FromByteVector(d);
					for (char& c : s)
						c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
					(void)out.Write(s);
				}
			}
			out.Close();
		});
	}
	Producer input;
	(void)input.Write("parallel-async-ok");
	input.Close();
	Consumer result = pipeline.Process(input.Consumer(), kAsyncParallel, logging);
	wait_for_pipeline_completion(result);
	DataType data;
	ASSERT_TRUE("parallel async has data", CONSUME(result, 0, data));
	ASSERT_EQUAL("parallel async content",
				StormByte::String::FromByteVector(data),
				std::string("PARALLEL-ASYNC-OK"));
	RETURN_TEST("test_pipeline_parallel_async_correctness", 0);
}
int test_pipeline_sync_vs_parallel_cpu_bound() {
	// Workload deliberately exaggerated so pipeline parallelism
	// has a clear, repeatable advantage over pure Sync.
	//
	// - more stages
	// - larger stream
	// - heavier per-byte ALU work
	// - small CONSUME chunks to allow real overlapping
	constexpr int         kStages    = 12;                 // was 8
	constexpr std::size_t kSize      = 12 * 1024 * 1024;   // was 4 MiB → 12 MiB
	constexpr std::size_t kChunk     = 2048;               // smaller chunks → more overlap
	constexpr int         kInnerWork = 48;                 // was 24 → heavier CPU work
	auto cpu_stage = [](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<StormByte::Logger::Log>) {
		while (!in.EoF()) {
			DataType data;
			if (CONSUME(in, kChunk, data) && !data.empty()) {
				for (auto& b : data) {
					std::uint8_t v = static_cast<std::uint8_t>(b);
					for (int w = 0; w < kInnerWork; ++w) {
						v = static_cast<std::uint8_t>(v * 131u + 17u);
						v ^= static_cast<std::uint8_t>(w * 3);
						// a bit more work to make it even more CPU-bound
						v = static_cast<std::uint8_t>((v << 1) | (v >> 7));
					}
					b = static_cast<std::byte>(v);
				}
				(void)out.Write(std::move(data));
			}
		}
		out.Close();
	};
	auto build_pipeline = [&]() {
		Pipeline pipeline;
		for (int i = 0; i < kStages; ++i)
			pipeline.AddPipe(cpu_stage);
		return pipeline;
	};
	DataType input_data(kSize);
	for (std::size_t i = 0; i < kSize; ++i)
		input_data[i] = static_cast<std::byte>((i * 31u + 17u) & 0xFFu);
	// Oracle (single-threaded sequential transform)
	DataType expected = input_data;
	for (int s = 0; s < kStages; ++s) {
		for (auto& b : expected) {
			std::uint8_t v = static_cast<std::uint8_t>(b);
			for (int w = 0; w < kInnerWork; ++w) {
				v = static_cast<std::uint8_t>(v * 131u + 17u);
				v ^= static_cast<std::uint8_t>(w * 3);
				v = static_cast<std::uint8_t>((v << 1) | (v >> 7));
			}
			b = static_cast<std::byte>(v);
		}
	}
	auto run_mode = [&](ExecutionMode mode, const char* label,
						long long& total_ms) -> int {
		Pipeline pipe = build_pipeline();
		Producer input;
		(void)input.Write(input_data);
		input.Close();
		const auto t0 = std::chrono::steady_clock::now();
		Consumer result = pipe.Process(input.Consumer(), mode, logging);
		const auto t1 = std::chrono::steady_clock::now();
		total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		ASSERT_FALSE(label, result.IsWritable());
		DataType out;
		ASSERT_TRUE(label, CONSUME(result, 0, out));
		ASSERT_EQUAL("size", out.size(), kSize);
		ASSERT_TRUE("content matches oracle", out == expected);
		return 0;
	};
	long long sync_ms = 0;
	long long parallel_ms = 0;
	// Optional warm-up (helps reduce first-run noise)
	{
		long long dummy = 0;
		(void)run_mode(ExecutionMode::Sync, "warmup-sync", dummy);
		(void)run_mode(ExecutionMode::Parallel, "warmup-parallel", dummy);
	}
	if (run_mode(ExecutionMode::Sync, "sync cpu-bound", sync_ms) != 0)
		return 1;
	if (run_mode(ExecutionMode::Parallel, "parallel cpu-bound", parallel_ms) != 0)
		return 1;
	// Uncomment for debugging when it fails on CI
	// std::cout << "Sync     total: " << sync_ms << " ms\n"
	//           << "Parallel total: " << parallel_ms << " ms\n";
	// Require a clear win, but allow a little noise.
	// On a healthy multi-core machine parallel should be noticeably faster.
	ASSERT_TRUE("parallel faster than sync (cpu-bound pipeline)",
				parallel_ms < sync_ms * 85 / 100);   // at least ~15% faster
	// Fallback more permissive if you still see flakes on very loaded CI:
	// ASSERT_TRUE("parallel not slower than sync", parallel_ms <= sync_ms);
	RETURN_TEST("test_pipeline_sync_vs_parallel_cpu_bound", 0);
}
int main() {
	int result = 0;
	result += test_pipeline_empty();
	result += test_pipeline_single_stage();
	result += test_pipeline_two_stages();
	result += test_pipeline_three_stages();
	result += test_pipeline_incremental_processing();
	result += test_pipeline_filter_stage();
	result += test_pipeline_multiple_writes();
	result += test_pipeline_empty_input();
	result += test_pipeline_large_data();
	result += test_pipeline_reuse();
	result += test_pipeline_copy_constructor();
	result += test_pipeline_move_constructor();
	result += test_pipeline_addpipe_move();
	result += test_pipeline_word_count();
	result += test_pipeline_reverse_string();
	result += test_pipeline_streaming_data();
	result += test_pipeline_byte_arithmetic();
	result += test_pipeline_large_concurrent_stress();
	result += test_pipeline_sync_execution();
	result += test_pipeline_interrupted_by_seterror();
	result += test_pipeline_large_async_many_stages();
	result += test_pipeline_async_reuse_many_times();
	result += test_pipeline_async_seterror_interrupts_quickly();
	result += test_pipeline_stage_must_close();
	result += test_pipeline_identity_many_stages();
	result += test_pipeline_null_logger();
	result += test_pipeline_available_bytes_during_process();
	result += test_pipeline_parallel_blocking();
	result += test_pipeline_parallel_async_correctness();
	result += test_pipeline_sync_vs_parallel_cpu_bound();
	if (result == 0) {
		std::cout << "Pipeline tests passed!" << std::endl;
	} else {
		std::cout << result << " Pipeline tests failed." << std::endl;
	}
	return result;
}
