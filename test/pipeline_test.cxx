#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/string.hxx>
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

int test_pipeline_empty() {
    Pipeline pipeline;
    
    Producer input;
    input.Write("TEST");
    input.Close();
    
    // Empty pipeline should just pass through
    Consumer result = pipeline.Process(input.Consumer());
    
    // Give threads time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("empty pipeline has data", data.has_value());
    ASSERT_EQUAL("empty pipeline content", StormByte::String::FromByteVector(*data), std::string("TEST"));
    
    RETURN_TEST("test_pipeline_empty", 0);
}

int test_pipeline_single_stage() {
    Pipeline pipeline;
    
    // Single stage: uppercase transformation
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("single stage has data", data.has_value());
    ASSERT_EQUAL("single stage uppercase", StormByte::String::FromByteVector(*data), std::string("HELLO WORLD"));
    
    RETURN_TEST("test_pipeline_single_stage", 0);
}

int test_pipeline_two_stages() {
    Pipeline pipeline;
    
    // Stage 1: uppercase
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Stage 2: replace spaces with underscores
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("two stages has data", data.has_value());
    ASSERT_EQUAL("two stages transformation", StormByte::String::FromByteVector(*data), std::string("HELLO_WORLD_TEST"));
    
    RETURN_TEST("test_pipeline_two_stages", 0);
}

int test_pipeline_three_stages() {
    Pipeline pipeline;
    
    // Stage 1: uppercase
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Stage 2: replace spaces
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                std::string str = StormByte::String::FromByteVector(*data);
                std::replace(str.begin(), str.end(), ' ', '-');
                out.Write(str);
            }
        }
        out.Close();
    });
    
    // Stage 3: add prefix and suffix
    pipeline.AddPipe([](Consumer in, Producer out) {
        out.Write("[");
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("three stages has data", data.has_value());
    ASSERT_EQUAL("three stages transformation", StormByte::String::FromByteVector(*data), std::string("[TEST-DATA]"));
    
    RETURN_TEST("test_pipeline_three_stages", 0);
}

int test_pipeline_incremental_processing() {
    Pipeline pipeline;
    
    // Stage that processes data incrementally
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(1); // Extract one byte at a time
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("incremental has data", data.has_value());
    ASSERT_EQUAL("incremental processing", StormByte::String::FromByteVector(*data), std::string("ABC"));
    
    RETURN_TEST("test_pipeline_incremental_processing", 0);
}

int test_pipeline_filter_stage() {
    Pipeline pipeline;
    
    // Stage that filters out non-alphabetic characters
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("filter has data", data.has_value());
    ASSERT_EQUAL("filter stage", StormByte::String::FromByteVector(*data), std::string("HelloWorld"));
    
    RETURN_TEST("test_pipeline_filter_stage", 0);
}

int test_pipeline_multiple_writes() {
    Pipeline pipeline;
    
    // Stage that duplicates each piece of data
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("multiple writes has data", data.has_value());
    ASSERT_EQUAL("multiple writes", StormByte::String::FromByteVector(*data), std::string("ABAB"));
    
    RETURN_TEST("test_pipeline_multiple_writes", 0);
}

int test_pipeline_empty_input() {
    Pipeline pipeline;
    
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
        }
        out.Close();
    });
    
    Producer input;
    input.Close(); // Close without writing
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("empty input has result", data.has_value());
    ASSERT_EQUAL("empty input size", data->size(), static_cast<std::size_t>(0));
    
    RETURN_TEST("test_pipeline_empty_input", 0);
}

int test_pipeline_large_data() {
    Pipeline pipeline;
    
    // Stage that counts characters
    pipeline.AddPipe([](Consumer in, Producer out) {
        std::size_t count = 0;
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("large data has result", data.has_value());
    ASSERT_EQUAL("large data count", StormByte::String::FromByteVector(*data), std::string("10000"));
    
    RETURN_TEST("test_pipeline_large_data", 0);
}

int test_pipeline_reuse() {
    Pipeline pipeline;
    
    // Stage that adds prefix
    pipeline.AddPipe([](Consumer in, Producer out) {
        out.Write(">");
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
        
        Consumer result1 = pipeline.Process(input1.Consumer());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto data1 = result1.Extract(0);
        ASSERT_TRUE("reuse first has data", data1.has_value());
        ASSERT_EQUAL("reuse first result", StormByte::String::FromByteVector(*data1), std::string(">TEST1"));
    }
    
    // Second use
    {
        Producer input2;
        input2.Write("TEST2");
        input2.Close();
        
        Consumer result2 = pipeline.Process(input2.Consumer());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto data2 = result2.Extract(0);
        ASSERT_TRUE("reuse second has data", data2.has_value());
        ASSERT_EQUAL("reuse second result", StormByte::String::FromByteVector(*data2), std::string(">TEST2"));
    }
    
    RETURN_TEST("test_pipeline_reuse", 0);
}

int test_pipeline_copy_constructor() {
    Pipeline pipeline1;
    
    pipeline1.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline2.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("copy constructor has data", data.has_value());
    ASSERT_EQUAL("copy constructor works", StormByte::String::FromByteVector(*data), std::string("TEST"));
    
    RETURN_TEST("test_pipeline_copy_constructor", 0);
}

int test_pipeline_move_constructor() {
    Pipeline pipeline1;
    
    pipeline1.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline2.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("move constructor has data", data.has_value());
    ASSERT_EQUAL("move constructor works", StormByte::String::FromByteVector(*data), std::string("test"));
    
    RETURN_TEST("test_pipeline_move_constructor", 0);
}

int test_pipeline_addpipe_move() {
    Pipeline pipeline;
    
    auto func = [](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("addpipe move has data", data.has_value());
    ASSERT_EQUAL("addpipe move works", StormByte::String::FromByteVector(*data), std::string("MOVE"));
    
    RETURN_TEST("test_pipeline_addpipe_move", 0);
}

int test_pipeline_word_count() {
    Pipeline pipeline;
    
    // Count words (space-separated)
    pipeline.AddPipe([](Consumer in, Producer out) {
        std::size_t word_count = 0;
        std::string buffer;
        
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("word count has data", data.has_value());
    ASSERT_EQUAL("word count result", StormByte::String::FromByteVector(*data), std::string("6"));
    
    RETURN_TEST("test_pipeline_word_count", 0);
}

int test_pipeline_reverse_string() {
    Pipeline pipeline;
    
    // Reverse the string
    pipeline.AddPipe([](Consumer in, Producer out) {
        std::string buffer;
        
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("reverse has data", data.has_value());
    ASSERT_EQUAL("reverse result", StormByte::String::FromByteVector(*data), std::string("FEDCBA"));
    
    RETURN_TEST("test_pipeline_reverse_string", 0);
}

int test_pipeline_streaming_data() {
    Pipeline pipeline;
    
    // Pass through with small delay to simulate processing
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    writer.join();
    
    // Wait for all processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto data = result.Extract(0);
    ASSERT_TRUE("streaming has data", data.has_value());
    ASSERT_EQUAL("streaming result", StormByte::String::FromByteVector(*data), std::string("Part1Part2Part3"));
    
    RETURN_TEST("test_pipeline_streaming_data", 0);
}

int test_pipeline_byte_arithmetic() {
    Pipeline pipeline;
    
    // Stage 1: Add 1 to each byte
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
            auto data = in.Extract(0);
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
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    auto data = result.Extract(0);
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

    if (result == 0) {
        std::cout << "Pipeline tests passed!" << std::endl;
    } else {
        std::cout << result << " Pipeline tests failed." << std::endl;
    }
    return result;
}
