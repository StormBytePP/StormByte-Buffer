# StormByte

StormByte is a comprehensive, cross-platform C++ library aimed at easing system programming, configuration management, logging, and database handling tasks. This library provides a unified API that abstracts away the complexities and inconsistencies of different platforms (Windows, Linux).

## Features

- **Buffer Operations**: Handles different typesd of buffers

## Table of Contents

- [Repository](#Repository)
- [Installation](#Installation)
- [Modules](#Modules)
	- [Base](https://dev.stormbyte.org/StormByte)
	- **Buffer**
	- [Config](https://dev.stormbyte.org/StormByte-Config)
	- [Crypto](https://dev.stormbyte.org/StormByte-Crypto)
	- [Database](https://dev.stormbyte.org/StormByte-Database)
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

A byte-oriented buffer with automatic growth on demand. Not thread-safe by itself.

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
    FIFO fifo(16);                          // 16-byte initial capacity
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

**Usage example:**

```cpp
#include <StormByte/buffer/shared_fifo.hxx>
#include <thread>

using StormByte::Buffer::SharedFIFO;

int main() {
    auto fifo = std::make_shared<SharedFIFO>();
    
    // Writer thread
    std::thread writer([fifo]() {
        fifo->Write("Data chunk 1");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fifo->Write("Data chunk 2");
        fifo->Close();                      // Signal completion
    });
    
    // Reader thread (blocks until data available)
    std::thread reader([fifo]() {
        while (!fifo->IsClosed() || !fifo->Empty()) {
            auto data = fifo->Extract(0);   // Extract all available
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

High-level interfaces for producer-consumer patterns with shared buffers.

- **Purpose**: Simplified API for producer-consumer workflows
- **Key Features**:
  - `Producer`: Write-only interface
  - `Consumer`: Read-only interface
  - Both share the same underlying `SharedFIFO`
  - Multiple producers/consumers can share one buffer
- **API**:
  - Producer: `Write()`, `Close()`, `Reserve()`, `Consumer()`
  - Consumer: `Read()`, `Extract()`, `Size()`, `Empty()`, `IsClosed()`, `Seek()`

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
        while (!consumer.IsClosed() || !consumer.Empty()) {
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
- **API**: `AddPipe(PipeFunction)`, `Process(Consumer)`

**Usage example:**

```cpp
#include <StormByte/buffer/pipeline.hxx>
#include <cctype>
#include <algorithm>

using StormByte::Buffer::Pipeline;
using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int main() {
    Pipeline pipeline;
    
    // Stage 1: Convert to uppercase
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
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
    pipeline.AddPipe([](Consumer in, Producer out) {
        while (!in.IsClosed() || !in.Empty()) {
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
    
    // Process data through pipeline
    Producer input;
    input.Write("hello world");
    input.Close();
    
    Consumer result = pipeline.Process(input.Consumer());
    
    // Wait and read result
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto final_data = result.Extract(0);
    if (final_data) {
        std::string output(reinterpret_cast<const char*>(final_data->data()), final_data->size());
        // output == "[HELLO_WORLD]"
    }
}
```

### Error Handling

The library uses `std::expected` for error handling in `Read()` and `Extract()` operations:

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

## Contributing

Contributions are welcome! Please fork the repository and submit pull requests for any enhancements or bug fixes.

## License

This project is licensed under GPL v3 License - see the [LICENSE](LICENSE) file for details.
