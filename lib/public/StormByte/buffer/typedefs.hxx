#include <StormByte/buffer/exception.hxx>
#include <StormByte/expected.hxx>

#include <cstddef>
#include <vector>

namespace StormByte::Buffer {
	template<class Exception>
	using ExpectedData	= Expected<std::vector<std::byte>, Exception>;
}