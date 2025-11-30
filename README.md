# StormByte

StormByte is a comprehensive, cross-platform C++ library aimed at easing system programming, configuration management, logging, and database handling tasks. This library provides a unified API that abstracts away the complexities and inconsistencies of different platforms (Windows, Linux).

## Features

- **Buffer Operations**: FIFO buffers, thread-safe shared buffers, producer-consumer interfaces, and pipelines

## Table of Contents

- [Repository](#Repository)
- [Installation](#Installation)
- [Modules](#Modules)
	- [Base](https://dev.stormbyte.org/StormByte)
	- **Buffer**
	- [Config](https://dev.stormbyte.org/StormByte-Config)
	- [Crypto](https://dev.stormbyte.org/StormByte-Crypto)
	- [Database](https://dev.stormbyte.org/StormByte-Database)
	- [Logger](https://dev.stormbyte.org/StormByte-Logger)
	- [Multimedia](https://dev.stormbyte.org/StormByte-Multimedia)
	- [Network](https://dev.stormbyte.org/StormByte-Network)
	- [System](https://dev.stormbyte.org/StormByte-System)
- [Contributing](#Contributing)
- [License](#License)

## Repository

You can visit the code repository at [GitHub](https://github.com/StormBytePP/StormByte-Buffer)

## Installation

### Prerequisites

Ensure you have the following installed:

- C++23 compatible compiler
- CMake 3.12 or higher

### Building

To build the library, follow these steps:

```sh
git clone https://github.com/StormBytePP/StormByte-Buffer.git
cd StormByte-Buffer
mkdir build
cd build
cmake ..
make
```

## Modules

### Buffer

The Buffer module provides a comprehensive set of classes for efficient data buffering, thread-safe producer-consumer patterns, and multi-stage data processing pipelines.

#### FIFO

A byte-oriented buffer backed by `std::deque<std::byte>` with automatic growth on demand. Not thread-safe by itself.

- **Purpose**: Basic buffer for sequential byte storage and retrieval
- **Key Features**: 
  - Non-destructive `Read()` with seek support
  - Destructive `Extract()` for consuming data
  - `Clear()` empties the buffer
- **API**: `Size()`, `Empty()`, `Clear()`, `Write()`, `Read(count)`, `Extract(count)`, `Seek()`

**Usage example:**

```cpp
#include <StormByte/buffer/fifo.hxx>

using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

int main() {
    FIFO fifo;                              // FIFO with dynamic growth
    fifo.Write("Hello World");
    
    // Non-destructive read
    auto data = fifo.Read(5);               // Read "Hello"
    if (data) {
        std::string s(reinterpret_cast<const char*>(data->data()), data->size());
        // s == "Hello", data remains in buffer
    }
    
    // Seek and read more
    fifo.Seek(6, Position::Absolute);       // Move to "World"
    auto more = fifo.Read(5);               // Read "World"
    
    // Destructive read (removes data)
    auto extracted = fifo.Extract(11);      // Extracts all data
    // Buffer is now empty
}
```

#### SharedFIFO

Thread-safe version of FIFO with blocking semantics for concurrent access.

- **Purpose**: Thread-safe buffer for multi-threaded applications
- **Key Features**:
  - All FIFO operations are thread-safe
  - `Read()` and `Extract()` block until data is available
  - `Close()` wakes waiting threads
  - Mutex and condition variable for synchronization
- **API**: Same as FIFO, plus `Close()`
- **API**: Same as FIFO, plus `Close()` and `SetError()`

Prefer using the `Producer`/`Consumer` wrappers rather than manipulating a `SharedFIFO` directly. The example below shows a writer and reader using these safe interfaces; readers should use `Consumer::EoF()` to detect completion and `Consumer::IsReadable()` to detect an error state.

**Usage example (Producer/Consumer):**

```cpp
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/consumer.hxx>
#include <thread>

using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int main() {
    Producer producer;
    Consumer consumer = producer.Consumer();

    // Writer thread
    std::thread writer([producer]() mutable {
        producer.Write("Data chunk 1");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        producer.Write("Data chunk 2");
        producer.Close(); // Signal completion
    });

    // Reader thread (blocks until data available)
    std::thread reader([consumer]() mutable {
        while (!consumer.EoF()) {
            auto data = consumer.Extract(0); // Extract all available
            if (data && !data->empty()) {
                // Process data...
            }
        }
    });

    writer.join();
    reader.join();
}
```

#### Producer and Consumer

High-level interfaces for producer-consumer patterns built on top of `SharedFIFO`.

- **Purpose**: Simplified API for producer-consumer workflows
- **Key Features**:
  - `Producer`: Write-only interface
  - `Consumer`: Read-only interface
  - Both share the same underlying `SharedFIFO`
  - Multiple producers/consumers can share one buffer
- **API**:
  - Producer: `Write()`, `Close()`, `SetError()`, `Consumer()`
  - Consumer: `Read()`, `Extract()`, `Size()`, `Empty()`, `EoF()`, `IsReadable()`, `IsWritable()`, `Seek()`

**Usage example:**

```cpp
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/consumer.hxx>
#include <thread>

using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int main() {
    Producer producer;
    Consumer consumer = producer.Consumer();  // Create consumer from producer

    // Producer thread
    std::thread writer([producer]() mutable {
        for (int i = 0; i < 10; i++) {
            producer.Write("Message " + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        producer.Close();
    });

    // Consumer thread
    std::thread reader([consumer]() mutable {
        while (!consumer.EoF()) {
            auto data = consumer.Extract(0);
            if (data && !data->empty()) {
                std::string msg(reinterpret_cast<const char*>(data->data()), data->size());
                std::cout << msg << std::endl;
            }
        }
    });

    writer.join();
    reader.join();
}
```

#### Pipeline

Multi-stage data processing pipeline with concurrent execution of stages.

- **Purpose**: Chain multiple transformation functions that process data concurrently
- **Key Features**:
  - Each stage runs in its own detached thread
  - Stages execute concurrently (parallel processing)
  - Data flows through thread-safe buffers
  - Reusable pipeline definition
- **API**: `AddPipe(PipeFunction)`, `Process(Consumer, ExecutionMode, StormByte::Logger::Log&)`

**Usage example:**

```cpp
#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/logger/log.hxx>
#include <cctype>
#include <algorithm>

using StormByte::Buffer::Pipeline;
using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int main() {
    Pipeline pipeline;
    // Logger instance passed to pipeline stages
    StormByte::Logger::Log logging(std::cout, StormByte::Logger::Level::LowLevel);

    // Stage 1: Convert to uppercase
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                std::string str(reinterpret_cast<const char*>(data->data()), data->size());
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });

    // Stage 2: Replace spaces with underscores
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                std::string str(reinterpret_cast<const char*>(data->data()), data->size());
                std::replace(str.begin(), str.end(), ' ', '_');
                out.Write(str);
            }
        }
        out.Close();
    });

    // Stage 3: Add prefix and suffix
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        out.Write("[");
        while (!in.EoF()) {
            auto data = in.Extract(0);
            if (data && !data->empty()) {
                out.Write(*data);
            }
        }
        out.Write("]");
        out.Close();
    });

    // Process data through pipeline
    Producer input;
    input.Write("hello world");
    input.Close();

    Consumer result = pipeline.Process(input.Consumer(), StormByte::Buffer::ExecutionMode::Async, logging);

    // Wait and read result without sleeps (poll EoF)
    while (!result.EoF()) { std::this_thread::yield(); }
    auto final_data = result.Extract(0);
    if (final_data) {
        std::string output(reinterpret_cast<const char*>(final_data->data()), final_data->size());
        // output == "[HELLO_WORLD]"
    }
}
```

### Error Handling

The library uses `std::expected`-like (`StormByte::Expected`) for error handling in `Read()` and `Extract()` operations:

```cpp
auto data = fifo.Read(100);  // Request more than available
if (data.has_value()) {
    // Success - use *data
    process(*data);
} else {
    // Error - insufficient data available
    std::cerr << "Insufficient data" << std::endl;
}
```

Additional notes on end-of-stream and errors:

- `Consumer::EoF()` returns true when the buffer is unreadable (closed or in error) and there are no available bytes; prefer it to detect stream completion in consumers.
- `Producer::SetError()` marks a buffer as erroneous (unreadable and unwritable); blocked `Read()`/`Extract()` calls wake and return an error. Use `IsReadable()`/`IsWritable()` to query state.


## Contributing

Contributions are welcome! Please fork the repository and submit pull requests for any enhancements or bug fixes.

## License

This project is licensed under GPL v3 License - see the [LICENSE](LICENSE) file for details.
