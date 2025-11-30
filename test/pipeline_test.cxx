#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/string.hxx>
#include <StormByte/system.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>

using StormByte::Buffer::Pipeline;
using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

// Configure the size of large data test (in kilobytes)
#define LARGE_TEST_SIZE_KB 1024

// Toggle between Read (non-destructive) and Extract (destructive) for testing
// Comment out to use Extract instead of Read
#define USE_READ

#ifdef USE_READ
    #define CONSUME(consumer, count) (consumer).Read(count)
#else
    #define CONSUME(consumer, count) (consumer).Extract(count)
#endif

// Use an in-memory stream for tests to avoid slow console I/O and heavy
// header formatting during hot loops. Tests are verbose during development
// so pick a reasonable emission level.
static std::ostringstream logging_stream;
StormByte::Logger::Log logging = StormByte::Logger::Log(logging_stream, StormByte::Logger::Level::Info);

// Helper to wait for pipeline completion without arbitrary sleeps
void wait_for_pipeline_completion(Consumer& consumer) {
    while (consumer.IsWritable()) {
        StormByte::System::Yield();
    }
}

int test_pipeline_empty() {
    Pipeline pipeline;
    
    Producer input;
    input.Write("TEST");
    input.Close();
    
    // Empty pipeline should just pass through
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("empty pipeline has data", data.has_value());
    ASSERT_EQUAL("empty pipeline content", StormByte::String::FromByteVector(*data), std::string("TEST"));
    
    RETURN_TEST("test_pipeline_empty", 0);
}

int test_pipeline_single_stage() {
    Pipeline pipeline;
    
    // Single stage: uppercase transformation
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Write("hello world");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("single stage has data", data.has_value());
    ASSERT_EQUAL("single stage uppercase", StormByte::String::FromByteVector(*data), std::string("HELLO WORLD"));
    
    RETURN_TEST("test_pipeline_single_stage", 0);
}

int test_pipeline_two_stages() {
    Pipeline pipeline;
    
    // Stage 1: uppercase
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& logging) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Stage 2: replace spaces with underscores
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& logging) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                std::replace(str.begin(), str.end(), ' ', '_');
                out.Write(str);
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Write("hello world test");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("two stages has data", data.has_value());
    ASSERT_EQUAL("two stages transformation", StormByte::String::FromByteVector(*data), std::string("HELLO_WORLD_TEST"));
    
    RETURN_TEST("test_pipeline_two_stages", 0);
}

int test_pipeline_three_stages() {
    Pipeline pipeline;
    
    // Stage 1: uppercase
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Stage 2: replace spaces
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                std::replace(str.begin(), str.end(), ' ', '-');
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Stage 3: add prefix and suffix
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        out.Write("[");
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
        }
        out.Write("]");
        out.Close();
    });
    
    Producer input;
    input.Write("test data");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("three stages has data", data.has_value());
    ASSERT_EQUAL("three stages transformation", StormByte::String::FromByteVector(*data), std::string("[TEST-DATA]"));
    
    RETURN_TEST("test_pipeline_three_stages", 0);
}

int test_pipeline_incremental_processing() {
    Pipeline pipeline;
    
    // Stage that processes data incrementally
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 1); // Read one byte at a time
            if (data && !data->empty()) {
                char c = static_cast<char>((*data)[0]);
                c = std::toupper(c);
                out.Write(std::string(1, c));
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Write("abc");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("incremental has data", data.has_value());
    ASSERT_EQUAL("incremental processing", StormByte::String::FromByteVector(*data), std::string("ABC"));
    
    RETURN_TEST("test_pipeline_incremental_processing", 0);
}

int test_pipeline_filter_stage() {
    Pipeline pipeline;
    
    // Stage that filters out non-alphabetic characters
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                std::string filtered;
                for (char c : str) {
                    if (std::isalpha(c)) {
                        filtered += c;
                    }
                }
                if (!filtered.empty()) {
                    out.Write(filtered);
                }
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Write("Hello123World456!");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("filter has data", data.has_value());
    ASSERT_EQUAL("filter stage", StormByte::String::FromByteVector(*data), std::string("HelloWorld"));
    
    RETURN_TEST("test_pipeline_filter_stage", 0);
}

int test_pipeline_multiple_writes() {
    Pipeline pipeline;
    
    // Stage that duplicates each piece of data
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                out.Write(*data);
                out.Write(*data);
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Write("AB");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("multiple writes has data", data.has_value());
    ASSERT_EQUAL("multiple writes", StormByte::String::FromByteVector(*data), std::string("ABAB"));
    
    RETURN_TEST("test_pipeline_multiple_writes", 0);
}

int test_pipeline_empty_input() {
    Pipeline pipeline;
    
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Close(); // Close without writing
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("empty input has result", data.has_value());
    ASSERT_EQUAL("empty input size", data->size(), static_cast<std::size_t>(0));
    
    RETURN_TEST("test_pipeline_empty_input", 0);
}

int test_pipeline_large_data() {
    Pipeline pipeline;
    
    // Stage that counts characters
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        std::size_t count = 0;
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                count += data->size();
            }
        }
        std::string result = std::to_string(count);
        out.Write(result);
        out.Close();
    });
    
    Producer input;
    std::string large_data(10000, 'A');
    input.Write(large_data);
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("large data has result", data.has_value());
    ASSERT_EQUAL("large data count", StormByte::String::FromByteVector(*data), std::string("10000"));
    
    RETURN_TEST("test_pipeline_large_data", 0);
}

int test_pipeline_reuse() {
    Pipeline pipeline;
    
    // Stage that adds prefix
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        out.Write(">");
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
        }
        out.Close();
    });
    
    // First use
    {
        Producer input1;
        input1.Write("TEST1");
        input1.Close();
        
        Consumer result1 = pipeline.Process(input1.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
        wait_for_pipeline_completion(result1);
        
        auto data1 = result1.Read(0);
        ASSERT_TRUE("reuse first has data", data1.has_value());
        ASSERT_EQUAL("reuse first result", StormByte::String::FromByteVector(*data1), std::string(">TEST1"));
    }
    
    // Second use
    {
        Producer input2;
        input2.Write("TEST2");
        input2.Close();
        
        Consumer result2 = pipeline.Process(input2.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
        wait_for_pipeline_completion(result2);
        
        auto data2 = result2.Read(0);
        ASSERT_TRUE("reuse second has data", data2.has_value());
        ASSERT_EQUAL("reuse second result", StormByte::String::FromByteVector(*data2), std::string(">TEST2"));
    }
    
    RETURN_TEST("test_pipeline_reuse", 0);
}

int test_pipeline_copy_constructor() {
    Pipeline pipeline1;
    
    pipeline1.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Copy the pipeline
    Pipeline pipeline2 = pipeline1;
    
    Producer input;
    input.Write("test");
    input.Close();
    
    Consumer result = pipeline2.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("copy constructor has data", data.has_value());
    ASSERT_EQUAL("copy constructor works", StormByte::String::FromByteVector(*data), std::string("TEST"));
    
    RETURN_TEST("test_pipeline_copy_constructor", 0);
}

int test_pipeline_move_constructor() {
    Pipeline pipeline1;
    
    pipeline1.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::tolower(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Move the pipeline
    Pipeline pipeline2 = std::move(pipeline1);
    
    Producer input;
    input.Write("TEST");
    input.Close();
    
    Consumer result = pipeline2.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("move constructor has data", data.has_value());
    ASSERT_EQUAL("move constructor works", StormByte::String::FromByteVector(*data), std::string("test"));
    
    RETURN_TEST("test_pipeline_move_constructor", 0);
}

int test_pipeline_addpipe_move() {
    Pipeline pipeline;
    
    auto func = [](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
        }
        out.Close();
    };
    
    // Add using move
    pipeline.AddPipe(std::move(func));
    
    Producer input;
    input.Write("MOVE");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("addpipe move has data", data.has_value());
    ASSERT_EQUAL("addpipe move works", StormByte::String::FromByteVector(*data), std::string("MOVE"));
    
    RETURN_TEST("test_pipeline_addpipe_move", 0);
}

int test_pipeline_word_count() {
    Pipeline pipeline;
    
    // Count words (space-separated)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        std::size_t word_count = 0;
        std::string buffer;
        
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                buffer += StormByte::String::FromByteVector(*data);
            }
        }
        
        bool in_word = false;
        for (char c : buffer) {
            if (std::isspace(c)) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                word_count++;
            }
        }
        
        out.Write(std::to_string(word_count));
        out.Close();
    });
    
    Producer input;
    input.Write("Hello world this is a test");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("word count has data", data.has_value());
    ASSERT_EQUAL("word count result", StormByte::String::FromByteVector(*data), std::string("6"));
    
    RETURN_TEST("test_pipeline_word_count", 0);
}

int test_pipeline_reverse_string() {
    Pipeline pipeline;
    
    // Reverse the string
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        std::string buffer;
        
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                buffer += StormByte::String::FromByteVector(*data);
            }
        }
        
        std::reverse(buffer.begin(), buffer.end());
        out.Write(buffer);
        out.Close();
    });
    
    Producer input;
    input.Write("ABCDEF");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("reverse has data", data.has_value());
    ASSERT_EQUAL("reverse result", StormByte::String::FromByteVector(*data), std::string("FEDCBA"));
    
    RETURN_TEST("test_pipeline_reverse_string", 0);
}

int test_pipeline_streaming_data() {
    Pipeline pipeline;
    
    // Pass through with small delay to simulate processing
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        out.Close();
    });
    
    Producer input;
    
    // Write data in parts
    std::thread writer([&input]() {
        input.Write("Part1");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        input.Write("Part2");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        input.Write("Part3");
        input.Close();
    });
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    writer.join();
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("streaming has data", data.has_value());
    ASSERT_EQUAL("streaming result", StormByte::String::FromByteVector(*data), std::string("Part1Part2Part3"));
    
    RETURN_TEST("test_pipeline_streaming_data", 0);
}

int test_pipeline_byte_arithmetic() {
    Pipeline pipeline;
    
    // Stage 1: Add 1 to each byte
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<int>(byte) + 1));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 2: Multiply by 2
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<int>(byte) * 2));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 3: Divide by 2
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<int>(byte) / 2));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 4: Subtract 1
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<int>(byte) - 1));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Create input: {1, 2, 3, 4, 5}
    std::vector<std::byte> input_data = {
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}
    };
    
    Producer input;
    input.Write(input_data);
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    wait_for_pipeline_completion(result);
    
    auto data = CONSUME(result, 0);
    ASSERT_TRUE("byte arithmetic has data", data.has_value());
    ASSERT_EQUAL("byte arithmetic size", data->size(), static_cast<std::size_t>(5));
    
    // Verify each byte: ((x+1)*2)/2-1 = x
    ASSERT_EQUAL("byte 0", static_cast<int>((*data)[0]), 1);
    ASSERT_EQUAL("byte 1", static_cast<int>((*data)[1]), 2);
    ASSERT_EQUAL("byte 2", static_cast<int>((*data)[2]), 3);
    ASSERT_EQUAL("byte 3", static_cast<int>((*data)[3]), 4);
    ASSERT_EQUAL("byte 4", static_cast<int>((*data)[4]), 5);
    
    RETURN_TEST("test_pipeline_byte_arithmetic", 0);
}

int test_pipeline_large_concurrent_stress() {
    Pipeline pipeline;
    
    // Stage 1: XOR with 0x55
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(byte ^ std::byte{0x55});
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 2: Add 17 to each byte
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<uint8_t>(byte) + 17));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 3: NOT (bitwise complement)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(~byte);
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 4: XOR with 0xAA
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(byte ^ std::byte{0xAA});
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 5: Multiply by 3 (mod 256)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<uint8_t>(byte) * 3));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 6: Rotate left by 3 bits
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    uint8_t val = static_cast<uint8_t>(byte);
                    result.push_back(static_cast<std::byte>((val << 3) | (val >> 5)));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 7: Subtract 42
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<uint8_t>(byte) - 42));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 8: XOR with 0x33
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(byte ^ std::byte{0x33});
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 9: UNDO - XOR with 0x33 (XOR is self-inverse)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(byte ^ std::byte{0x33});
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 10: UNDO - Add 42 (reverse of subtract 42)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<uint8_t>(byte) + 42));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 11: UNDO - Rotate right by 3 bits (reverse of rotate left)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    uint8_t val = static_cast<uint8_t>(byte);
                    result.push_back(static_cast<std::byte>((val >> 3) | (val << 5)));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 12: UNDO - Multiply by 171 (modular inverse of 3 mod 256)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<uint8_t>(byte) * 171));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 13: UNDO - XOR with 0xAA (XOR is self-inverse)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(byte ^ std::byte{0xAA});
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 14: UNDO - NOT (bitwise complement is self-inverse)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(~byte);
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 15: UNDO - Subtract 17 (reverse of add 17)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(static_cast<std::byte>(static_cast<uint8_t>(byte) - 17));
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Stage 16: UNDO - XOR with 0x55 (XOR is self-inverse)
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::vector<std::byte> result;
                result.reserve(data->size());
                for (const auto& byte : *data) {
                    result.push_back(byte ^ std::byte{0x55});
                }
                out.Write(result);
            }
        }
        out.Close();
    });
    
    // Create large test data
    const std::size_t data_size = LARGE_TEST_SIZE_KB * 1024;
    std::vector<std::byte> input_data;
    input_data.reserve(data_size);
    
    // Fill with pseudo-random pattern for better testing
    for (std::size_t i = 0; i < data_size; ++i) {
        input_data.push_back(static_cast<std::byte>((i * 31 + 17) % 256));
    }
    
    Producer input;
    
    // Writer thread: writes data in chunks (faster than pipeline processing)
    std::thread writer([&input, &input_data]() {
        const std::size_t chunk_size = 4096; // 4KB chunks
        std::size_t offset = 0;
        
        while (offset < input_data.size()) {
            std::size_t to_write = std::min(chunk_size, input_data.size() - offset);
            std::vector<std::byte> chunk(input_data.begin() + offset, input_data.begin() + offset + to_write);
            input.Write(chunk);
            offset += to_write;
            // Writer is faster - no delay needed, just yield
            StormByte::System::Yield();
        }
        input.Close();
    });
    
    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);
    
    writer.join();
    
        // Wait for pipeline completion without sleeps
    wait_for_pipeline_completion(result);
    
    // Read result in chunks (slower reader)
    std::vector<std::byte> output_data;
    output_data.reserve(data_size);
    
    while (result.AvailableBytes() > 0) {
        auto chunk = CONSUME(result, 2048); // 2KB chunks (smaller than writer)
        if (chunk && !chunk->empty()) {
            output_data.insert(output_data.end(), chunk->begin(), chunk->end());
        }
        StormByte::System::Yield();
    }
    
    // Verify size
    ASSERT_EQUAL("large stress test size", output_data.size(), data_size);
    
    // Verify data integrity - after 16 stages (8 transforms + 8 inverse), should match original
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

    // Stage 1: uppercase
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });

    // Stage 2: replace spaces
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = CONSUME(in, 0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                std::replace(str.begin(), str.end(), ' ', '-');
                out.Write(str);
            }
        }
        out.Close();
    });

    Producer input;
    input.Write("sync mode test");
    input.Close();

    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Sync, logging);

    // In Sync mode, processing should have completed already
    ASSERT_FALSE("sync result writable", result.IsWritable());

    auto data = CONSUME(result, 0);
    ASSERT_TRUE("sync has data", data.has_value());
    ASSERT_EQUAL("sync transformation", StormByte::String::FromByteVector(*data), std::string("SYNC-MODE-TEST"));

    RETURN_TEST("test_pipeline_sync_execution", 0);
}

int test_pipeline_interrupted_by_seterror() {
    Pipeline pipeline;

    // Construct a long pipeline with stages that check writability and bail fast on error
    for (int i = 0; i < 8; ++i) {
        pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
            while (!in.EoF()) {
                auto data = CONSUME(in, 0);
                if (data && !data->empty()) {
                    // Simulate some work but check for cancellation via IsWritable()
                    for (int k = 0; k < 200; ++k) {
                        if (!out.IsWritable()) {
                            return; // interrupted
                        }
                        StormByte::System::Yield();
                    }
                    // Attempt to write; if interrupted, Write may fail or be ignored
                    if (!out.IsWritable()) return;
                    out.Write(*data);
                }
            }
            if (out.IsWritable()) out.Close();
        });
    }

    // Prepare a reasonably large input
    Producer input;
    std::string payload(50000, 'X');
    input.Write(payload);
    input.Close();

    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);

    // Immediately signal error to interrupt the pipeline
    pipeline.SetError();

    // Wait for consumers to observe unwritable state
    wait_for_pipeline_completion(result);

    // With early interruption, final buffer should have no data and be at EOF
    ASSERT_FALSE("interrupted not writable", result.IsWritable());
    ASSERT_TRUE("interrupted eof", result.EoF());
    ASSERT_EQUAL("interrupted size zero", result.AvailableBytes(), static_cast<std::size_t>(0));

    RETURN_TEST("test_pipeline_interrupted_by_seterror", 0);
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

    if (result == 0) {
        std::cout << "Pipeline tests passed!" << std::endl;
    } else {
        std::cout << result << " Pipeline tests failed." << std::endl;
    }
    return result;
}
