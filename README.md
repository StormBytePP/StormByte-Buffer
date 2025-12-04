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
    StormByte::Buffer::DataType data;
    auto res = fifo.Read(5, data);               // Read "Hello" into `data`
    if (res.has_value()) {
        std::string s(reinterpret_cast<const char*>(data.data()), data.size());
        // s == "Hello", data remains in buffer
    }

    // Seek and read more
    fifo.Seek(6, Position::Absolute);       // Move to "World"
    StormByte::Buffer::DataType more;
    auto res_more = fifo.Read(5, more);     // Read "World" into `more`

    // Destructive read (removes data)
    StormByte::Buffer::DataType extracted;
    auto res_ex = fifo.Extract(11, extracted);      // Extracts up to 11 bytes into `extracted`
    // Buffer is now empty (if extraction succeeded)
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
            StormByte::Buffer::DataType data;
            auto res = consumer.Extract(0, data); // Extract all available into `data`
            if (res.has_value() && !data.empty()) {
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

> Note: The older `Forwarder` adapter has been removed from the public API.
> Use the provided `ReadOnly`/`WriteOnly` implementations (for example
> `SharedFIFO`/`FIFO`) together with `Bridge` for passthrough scenarios.

#### Bridge

`Bridge` is a lightweight adapter that forwards bytes from a `ReadOnly` source to a
`WriteOnly` sink in user-space. It does not require (or reference) the old `Forwarder`
adapter; instead, construct a `Bridge` with any objects implementing the `ReadOnly`
and `WriteOnly` interfaces (for example `SharedFIFO` for reading and `FIFO` for
writing).

- **Purpose**: Move bytes from a source to a sink without custom buffering logic.
- **Behaviour**:
    - `Passthrough()` (no arguments) reads until end-of-file (EoF) from the read
        handler and writes everything to the write handler.
    - `Passthrough(count)` reads up to `count` bytes and writes them to the sink.
        If `count == 0`, the call will process all currently available bytes but will
        not block until EoF; to process until EoF use the parameterless `Passthrough()`.
    - On failure, `Passthrough()` returns an `ExpectedVoid<Error>` describing the
        underlying read or write error.

**Usage example (recommended):**

```cpp
#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/shared_fifo.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/string.hxx>

using StormByte::Buffer::Bridge;
using StormByte::Buffer::SharedFIFO;
using StormByte::Buffer::FIFO;

int main() {
        SharedFIFO read_fifo;   // ReadOnly implementation
        FIFO write_fifo;        // WriteOnly implementation

        const std::string test_data = "ABCD";
        (void)read_fifo.Write(test_data);
        read_fifo.Close();

        Bridge bridge(read_fifo, write_fifo, 4096); // chunk size
        auto res = bridge.Passthrough(); // forwards all bytes until EoF
        if (!res.has_value()) {
                // handle error
        }

        std::vector<std::byte> output;
        output.reserve(write_fifo.AvailableBytes());
        (void)write_fifo.Extract(write_fifo.AvailableBytes(), output);
        std::string result = StormByte::String::FromByteVector(output);
        // result == "ABCD"
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
            StormByte::Buffer::DataType data;
            auto res = in.Extract(0, data);
            if (res.has_value() && !data.empty()) {
                std::string str(reinterpret_cast<const char*>(data.data()), data.size());
                for (auto& c : str) c = std::toupper(c);
                out.Write(str);
            }
        }
        out.Close();
    });

    // Stage 2: Replace spaces with underscores
    pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
        while (!in.EoF()) {
            StormByte::Buffer::DataType data;
            auto res = in.Extract(0, data);
            if (res.has_value() && !data.empty()) {
                std::string str(reinterpret_cast<const char*>(data.data()), data.size());
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
            StormByte::Buffer::DataType data;
            auto res = in.Extract(0, data);
            if (res.has_value() && !data.empty()) {
                out.Write(data);
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
    StormByte::Buffer::DataType final_data;
    auto final_res = result.Extract(0, final_data);
    if (final_res.has_value()) {
        std::string output(reinterpret_cast<const char*>(final_data.data()), final_data.size());
        // output == "[HELLO_WORLD]"
    }
}
```

### Error Handling

The library uses `std::expected`-like (`StormByte::Expected`) for error handling:

**Read/Extract operations** return `ExpectedVoid<ReadError>` and fill a `DataType` buffer passed as the second parameter:
```cpp
StormByte::Buffer::DataType data;
auto res = fifo.Read(100, data);  // Request up to 100 bytes into `data`
if (res.has_value()) {
    // Success - use `data`
    process(data);
} else {
    // Error - insufficient data available or other read error
    std::cerr << "Read error: " << res.error()->what() << std::endl;
}
```

**Write operations** return `ExpectedVoid<WriteError>`:
```cpp
auto result = producer.Write("Some data");
if (result.has_value()) {
    // Success - data written
} else {
    // Error - buffer closed or in error state
    std::cerr << "Write error: " << result.error()->what() << std::endl;
}
```

Additional notes on end-of-stream and errors:

- `Consumer::EoF()` returns true when the buffer is unreadable (closed or in error) and there are no available bytes; prefer it to detect stream completion in consumers.
- `Producer::SetError()` marks a buffer as erroneous (unreadable and unwritable); blocked `Read()`/`Extract()` calls wake and return an error. Use `IsReadable()`/`IsWritable()` to query state.


## Contributing

Contributions are welcome! Please fork the repository and submit pull requests for any enhancements or bug fixes.

## License

This project is licensed under GPL v3 License - see the [LICENSE](LICENSE) file for details.
