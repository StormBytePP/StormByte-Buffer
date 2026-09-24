# StormByte-Buffer

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 or commercial](https://img.shields.io/badge/License-LGPL_v3_or_commercial-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Buffer**: FIFO, SharedFIFO, Ring, Producer/Consumer, Hopper, Sink, pipelines, Bridge and buffered I/O for the StormByte C++ suite.

It depends on [StormByte-String 1.0.0](https://github.com/StormBytePP/StormByte-String/releases/tag/1.0.0) or newer, which vendors [StormByte Base 2.0.0](https://github.com/StormBytePP/StormByte/releases/tag/2.0.0) or newer, [StormByte-System 2.0.0](https://github.com/StormBytePP/StormByte-System/releases/tag/2.0.0) or newer, and optionally [StormByte-Logger 2.0.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/2.0.0) or newer for pipeline stages (`Scope`). Public headers live under `StormByte/buffer/`.

The suite is split on purpose. Base, Config, Crypto, Database, Logger, Multimedia, Network, String and System are **other repositories**. This one does not implement them.

## Designed to interconnect

The point of this library is that its pieces plug into each other. A `Producer` is a `Consumer`. A `Bridge` pulls from any `ExternalReader` and pushes into any `ExternalWriter`. A `BufferedFileReader` *is* a `BufferedReader`; a `BufferedFileWriter` *is* a `BufferedWriter`. Leaves only implement origin hooks. Caches, prefetch, backpressure and `Seek` live in the bases.

Typical wires:

- `Producer` → `Consumer` (same ring).
- `Bridge(Consumer, BufferedFileWriter)` — drain a ring to a file.
- `BufferedFileReader` as a `BufferedReader` — sequential or seekable reads with a cache map.
- Future leaves (remote file, socket) inherit the File leaves and override the same hooks.

See [Bridge](#bridge), [IO::BufferedReader](#iobufferedreader), [IO::BufferedWriter](#iobufferedwriter) and [BufferedFileReader / BufferedFileWriter](#bufferedfilereader--bufferedfilewriter).

## What this module does

- **Data** — owned contiguous octets (`StormByte::Buffer::Data`). Vector-like API in lowercase (`size`, `data`, `span`, `begin`/`end`, …). Lengths are `StormByte::Size`. Use this type for every octet payload that enters or leaves Buffer; do not use `std::vector<std::byte>` across a DLL boundary on Windows.
- **FIFO** — grow-on-demand byte buffer. Not thread-safe. `Read` / `Peek` keep data; `Extract` consumes it.
- **SharedFIFO** — thread-safe FIFO. `Read` / `Extract` block until data or `Close` / `SetError`.
- **Ring** — concurrent ring (many-to-many).
- **Producer / Consumer** — write-only / read-only handles over a shared `Ring`. See [Producer and Consumer](#producer-and-consumer).
- **Hopper** — SPSC queue of typed items with optional capacity. See [Hopper](#hopper).
- **Sink** — map of integer keys to Hopper buckets. See [Sink](#sink).
- **Bridge** — chunked passthrough `ExternalReader` → `ExternalWriter`, with optional high-water. See [Bridge](#bridge).
- **IO::BufferedReader / IO::BufferedWriter** — session bases (`Open` / `Close` / `Tell` / `EoF`). Leaves implement `Origin*`. See [IO::BufferedReader](#iobufferedreader) and [IO::BufferedWriter](#iobufferedwriter).
- **BufferedFileReader / BufferedFileWriter** — file leaves. Path-only constructors pick device-tuned windows at `Open` via System `Device`; explicit constructors keep the knobs you pass. See [BufferedFileReader / BufferedFileWriter](#bufferedfilereader--bufferedfilewriter).
- **Pipeline** — stages chained with `ExecutionMode`. See [Pipeline](#pipeline).
- **Lifecycle** — `Close()`, `SetError()`, `EoF()`, `IsReadable()`, `IsWritable()`.

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormBytePP/StormByte) | Exceptions, Expected, serialization, UUID, concepts | [/StormByte](https://dev.stormbyte.org/StormByte) |
| **Buffer** | This repository | [/StormByte-Buffer](https://dev.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormBytePP/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) | [/StormByte-Config](https://dev.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormBytePP/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the public API | [/StormByte-Crypto](https://dev.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormBytePP/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://dev.stormbyte.org/StormByte-Database) |
| [Logger](https://github.com/StormBytePP/StormByte-Logger) | Stream logger with levels, headers, hierarchical components and `Scope` | [/StormByte-Logger](https://dev.stormbyte.org/StormByte-Logger) |
| [Multimedia](https://github.com/StormBytePP/StormByte-Multimedia) | Decode, encode and containers without raw FFmpeg types; codecs enabled only if present | [/StormByte-Multimedia](https://dev.stormbyte.org/StormByte-Multimedia) |
| [Network](https://github.com/StormBytePP/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) | [/StormByte-Network](https://dev.stormbyte.org/StormByte-Network) |
| [String](https://github.com/StormBytePP/StormByte-String) | Owned UTF-8 / wide text that can cross a DLL boundary (`String`, `WString`, `CString`, `WCString`) | [/StormByte-String](https://dev.stormbyte.org/StormByte-String) |
| [System](https://github.com/StormBytePP/StormByte-System) | Processes, pipes, `Device`, host and environment across Linux, Windows and macOS | [/StormByte-System](https://dev.stormbyte.org/StormByte-System) |

## Table of Contents

- [Designed to interconnect](#designed-to-interconnect)
- [What this module does](#what-this-module-does)
- [The rest of the suite](#the-rest-of-the-suite)
- [Installation](#installation)
- [Usage](#usage)
  - [FIFO](#fifo)
  - [Producer and Consumer](#producer-and-consumer)
  - [Hopper](#hopper)
  - [Sink](#sink)
  - [Bridge](#bridge)
  - [IO::BufferedReader](#iobufferedreader)
  - [IO::BufferedWriter](#iobufferedwriter)
  - [BufferedFileReader / BufferedFileWriter](#bufferedfilereader--bufferedfilewriter)
  - [Pipeline](#pipeline)
- [Support](#support)
- [Contributing](#contributing)
- [License](#license)

## Installation

Needs a C++26 compiler, CMake 3.28 or newer, [StormByte-String 1.0.0](https://github.com/StormBytePP/StormByte-String/releases/tag/1.0.0) or newer (which vendors [StormByte Base 2.0.0](https://github.com/StormBytePP/StormByte/releases/tag/2.0.0) or newer), [StormByte-System 2.0.0](https://github.com/StormBytePP/StormByte-System/releases/tag/2.0.0) or newer, and optionally [StormByte-Logger 2.0.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/2.0.0) or newer when pipeline stages take a logger.

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Buffer.git
cd StormByte-Buffer
cmake -S . -B build
cmake --build build
```

## Usage

Headers are `#include <StormByte/buffer/….hxx>`. Namespace root is `StormByte::Buffer`. I/O types live in `StormByte::Buffer::IO`.

Octet payloads use `StormByte::Buffer::Data`. `size()` is `StormByte::Size`. Text that leaves the module (`HexDump`) is `StormByte::String::String`.

### FIFO

```cpp
#include <StormByte/buffer/fifo.hxx>

using StormByte::Buffer::Data;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

int main() {
	FIFO fifo;
	fifo.Write("Hello World");

	Data data;
	auto res = fifo.Read(5, data); // "Hello", still in the buffer
	fifo.Seek(6, Position::Absolute);

	Data extracted;
	auto gone = fifo.Extract(5, extracted); // "World"
}
```

`FIFO` is not thread-safe. Concurrent writers/readers use `SharedFIFO` or `Ring`.

### Producer and Consumer

Prefer these over touching `SharedFIFO` / `Ring` by hand. A `Producer` yields a `Consumer` over the same ring; that `Consumer` is an `ExternalReader` and can feed a [Bridge](#bridge).

```cpp
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/consumer.hxx>
#include <thread>

using StormByte::Buffer::Consumer;
using StormByte::Buffer::Data;
using StormByte::Buffer::Producer;

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
			Data data;
			if (consumer.Extract(0, data) && !data.empty()) {
				// process
			}
		}
	});

	writer.join();
	reader.join();
}
```

### Hopper

`Hopper<T>` is a single-producer single-consumer (SPSC) queue for discrete typed items (`StormByte::Type::MoveConstructible T`). Capacity `0` is unbounded; `Push` blocks when a bounded hopper is full. `Eof()` ends production. Smart pointer types (`StormByte::Type::SmartPointer<T>`) discard null items on `Push`.

`Push` and `Pop` are the stable API. `hopper << item`, `hopper >> item` and `item >> hopper` do the same thing.

`Notify(cv)` stores a pointer to a condition variable the Hopper does **not** own. The Hopper outlives a typical consumer. Call `Unnotify()` before that CV is destroyed, otherwise a later producer `Eof` can signal a freed object.

```cpp
#include <StormByte/buffer/hopper.hxx>
#include <thread>
#include <memory>
#include <iostream>

using StormByte::Buffer::Hopper;

int main() {
	Hopper<std::unique_ptr<int>> hopper(5);

	std::thread producer([&hopper]() {
		for (int i = 0; i < 10; ++i)
			hopper << std::make_unique<int>(i);
		hopper.Eof();
	});

	std::thread consumer([&hopper]() {
		while (!hopper.Empty() || !hopper.EoF()) {
			auto item = hopper.Pop();
			if (item)
				std::cout << "Popped: " << *item << "\n";
		}
	});

	producer.join();
	consumer.join();
}
```

### Sink

`Sink<T>` maps integer keys to `Hopper<T>` buckets. Wire a consumer with `To(key)` / `>>` / `<<`. `Bind` is the old name and is `[[deprecated]]`.

`Sink::EoF()` contract:

- **Zero hoppers:** `true` only if this `Sink` was closed (`Eof()` or destruction).
- **With hoppers:** `true` when every hopper is empty and `Hopper::EoF()` is `true`, even if this `Sink` did not call `Eof()` (the producer may have closed a shared hopper).
- **Dynamic wire:** attaching a new key after `EoF()` was `true` may make `EoF()` `false` again.
- **Meaning:** no items remain and none can enter the current buckets.

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

	producerSink.To(1, consumerSink);
	producerSink.To(2, consumerSink);

	std::thread writer([&producerSink]() {
		producerSink.Push(1, std::make_shared<std::string>("Message on Channel 1"));
		producerSink.Push(2, std::make_shared<std::string>("Message on Channel 2"));
		producerSink.Eof();
	});

	std::thread reader([&consumerSink]() {
		while (!consumerSink.EoF()) {
			auto msg = consumerSink.Pop();
			if (msg)
				std::cout << "Received: " << *msg << "\n";
		}
	});

	writer.join();
	reader.join();
}
```

### Bridge

`Bridge` copies bytes from an `ExternalReader` to an `ExternalWriter` in chunks. Optional high-water (`0` = no extra cap; the writer may still apply its own backpressure). Use it when the source is a pipe or a ring and the sink is a file or another writer.

```cpp
#include <StormByte/buffer/io/bridge.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <StormByte/buffer/producer.hxx>

using StormByte::Buffer::Producer;
using StormByte::Buffer::IO::Bridge;
using StormByte::Buffer::IO::BufferedFileWriter;

int main() {
	Producer producer;
	BufferedFileWriter out("out.bin");
	out.Open();

	Bridge bridge(producer.Consumer(), out, /*high_water=*/0);
	producer.Write("payload");
	producer.Close();
	(void)bridge.Pump();
	out.Close();
}
```

### IO::BufferedReader

Session over a byte origin. The public type owns cache, prefetch, `Tell` and `Seek`. A leaf only implements `OriginOpen` / `OriginClose` / `OriginPull` and, if it can, `OriginCanSeek` / `OriginSeek` / `OriginHasSize` / `OriginSize`.

- `Open` is not idempotent. It calls the leaf `Setup()` once, then the origin.
- `Read` / `Peek` take a `FIFO` or a writable `std::span<std::byte>`.
- `Seek` is valid only when the origin is seekable. It is **not** guaranteed O(1). Seekable origins keep a map of cached spans; a hit does not drop the window. A miss seeks the origin and adds a span. `MaxMemory` is a cap; GC drops spans farthest from `Tell`. `MaxMemory == 0` disables the cache. Not seekable: one forward span; `Seek` returns `Failed` and does not call the hook.
- Prefetch is stopped before `Seek` and before a move. The next `Read` / `Peek` requests it again.

### IO::BufferedWriter

Session over a byte sink. The public type owns write buffering, `Dirty`, `Flush` and `Truncate`. A leaf implements `OriginOpen` / `OriginClose` / `OriginPush` and optionally `OriginFlush` / `OriginTruncate`.

- `WriteChunk == 0` is direct: each `Write` hits the origin and `Dirty()` stays `0`.
- `WriteChunk > 0` holds bytes until a full chunk, `Flush`, or `Close`.
- `BackPressure` is in **chunks**, not bytes. `0` disables that extra cap (the leaf may still refuse work).
- Device speed knobs are fixed at construction. `MaxMemory` / `MaxWait` remain settable.

### BufferedFileReader / BufferedFileWriter

File leaves of the bases above. They are meant to be derived from (same hooks, no extra setters for the path). Override `CreateDevice()` to return a `unique_ptr<StormByte::System::Device>` when a derived leaf needs its own measurement (no slicing).

Two constructors:

| Constructor | What happens at `Open` |
| --- | --- |
| `BufferedFileReader(path)` / `BufferedFileWriter(path)` | `Setup()` uses `CreateDevice()` and `Device::Window` to set `ReadAhead` (reader) or `WriteChunk` + `BackPressure` (writer). `MaxMemory` on the short reader ctor is 1 MiB. |
| `BufferedFileReader(path, read_ahead, max_memory)` / `BufferedFileWriter(path, write_chunk, backpressure)` | Those values stay. `Setup()` does not overwrite them. `(path, 0, 0)` is direct / no prefetch. |

The device does not change after construction, so there are no setters for `ReadAhead` / `WriteChunk` / `BackPressure` on the *policy of the device*. `MaxMemory` and `MaxWait` stay settable: they are cache and wait policy, not device speed.

`Setup()` is a protected hook on the base, called from `Open` before `OriginOpen`. A derived leaf can override `Setup()` (or skip the probe) and still reuse File origin hooks.

```cpp
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>

using StormByte::Buffer::FIFO;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::BufferedFileWriter;

int main() {
	BufferedFileReader in("in.bin");
	in.Open();
	FIFO dest;
	(void)in.Read(16, dest);

	BufferedFileWriter direct("out.bin", 0, 0);
	direct.Open();
	(void)direct.Write(dest);
	direct.Close();
}
```

### Pipeline

Stages receive `ExternalReader&`, `ExternalWriter&` and an optional `std::shared_ptr<Logger::Log>`. They must `out.Close()` or `out.SetError()`.

When `Process` gets a non-null logger it passes `log->Scope("Buffer/Pipeline")` to every stage. `%c` is then `Buffer/Pipeline`. Do not pre-scope `Buffer/Pipeline` on the argument. A stage that needs a leaf can `log->Scope("Decode")`.

```cpp
#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/buffer/external.hxx>
#include <StormByte/logger/log.hxx>
#include <cctype>
#include <memory>

using StormByte::Buffer::Data;
using StormByte::Buffer::Pipeline;
using StormByte::Buffer::Producer;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalWriter;
using StormByte::Logger::Log;
using StormByte::Logger::Level;

int main() {
	auto log = std::make_shared<Log>(std::cout, Level::Info, "[%L] %c");
	Pipeline pipeline;
	pipeline.AddPipe([](ExternalReader& in, ExternalWriter& out,
						std::shared_ptr<Log> log) {
		while (!in.EoF()) {
			Data data;
			if (in.Extract(0, data) && !data.empty()) {
				std::string str(reinterpret_cast<const char*>(data.data()),
					static_cast<std::size_t>(data.size()));
				for (auto& c : str)
					c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
				(void)out.Write(str);
			}
		}
		out.Close();
	});

	Producer input;
	(void)input.Write("hello");
	input.Close();
	auto out = pipeline.Process(input.Consumer(),
		StormByte::Buffer::ExecutionMode::Sync, log);
}
```

`ExecutionMode`: `Sync` (caller thread), `Async` (background), `Parallel` (one thread per stage). Flags combine (`Async | Parallel`).

## Support

StormByte is developed in spare time. Sponsorship is optional and does not buy features, priority or support.

- [GitHub Sponsors](https://github.com/sponsors/StormBytePP)
- [PayPal](https://paypal.me/StormBytePP)

## Contributing

Issues only on this repository. Fork and open a pull request against `master`.

## License

Dual license: GNU Lesser General Public License v3.0 or later, or a commercial license from the copyright holder. See [LICENSE](LICENSE), [COPYING.LGPLv3](COPYING.LGPLv3) and <https://www.gnu.org/licenses/lgpl-3.0.html>. Third-party trees under `thirdparty/` keep their own licenses.
