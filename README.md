# StormByte

StormByte is a comprehensive, cross-platform C++ library aimed at easing system programming, configuration management, logging, and database handling tasks. This library provides a unified API that abstracts away the complexities and inconsistencies of different platforms (Windows, Linux).

## Features

- **System Operations**: Handles pipes, processes, and system variables seamlessly across different platforms.

## Table of Contents

- [Repository](#Repository)
- [Installation](#Installation)
- [Modules](#Modules)
	- [Base](https://dev.stormbyte.org/StormByte)
	- [Config](https://dev.stormbyte.org/StormByte-Config)
	- [Crypto](https://dev.stormbyte.org/StormByte-Crypto)
	- [Database](https://dev.stormbyte.org/StormByte-Database)
	- **Memory**
	- [Multimedia](https://dev.stormbyte.org/StormByte-Multimedia)
	- [Network](https://dev.stormbyte.org/StormByte-Network)
	- [System](https://dev.stormbyte.org/StormByte-System)
- [Contributing](#Contributing)
- [License](#License)

## Repository

You can visit the code repository at [GitHub](https://github.com/StormBytePP/StormByte-Memory)

## Installation

### Prerequisites

Ensure you have the following installed:

- C++23 compatible compiler
- CMake 3.12 or higher

### Building

To build the library, follow these steps:

```sh
git clone https://github.com/StormBytePP/StormByte-Memory.git
cd StormByte-Memory
mkdir build
cd build
cmake ..
make
```

## Modules

### Memory

#### FIFO (Ring Buffer)

- Purpose: byte-oriented circular buffer with grow-on-demand.
- Highlights: wrap-aware writes, partial/full reads, zero-copy fast path when contiguous, `Clear()` restores initial capacity.
- Key API: `Size()`, `Capacity()`, `Empty()`, `Full()`, `Clear()`, `Reserve(n)`, `Write(std::string)`, `Write(std::vector<std::byte> const&/&&)`, `Read(count=0)`.

Usage example:

```cpp
#include <StormByte/memory/buffer/fifo.hxx>
#include <string>
#include <vector>
#include <cstddef>

using StormByte::Memory::Buffer::FIFO;

int main() {
	FIFO fifo(16);                   // start with 16-byte capacity
	fifo.Write(std::string("Hello"));

	// Write bytes from a vector
	std::vector<std::byte> bytes = { std::byte{' '}, std::byte{'W'}, std::byte{'o'}, std::byte{'r'}, std::byte{'l'}, std::byte{'d'} };
	fifo.Write(bytes);

	// Read all available bytes
	auto out = fifo.Read();
	std::string s(reinterpret_cast<const char*>(out.data()), out.size());
	// s == "Hello World"

	// Memory behavior helpers
	fifo.Clear();                    // empties and restores capacity to 16
	fifo.Reserve(64);                // ensure capacity is at least 64
}
```

## Contributing

Contributions are welcome! Please fork the repository and submit pull requests for any enhancements or bug fixes.

## License

This project is licensed under GPL v3 License - see the [LICENSE](LICENSE) file for details.
