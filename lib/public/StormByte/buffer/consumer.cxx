#include <StormByte/buffer/consumer.hxx>

using namespace StormByte::Buffer;

FIFO Consumer::ReadUntilEoF() {
	Buffer::FIFO result;
	
	while (!EoF()) {
		auto data = Read();
		if (data) {
			(void)result.Write(*data);
		} else {
			// On error, break the loop
			break;
		}
	}
	
	return result;
}

FIFO Consumer::ExtractUntilEoF() {
	Buffer::FIFO result;
	
	while (!EoF()) {
		auto data = Extract();
		if (data) {
			(void)result.Write(*data);
		} else {
			// On error, break the loop
			break;
		}
	}
	
	return result;
}