# StormByte-Buffer

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Buffer**: FIFO, SharedFIFO, Ring, Producer/Consumer and pipelines for the StormByte C++ suite.

It depends on [StormByte Base](https://github.com/StormBytePP/StormByte) and optionally [StormByte Logger](https://github.com/StormBytePP/StormByte-Logger) for pipeline stages. Public headers live under `StormByte/buffer/`.

The suite is split on purpose. Base, Config, Crypto, Database, Logger, Multimedia, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **FIFO** — grow-on-demand byte buffer. Not thread-safe. `Read` / `Peek` keep data; `Extract` consumes it.
- **SharedFIFO** — thread-safe FIFO. `Read` / `Extract` block until data or `Close` / `SetError`.
- **Ring** — concurrent ring (`shared_mutex`, many-to-many).
- **Producer / Consumer** — write-only / read-only handles over a shared `Ring`.
- **Bridge** — chunked passthrough from `ExternalReader` to `ExternalWriter`.
- **Pipeline** — stages chained with `ExecutionMode`: `Sync`, `Async`, `Parallel` (combinable).
- **Lifecycle** — `Close()`, `SetError()`, `EoF()`, `IsReadable()`, `IsWritable()`.
- **Private** — `LockFreeRing` is SPSC only, used between pipeline stages.

## The rest of the suite

| Module | Role |
| --- | --- |
| [Base](https://github.com/StormBytePP/StormByte) | Exceptions, `Expected`, little-endian serialization, strings, concepts — the suite root |
| [Buffer](https://github.com/StormBytePP/StormByte-Buffer) | This repository |
| [Config](https://github.com/StormBytePP/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) |
| [Crypto](https://github.com/StormBytePP/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the private tree |
| [Database](https://github.com/StormBytePP/StormByte-Database) | One API for SQLite, PostgreSQL and MariaDB: prepared statements and RAII transactions |
| [Logger](https://github.com/StormBytePP/StormByte-Logger) | Stream logger with levels, headers, human-readable sizes and redaction (`ThreadedLog`) |
| [Multimedia](https://github.com/StormBytePP/StormByte-Multimedia) | Decode, encode and containers without raw FFmpeg types; codecs enabled only if present |
| [Network](https://github.com/StormBytePP/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) |
| [System](https://github.com/StormBytePP/StormByte-System) | Processes, pipes and environment variables across Linux, Windows and macOS |

Docs sites (when published): [Base](https://dev.stormbyte.org/StormByte), [Buffer](https://dev.stormbyte.org/StormByte-Buffer), [Config](https://dev.stormbyte.org/StormByte-Config), [Crypto](https://dev.stormbyte.org/StormByte-Crypto), [Database](https://dev.stormbyte.org/StormByte-Database), [Logger](https://dev.stormbyte.org/StormByte-Logger), [Multimedia](https://dev.stormbyte.org/StormByte-Multimedia), [Network](https://dev.stormbyte.org/StormByte-Network), [System](https://dev.stormbyte.org/StormByte-System).

## Table of Contents

- [What this module does](#what-this-module-does)
- [The rest of the suite](#the-rest-of-the-suite)
- [Installation](#installation)
- [Usage](#usage)
  - [FIFO](#fifo)
  - [Producer and Consumer](#producer-and-consumer)
  - [Pipeline](#pipeline)
- [Contributing](#contributing)
- [License](#license)

## Installation

Needs a C++26 compiler, CMake 3.28 or newer, and [StormByte Base](https://github.com/StormBytePP/StormByte) ≥ 1.0.0.

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
