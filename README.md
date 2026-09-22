# StormByte-Buffer

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Buffer**: in-memory byte stores, typed queues, pipelines and buffered binary I/O.

It depends on [StormByte Base 1.2.0](https://github.com/StormBytePP/StormByte/releases/tag/1.2.0) or newer and optionally [StormByte Logger 1.2.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.2.0) or newer for pipeline stages (`Scope`). Public headers live under `StormByte/buffer/`.

## Designed to interconnect

The types in this module are one toolkit, not a pile of unrelated buffers.

- **Stores** hold octets or items: [FIFO](#fifo), SharedFIFO, [Ring](#producer-and-consumer), [Hopper](#hopper) / [Sink](#sink).
- **Handles** expose one side of a store: [Producer / Consumer](#producer-and-consumer) over a Ring, `ExternalReader` / `ExternalWriter` over a store.
- **Sessions** talk to a device: [IO::BufferedReader](#iobufferedreader) / [IO::BufferedWriter](#iobufferedwriter), with file leaves.
- **Pumps** move data between those tips: [Bridge](#bridge) (bytes, any pairing of External and IO) and [Pipeline](#pipeline) (stages over External tips).

A `Producer` is an `ExternalWriter`. A `Consumer` is an `ExternalReader`. A file leaf **is** a `BufferedReader` / `BufferedWriter`. `Bridge` and `Pipeline` take those tips by reference; they do not own them. Wire a live source to a live sink and let the pump run.

Byte path and item path stay distinct. `Bridge` moves octets. `Hopper` / `Sink` move typed items. Do not put a Hopper on a Bridge.

## What this module does

- **FIFO** — grow-on-demand byte buffer. Not thread-safe. `Read` / `Peek` keep data; `Extract` consumes it. See [FIFO](#fifo).
- **SharedFIFO** — thread-safe FIFO. `Read` / `Extract` block until data or `Close` / `SetError`.
- **Ring** — concurrent ring (`shared_mutex`, many-to-many). Prefer [Producer / Consumer](#producer-and-consumer) over touching it by hand.
- **Producer / Consumer** — write-only / read-only handles over a shared Ring. Also the usual External tips for [Bridge](#bridge) and [Pipeline](#pipeline).
- **Hopper** — SPSC queue of typed items. Not a Bridge tip. See [Hopper](#hopper).
- **Sink** — integer keys to Hopper buckets. Wire with `To(key)` / `>>` / `<<`. See [Sink](#sink).
- **ExternalReader / ExternalWriter** — abstract byte tips over a store (`ExternalBufferReader` / `ExternalBufferWriter` wrap FIFO / SharedFIFO / Consumer / Producer).
- **IO::BufferedReader** — coordinated binary read origin: prefetch, span cache, `Read` / `Peek` / `Seek` / `Tell` / `Size`. Leaves implement `Origin*` only. See [IO::BufferedReader](#iobufferedreader).
- **IO::BufferedWriter** — coordinated binary write sink: optional write-behind, `Write` / `Flush` / `Truncate`. Leaves implement `Origin*` only. See [IO::BufferedWriter](#iobufferedwriter).
- **IO::BufferedFileReader / IO::BufferedFileWriter** — filesystem leaves (binary `ifstream` / append `ofstream`).
- **Bridge** — worker that pumps any readable tip into any writable tip under `high_water`. See [Bridge](#bridge).
- **Pipeline** — stages over External tips, `ExecutionMode` `Sync` / `Async` / `Parallel`. See [Pipeline](#pipeline).
- **Lifecycle** — `Close()`, `SetError()`, `EoF()`, `IsReadable()`, `IsWritable()`.

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormBytePP/StormByte) | Exceptions, Expected, serialization, strings, UUID, concepts | [/StormByte](https://dev.stormbyte.org/StormByte) |
| **Buffer** | This repository | [/StormByte-Buffer](https://dev.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormBytePP/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) | [/StormByte-Config](https://dev.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormBytePP/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the private tree | [/StormByte-Crypto](https://dev.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormBytePP/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://dev.stormbyte.org/StormByte-Database) |
| [Logger](https://github.com/StormBytePP/StormByte-Logger) | Stream logger with levels, headers, hierarchical components and `Scope` | [/StormByte-Logger](https://dev.stormbyte.org/StormByte-Logger) |
| [Multimedia](https://github.com/StormBytePP/StormByte-Multimedia) | Decode, encode and containers without raw FFmpeg types; codecs enabled only if present | [/StormByte-Multimedia](https://dev.stormbyte.org/StormByte-Multimedia) |
| [Network](https://github.com/StormBytePP/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) | [/StormByte-Network](https://dev.stormbyte.org/StormByte-Network) |
| [System](https://github.com/StormBytePP/StormByte-System) | Processes, pipes and environment variables across Linux, Windows and macOS | [/StormByte-System](https://dev.stormbyte.org/StormByte-System) |

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
  - [Pipeline](#pipeline)
  - [IO::BufferedReader](#iobufferedreader)
  - [IO::BufferedWriter](#iobufferedwriter)
  - [IO::BufferedFileReader](#iobufferedfilereader)
  - [IO::BufferedFileWriter](#iobufferedfilewriter)
  - [Bridge](#bridge)
- [Contributing](#contributing)
- [License](#license)
- [Support](#support)

## Installation

Needs a C++26 compiler, CMake 3.28 or newer, [StormByte Base 1.2.0](https://github.com/StormBytePP/StormByte/releases/tag/1.2.0) or newer, and optionally [StormByte Logger 1.2.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.2.0) or newer when pipeline stages take a logger.

```sh
git clone --recursive https://github.com/StormBytePP/StormByte-Buffer.git
cd StormByte-Buffer
cmake -S . -B build
cmake --build build
```

## Usage

Headers are `#include <StormByte/buffer/….hxx>`. Namespace root is `StormByte::Buffer`. IO types live in `StormByte::Buffer::IO`.

### FIFO

In-memory octet store. `Write` appends. `Read` / `Peek` copy; `Extract` consumes. `Seek` here is the FIFO read cursor (`Position::Absolute` / `Relative`), not an IO session seek.

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

`FIFO` is not thread-safe. Concurrent writers/readers use `SharedFIFO` or a [Ring](#producer-and-consumer). Wrap a FIFO in `ExternalBufferReader` / `ExternalBufferWriter` when a [Bridge](#bridge) or [Pipeline](#pipeline) stage needs a tip.

### Producer and Consumer

Prefer these over touching `SharedFIFO` / `Ring` by hand. `Producer` writes the shared Ring; `Consumer` reads it. Copy the handles, not the Ring.

That pair is the usual in-memory tip for [Bridge](#bridge) (`Consumer` → `ExternalReader`, `Producer` → `ExternalWriter`) and the input of [Pipeline](#pipeline) (`Process` takes a `Consumer`).

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

`Producer::Occupied` / `Generic::Size` is live Ring occupancy. `Extract` on the Consumer lowers it; `Read` / `Peek` do not. A Bridge that applies `high_water` on a Producer sink needs that drop.

### Hopper

`Hopper<T>` is an SPSC **item** queue (`StormByte::Type::MoveConstructible T`), not a byte buffer. Capacity `0` is unbounded; `Push` blocks when a bounded hopper is full. `Eof()` ends production. Smart pointer types discard null items on `Push`.

`Push` / `Pop` stay. `hopper << item`, `hopper >> item` and `item >> hopper` do the same.

`Notify(cv)` stores a pointer to a condition variable the Hopper does **not** own. Call `Unnotify()` before that CV is destroyed.

Hopper and [Sink](#sink) are the item path. They are not Bridge tips.

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

`Sink<T>` maps integer keys to `Hopper<T>` buckets. Wire a consumer with `To(key)` / `>>` / `<<`. `Bind` is the old name and is gone from Unreleased.

`Sink::EoF()` contract:

- **Zero hoppers:** `true` only if this `Sink` was closed (`Eof()` or destruction).
- **With hoppers:** `true` when every hopper is empty and `Hopper::EoF()` is `true`, even if this `Sink` did not call `Eof()`.
- **Dynamic wire:** attaching a new key after `EoF()` was `true` may make `EoF()` `false` again.

Same rule as Hopper: items, not octets. See [Hopper](#hopper).

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

### Pipeline

Stages receive `ExternalReader&`, `ExternalWriter&` and an optional logger. They must `out.Close()` or `out.SetError()`.

`Process` takes a `Consumer` (see [Producer and Consumer](#producer-and-consumer)). When the logger is non-null it is scoped as `Buffer/Pipeline` before it reaches the stages. Do not pre-scope that name on the argument. A stage that needs a leaf can `Scope` further.

```cpp
#include <StormByte/buffer/pipeline.hxx>
#include <StormByte/buffer/external.hxx>
#include <StormByte/logger/log.hxx>
#include <cctype>
#include <memory>

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
			StormByte::Buffer::DataType data;
			if (in.Extract(0, data) && !data.empty()) {
				std::string str(reinterpret_cast<const char*>(data.data()), data.size());
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

Byte IO sessions are not Pipeline stages. Pump a `BufferedReader` into a Consumer with [Bridge](#bridge) first if a stage must see file bytes.

### IO::BufferedReader

Public base for a **binary** read origin (file leaf, or any device a derived class can pull). It owns the session, optional prefetch, the cache map, `Read` / `Peek` / `Seek` / `Tell` / `Size` and `MaxWait`. Construction is `State::Unavailable`. `Open` arms it; `Close` is idempotent; `Open` is not.

A leaf does **not** override `Read`, `Peek`, `Seek`, `Open`, `Close` or `Rewind`. It implements only:

- `OriginOpen` / `OriginClose` — arm and release the device; call `SetState`.
- `OriginPull(n, dest)` — read up to `n` raw bytes. Do not cache inside the hook.
- `OriginCanSeek` / `OriginSeek` — or report not seekable.
- `OriginHasSize` / `OriginSize` — or report unknown length.

`IsSeekable()` is `OriginCanSeek`. When that is true, `Seek(offset, Position::Absolute)` / `Seek(offset, Position::Relative)` moves `Tell` and **always** calls `OriginSeek` for the resolved target, including a cache hit, so the next pull is not silent corruption. Cached spans stay; a hit only avoids re-pulling those bytes. When `IsSeekable()` is false, `Seek` returns `Failed` and does not call the hook.

There is no `Whence`. End-relative positioning is `Seek(*Size() + off, Position::Absolute)` when `IsSized()`. Negative absolute offsets and relative steps before 0 fail. Seek without `Open` fails.

Seek is not O(1) and is not guaranteed to return immediately (prefetch cancel, device seek, cache bookkeeping).

Seekable origins keep a map of owned spans `[offset, offset+len)`. Overlap and abutment merge, even past `ReadAhead`. `MaxMemory` evicts the spans farthest from `Tell`. `MaxMemory` 0 stores no cache and still serves `Read` / `Peek` from the origin. A non-seekable origin keeps one forward span.

Pass `const BufferedReader&` into a [Bridge](#bridge) when the pump should drain the session sequentially. Random access stays on the reader: call `Seek` / `Tell` / `Size` yourself. Do not put a Bridge in front of a source that must seek.

### IO::BufferedWriter

Public base for a **binary** write sink. It owns the session, optional SPSC write-behind (`WriteChunk` / `BackPressure`), `Write`, `Flush`, `Truncate` and `Tell`. Either knob `0` is direct (blocking) write. Both `> 0` buffer until a full chunk or `Flush`. `Write` is atomic: the whole payload is accepted or nothing is (`TryAgain` if it would exceed backpressure).

A leaf does **not** override `Write`, `Flush`, `Open`, `Close`, `Rewind` or `Truncate`. It implements only:

- `OriginOpen` / `OriginClose` — arm and release; call `SetState`.
- `OriginPush(span)` — write those bytes. Do not buffer in the hook.
- `OriginFlush` — make accepted bytes visible on the device.
- `OriginTruncate` — drop destination contents.

No `Seek` on the writer. `Tell` is bytes accepted since `Open` or `Truncate`.

The destructor of a leaf must call `Close` while its vtable is live.

A live writer is a Bridge sink. See [Bridge](#bridge).

### IO::BufferedFileReader

Filesystem leaf over `BufferedReader`. Binary `ifstream`. Does not open in the constructor. `OriginCanSeek` is true and `OriginHasSize` is set after a successful `Open` on a regular file. `Seek` stays on the base; this class only implements `OriginSeek` (`seekg`).

```cpp
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/typedefs.hxx>
#include <iostream>

using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::ToString;

int main() {
	BufferedFileReader in("payload.bin", /*read_ahead*/ 4096, /*max_memory*/ 1 << 20);
	if (!in.Open()) {
		std::cerr << ToString(in.State()) << "\n";
		return 1;
	}

	FIFO dest;
	auto got = in.Read(16, dest);
	if (in.IsSeekable())
		(void)in.Seek(0, Position::Absolute);

	std::cout << ToString(got.status) << " " << got.count << "\n";
	in.Close();
}
```

Missing path is `State::Missing`. A directory is `State::Directory`. No read permission is `State::Permission`.

Use this leaf directly when the caller must `Seek`. Use it as a Bridge source when the caller only drains forward. See [IO::BufferedReader](#iobufferedreader) and [Bridge](#bridge).

### IO::BufferedFileWriter

Filesystem leaf over `BufferedWriter`. Binary append `ofstream`. Creates the file when the parent directory exists. Does not `mkdir -p`. Overwrite is `Truncate`, not an open flag.

```cpp
#include <StormByte/buffer/io/buffered_file_writer.hxx>
#include <span>
#include <cstddef>
#include <iostream>

using StormByte::Buffer::IO::BufferedFileWriter;
using StormByte::Buffer::IO::ToString;

int main() {
	BufferedFileWriter out("out.bin", /*write_chunk*/ 4096, /*back_pressure*/ 4);
	if (!out.Open()) {
		std::cerr << ToString(out.State()) << "\n";
		return 1;
	}

	const char raw[] = { 'A', 'B', 'C', 'D' };
	auto wr = out.Write(std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(raw), 4));
	out.Flush();
	out.Close();
}
```

Missing parent is `State::Missing`. Path is a directory → `State::Directory`. No write permission → `State::NotWritable`.

Typical sink for a sequential pump: [Producer](#producer-and-consumer) → [Bridge](#bridge) → this leaf.

### Bridge

`StormByte::Buffer::Bridge` pumps octets from a readable tip to a writable tip on a worker started in the constructor. It holds references only. The tips must outlive the Bridge. There is no public `Stop`; the destructor joins.

Pairings:

| Source | Sink |
| --- | --- |
| `ExternalReader` | `ExternalWriter` |
| `const IO::BufferedReader&` | `IO::BufferedWriter&` |
| `ExternalReader` | `IO::BufferedWriter&` |
| `const IO::BufferedReader&` | `ExternalWriter` |

`high_water` is the last constructor argument and the sink occupancy cap. `0` starts `Paused`. Setters do not pause or start the worker. `Drainer(Toggle)` flips Started / Paused. `Bridge::Flush` waits for the in-flight transaction and flushes an IO writer. `Drainer(Flush)` pushes what is already held and does not flush the writer.

| Path | Wiring |
| --- | --- |
| Sequential pump into a file | `Producer` → `Bridge(Consumer, BufferedFileWriter, high_water)` |
| Seekable, sized source | Leaf `BufferedReader` directly (`Read`, `Seek`, `Tell`, `Size`) — no `Bridge` |
| Non-seekable pipe | `IsSeekable() == false`. Optional `Bridge` from a buffer or file into a `Producer` |

```cpp
#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/producer.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>

using StormByte::Buffer::Bridge;
using StormByte::Buffer::Producer;
using StormByte::Buffer::IO::BufferedFileWriter;

int main() {
	Producer producer;
	auto consumer = producer.Consumer();
	BufferedFileWriter out("out.bin", 4096, 4);
	if (!out.Open())
		return 1;

	Bridge bridge(consumer, out, /*high_water*/ 1 << 20);
	(void)producer.Write("payload");
	producer.Close();
	(void)bridge.Flush();
	out.Close();
}
```

Moved-from is `Stopped`. See [Producer and Consumer](#producer-and-consumer), [IO::BufferedReader](#iobufferedreader) and [IO::BufferedFileWriter](#iobufferedfilewriter).

## Contributing

Issues only on this repository. Fork and open a pull request against `master`.

## License

GNU Lesser General Public License version 3 or later. See [LICENSE](LICENSE) and <https://www.gnu.org/licenses/lgpl-3.0.html>.

## Support

StormByte is developed in spare time. Sponsorship is optional and does not buy features, priority or support.

- [GitHub Sponsors](https://github.com/sponsors/StormBytePP)
- [PayPal](https://paypal.me/StormBytePP)
