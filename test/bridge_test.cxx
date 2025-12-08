#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/string.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using StormByte::Buffer::Bridge;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::SharedFIFO;

int full_passthrough_test() {
	StormByte::Buffer::SharedFIFO read_fifo;
	StormByte::Buffer::FIFO write_fifo;

	const std::string test_data = "The quick brown fox jumps over the lazy dog.";
	(void)read_fifo.Write(test_data);
	read_fifo.Close();

	Bridge bridge(read_fifo, write_fifo, 4096); // Chunk size of 4 kbytes

	auto res = bridge.Passthrough();
	ASSERT_TRUE("bridge passthrough success", res);

	std::vector<std::byte> output_data;
	output_data.reserve(write_fifo.AvailableBytes());
	auto read_res = write_fifo.Extract(write_fifo.AvailableBytes(), output_data);
	ASSERT_TRUE("write fifo read success", read_res);

	std::string result = StormByte::String::FromByteVector(output_data);
	ASSERT_EQUAL("bridge passthrough content matches", result, test_data);

	RETURN_TEST("full_pass_through_test", 0);
}

int partial_passthrough_test() {
	StormByte::Buffer::SharedFIFO read_fifo;
	StormByte::Buffer::FIFO write_fifo;

	const std::string test_data = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
	(void)read_fifo.Write(test_data);
	read_fifo.Close();

	Bridge bridge(read_fifo, write_fifo, 16); // Small chunk size of 16 bytes

	auto res = bridge.Passthrough(32); // Only transfer 32 bytes
	ASSERT_TRUE("bridge partial passthrough success", res);

	std::vector<std::byte> output_data;
	output_data.reserve(write_fifo.AvailableBytes());
	auto read_res = write_fifo.Extract(write_fifo.AvailableBytes(), output_data);
	ASSERT_TRUE("write fifo read success", read_res);

	std::string result = StormByte::String::FromByteVector(output_data);
	ASSERT_EQUAL("bridge partial passthrough content matches", result, test_data.substr(0, 32));

	RETURN_TEST("partial_pass_through_test", 0);
}

int test_bridge_sharedfifo_threaded() {
	StormByte::Buffer::SharedFIFO read_fifo;
	StormByte::Buffer::FIFO write_fifo;

	std::string expected;
	expected.reserve(100);
	for (int i = 0; i < 10; ++i) {
		std::string chunkA(5, 'A');
		std::string chunkB(5, 'B');
		expected += chunkA + chunkB;
	}

	// Writer thread writes into SharedFIFO asynchronously
	std::thread writer([&]() {
		for (int i = 0; i < 10; ++i) {
			(void)read_fifo.Write(std::string(5, 'A'));
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			(void)read_fifo.Write(std::string(5, 'B'));
		}
		read_fifo.Close();
	});

	Bridge bridge(read_fifo, write_fifo, 8);

	auto res = bridge.Passthrough();
	ASSERT_TRUE("bridge passthrough success (threaded)", res);

	writer.join();

	std::vector<std::byte> output_data;
	output_data.reserve(write_fifo.AvailableBytes());
	auto read_res = write_fifo.Extract(write_fifo.AvailableBytes(), output_data);
	ASSERT_TRUE("write fifo read success (threaded)", read_res);

	std::string result = StormByte::String::FromByteVector(output_data);
	ASSERT_EQUAL("bridge passthrough content matches (threaded)", result, expected);

	RETURN_TEST("bridge_sharedfifo_threaded", 0);
}

int main() {
	int result = 0;

	//result += full_passthrough_test();
	//result += partial_passthrough_test();
	result += test_bridge_sharedfifo_threaded();

	if (result == 0) {
		std::cout << "Bridge tests passed!" << std::endl;
	} else {
		std::cout << result << " Bridge tests failed." << std::endl;
	}
	return result;
}
