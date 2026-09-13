# StormByte-Buffer

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Buffer**: FIFO, SharedFIFO, Ring, Producer/Consumer, Hopper, Sink and pipelines for the StormByte C++ suite.

It depends on [StormByte Base 1.1.0](https://github.com/StormBytePP/StormByte/releases/tag/1.1.0) (or newer) and optionally [StormByte Logger 1.1.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.1.0) (or newer) for pipeline stages. Public headers live under `StormByte/buffer/`.

The suite is split on purpose. Base, Config, Crypto, Database, Logger, Multimedia, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **FIFO** — grow-on-demand byte buffer. Not thread-safe. `Read` / `Peek` keep data; `Extract` consumes it.
- **SharedFIFO** — thread-safe FIFO. `Read` / `Extract` block until data or `Close` / `SetError`.
- **Ring** — concurrent ring (`shared_mutex`, many-to-many).
- **Producer / Consumer** — write-only / read-only handles over a shared `Ring`.
- **Hopper** — single-producer single-consumer (SPSC) queue of typed items with optional capacity ceiling and condition-variable notifications.
- **Sink** — map of integer keys to Hopper buckets supporting Round-Robin or custom selection and terminal producer drain mode.
- **Bridge** — chunked passthrough from `ExternalReader` to `ExternalWriter`.
- **Pipeline** — stages chained with `ExecutionMode`: `Sync`, `Async`, `Parallel` (combinable).
- **Lifecycle** — `Close()`, `SetError()`, `EoF()`, `IsReadable()`, `IsWritable()`.
- **Private** — `LockFreeRing` is SPSC only, used between pipeline stages.

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormBytePP/StormByte) | Exceptions, Expected, serialization, strings, UUID, concepts | [/StormByte](https://dev.stormbyte.org/StormByte) |
| **Buffer** | This repository | [/StormByte-Buffer](https://dev.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormBytePP/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) | [/StormByte-Config](https://dev.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormBytePP/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the private tree | [/StormByte-Crypto](https://dev.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormBytePP/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://dev.stormbyte.org/StormByte-Database) |
| [Logger](https://github.com/StormBytePP/StormByte-Logger) | Stream logger with levels, headers, human-readable sizes and redaction (`ThreadedLog`) | [/StormByte-Logger](https://dev.stormbyte.org/StormByte-Logger) |
| [Multimedia](https://github.com/StormBytePP/StormByte-Multimedia) | Decode, encode and containers without raw FFmpeg types; codecs enabled only if present | [/StormByte-Multimedia](https://dev.stormbyte.org/StormByte-Multimedia) |
| [Network](https://github.com/StormBytePP/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) | [/StormByte-Network](https://dev.stormbyte.org/StormByte-Network) |
| [System](https://github.com/StormBytePP/StormByte-System) | Processes, pipes and environment variables across Linux, Windows and macOS | [/StormByte-System](https://dev.stormbyte.org/StormByte-System) |

## Table of Contents

- [What this module does](#what-this-module-does)
- [The rest of the suite](#the-rest-of-the-suite)
- [Installation](#installation)
- [Usage](#usage)
  - [FIFO](#fifo)
  - [Producer and Consumer](#producer-and-consumer)
  - [Hopper](#hopper)
  - [Sink](#sink)
  - [Pipeline](#pipeline)
- [Contributing](#contributing)
- [License](#license)

## Installation

Needs a C++26 compiler, CMake 3.28 or newer, [StormByte Base 1.1.0](https://github.com/StormBytePP/StormByte/releases/tag/1.1.0) or newer, and optionally [StormByte Logger 1.1.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.1.0) or newer.

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Buffer.git
cd StormByte-Buffer
cmake -S . -B build
cmake --build build
```

## Usage

Headers are `#include <StormByte/buffer/….hxx>`. Namespace root is `StormByte::Buffer`.

### FIFO

```cpp
#include <StormByte/buffer/fifo.hxx>

using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

int main() {
	FIFO fifo;
	fifo.Write("Hello World");

	StormByte::Buffer::DataType data;
	auto res = fifo.Read(5, data); // "Hello", still in the buffer
	fifo.Seek(6, Position::Absolute);

	StormByte::Buffer::DataType extracted;
	auto gone = fifo.Extract(5, extracted); // "World"
}
```

`FIFO` is not thread-safe. Concurrent writers/readers use `SharedFIFO` or `Ring`.

### Producer and Consumer

Prefer these over touching `SharedFIFO` / `Ring` by hand.

```cpp
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/consumer.hxx>
#include <thread>

using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int main() {
	Producer producer;
	Consumer consumer = producer.Consumer();

	std::thread writer([producer]() mutable {
		producer.Write("Data chunk 1");
		producer.Write("Data chunk 2");
		producer.Close();
	});

	std::thread reader([consumer]() mutable {
		while (!consumer.EoF()) {
			StormByte::Buffer::DataType data;
			auto res = consumer.Extract(0, data);
			if (res.has_value() && !data.empty()) {
				// process
			}
		}
	});

	writer.join();
	reader.join();
}
```

### Hopper

`Hopper<T>` is a single-producer single-consumer (SPSC) queue for discrete typed items (`StormByte::Type::MoveConstructible T`), unlike byte-oriented buffers (`FIFO`, `SharedFIFO`). It supports an optional capacity ceiling (0 = unbounded) where `Push` blocks when full, `Eof()` signaling for end of production, and consumer condition-variable notification via `Notify()`. Smart pointer types (`StormByte::Type::SmartPointer<T>`) automatically discard null items on `Push`.

```cpp
#include <StormByte/buffer/hopper.hxx>
#include <thread>
#include <memory>
#include <iostream>

using StormByte::Buffer::Hopper;

int main() {
	Hopper<std::unique_ptr<int>> hopper(5); // Bounded hopper: capacity 5

	std::thread producer([&hopper]() {
		for (int i = 0; i < 10; ++i) {
			hopper.Push(std::make_unique<int>(i));
		}
		hopper.Eof();
	});

	std::thread consumer([&hopper]() {
		while (!hopper.Empty() || !hopper.EoF()) {
			auto item = hopper.Pop();
			if (item) {
				std::cout << "Popped: " << *item << "\n";
			}
		}
	});

	producer.join();
	consumer.join();
}
```

### Sink

`Sink<T>` manages a collection of `Hopper<T>` buckets keyed by arbitrary integer identifiers (representing channels, tracks, sessions, etc.). Producers push items specifying a key. Consumers `Bind` to share hoppers under specific keys, or use `Drain()` on terminal producers so `Push` to un-bound keys drops items without waiting. `Pop()` retrieves items across buckets using Round-Robin or a custom `Select` index chooser callback.

`Sink::EoF()` contract details:
- **Zero hoppers:** `EoF()` is `true` only if this `Sink` was closed (via `Eof()` or destruction).
- **With hoppers:** `EoF()` is `true` when all hoppers are empty and `Hopper::EoF()` is `true`, even if this `Sink` itself did not call `Eof()` (since `Bind` shares the hopper and the producer may close it from the other `Sink`).
- **Dynamic Bind:** Binding a new key after `EoF()` returned `true` may cause `EoF()` to evaluate to `false` again if new work is attached.
- **Meaning:** `EoF()` does not mean "this object called `Eof()`", but rather "no items remain and none can enter current buckets".

```cpp
#include <StormByte/buffer/sink.hxx>
#include <thread>
#include <memory>
#include <string>
#include <iostream>

using StormByte::Buffer::Sink;

int main() {
	Sink<std::shared_ptr<std::string>> producerSink;
	Sink<std::shared_ptr<std::string>> consumerSink;

	// Share Hopper buckets for key 1 and key 2 with consumerSink
	producerSink.Bind(1, consumerSink);
	producerSink.Bind(2, consumerSink);

	std::thread writer([&producerSink]() {
		producerSink.Push(1, std::make_shared<std::string>("Message on Channel 1"));
		producerSink.Push(2, std::make_shared<std::string>("Message on Channel 2"));
		producerSink.Eof();
	});

	std::thread reader([&consumerSink]() {
		while (!consumerSink.EoF()) {
			auto msg = consumerSink.Pop(); // Round-robin across keys 1 and 2
			if (msg) {
				std::cout << "Received: " << *msg << "\n";
			}
		}
	});

	writer.join();
	reader.join();
}
```

### Pipeline

Stages must `Close()` or `SetError()` on the outgoing producer. Optional Logger is a stage argument.

```cpp
#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/logger/log.hxx>
#include <cctype>

using StormByte::Buffer::Pipeline;
using StormByte::Buffer::Producer;
using StormByte::Buffer::Consumer;

int main() {
	Pipeline pipeline;
	StormByte::Logger::Log logging(std::cout, StormByte::Logger::Level::LowLevel);

	pipeline.AddPipe([](Consumer in, Producer out, StormByte::Logger::Log& log) {
		while (!in.EoF()) {
			StormByte::Buffer::DataType data;
			auto res = in.Extract(0, data);
			if (res.has_value() && !data.empty()) {
				std::string str(reinterpret_cast<const char*>(data.data()), data.size());
				for (auto& c : str)
					c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				out.Write(str);
			}
		}
		out.Close();
	});
}
```

`ExecutionMode`: `Sync` (caller thread), `Async` (background), `Parallel` (one thread per stage). Flags combine (`Async | Parallel`).

## Contributing

Issues only on this repository. Fork and open a pull request against `master`.

## License

GNU Lesser General Public License version 3 or later. See [LICENSE](LICENSE) and <https://www.gnu.org/licenses/lgpl-3.0.html>.
