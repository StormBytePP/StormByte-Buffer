# StormByte-Buffer

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)
[![CI](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml/badge.svg)](https://github.com/StormBytePP/StormByte-Buffer/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Buffer**: FIFO, SharedFIFO, Ring, Producer/Consumer, Hopper, Sink, pipelines and buffered I/O for the StormByte C++ suite.

It depends on [StormByte Base 1.2.0](https://github.com/StormBytePP/StormByte/releases/tag/1.2.0) or newer and optionally [StormByte Logger 1.2.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.2.0) or newer for pipeline stages (`Scope`). Public headers live under `StormByte/buffer/`.

The suite is split on purpose. Base, Config, Crypto, Database, Logger, Multimedia, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **FIFO** — grow-on-demand byte buffer. Not thread-safe. `Read` / `Peek` keep data; `Extract` consumes it.
- **SharedFIFO** — thread-safe FIFO. `Read` / `Extract` block until data or `Close` / `SetError`.
- **Ring** — concurrent ring (`shared_mutex`, many-to-many).
- **Producer / Consumer** — write-only / read-only handles over a shared `Ring`.
- **Hopper** — single-producer single-consumer (SPSC) queue of typed items with optional capacity ceiling. `Push` / `Pop` stay; `<<` / `>>` are the same operations. `Notify(cv)` does not own the CV; call `Unnotify` before that CV dies.
- **Sink** — map of integer keys to Hopper buckets. Wire with `To(key)` / `>>` / `<<`. Round-robin or custom `Select`, plus terminal producer `Drain`.
- **Pipeline** — stages chained with `ExecutionMode`: `Sync`, `Async`, `Parallel` (combinable). A non-null logger is scoped as `Buffer/Pipeline` before it reaches the stages.
- **IO::BufferedReader** — public base for a binary read origin with optional prefetch. Leaves implement `Origin*` hooks only; they do not override `Read` / `Peek` / `Seek`.
- **IO::BufferedWriter** — public base for a binary write sink with optional write-behind chunks. Leaves implement `Origin*` hooks only; they do not override `Write` / `Flush`.
- **IO::BufferedFileReader** / **IO::BufferedFileWriter** — filesystem leaves over those bases (binary `ifstream` / append `ofstream`).
- **Bridge** — move octets from any readable tip to any writable tip: in-memory buffers (`ExternalReader` / `ExternalWriter`) and/or IO sessions (`IO::BufferedReader` / `IO::BufferedWriter`), in any pairing. Not a typed Hopper pump.
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

`Hopper<T>` is an SPSC **item** queue, not a byte buffer. Use it for `shared_ptr<Item>` and similar. It is not a Bridge tip.

```cpp
#include <StormByte/buffer/hopper.hxx>
#include <memory>

using StormByte::Buffer::Hopper;

int main() {
	Hopper<std::shared_ptr<int>> hopper(8);
	hopper << std::make_shared<int>(1);
	auto item = hopper.Pop();
	hopper.Eof();
}
```

`Notify(cv)` does not own `cv`. Call `Unnotify` before that condition variable is destroyed.

### Sink

`Sink<T>` is a map of integer keys to `Hopper<T>` buckets. Wire with `To(key)` / `>>` / `<<`. It is not a Bridge tip.

```cpp
#include <StormByte/buffer/sink.hxx>
#include <memory>

using StormByte::Buffer::Sink;
using StormByte::Buffer::Hopper;

int main() {
	Sink<std::shared_ptr<int>> sink;
	Hopper<std::shared_ptr<int>> hopper;
	sink.To(0) << hopper;
	sink << std::make_shared<int>(7);
}
```

### Pipeline

Stages receive `ExternalReader&`, `ExternalWriter&` and an optional `std::shared_ptr<Logger::Log>`. They must `out.Close()` or `out.SetError()`.

When `Process` gets a non-null logger it passes `log->Scope("Buffer/Pipeline")` to every stage. `%c` is then `Buffer/Pipeline`, or `Multimedia/Buffer/Pipeline` if the caller already scoped a parent. Do not pre-scope `Buffer/Pipeline` on the argument. A stage that needs a leaf can `log->Scope("Decode")`.

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

### IO::BufferedReader

`StormByte::Buffer::IO::BufferedReader` is the public base for a **binary** read origin (file, socket, prefilled FIFO, anything a leaf can pull). It owns the session, the optional prefetch window, `Read` / `Peek` / `Seek` / `Tell` and `MaxWait`. Construction is `State::Unavailable`. `Open` arms it; `Close` is idempotent; `Open` is not.

A leaf does **not** override `Read`, `Peek`, `Seek`, `Open`, `Close` or `Rewind`. It implements only:

- `OriginOpen` / `OriginClose` — arm and release the device; call `SetState`.
- `OriginPull(n, dest)` — read up to `n` raw bytes. Do not cache inside the hook.
- `OriginCanSeek` / `OriginSeek` — or report not seekable.
- `OriginHasSize` / `OriginSize` — or report unknown length.

Network or Multimedia can inherit this class and pass `const BufferedReader&` into a file/source API.

### IO::BufferedWriter

`StormByte::Buffer::IO::BufferedWriter` is the public base for a **binary** write sink. It owns the session, optional SPSC write-behind (`WriteChunk` / `BackPressure`), `Write`, `Flush`, `Truncate` and `Tell`. Either knob `0` is direct (blocking) write. Both `> 0` buffer until a full chunk or `Flush`. `Write` is atomic: the whole payload is accepted or nothing is (`TryAgain` if it would exceed backpressure).

A leaf does **not** override `Write`, `Flush`, `Open`, `Close`, `Rewind` or `Truncate`. It implements only:

- `OriginOpen` / `OriginClose` — arm and release; call `SetState`.
- `OriginPush(span)` — write those bytes. Do not buffer in the hook.
- `OriginFlush` — make accepted bytes visible on the device (file: stream flush; network: no-op `Ok`).
- `OriginTruncate` — drop destination contents (file: resize 0; network may no-op `Ok`).

The destructor of a leaf must call `Close` while its vtable is live.

### IO::BufferedFileReader

Filesystem leaf over `BufferedReader`. Binary `ifstream`. Does not open in the constructor. Seekable and sized when the path is a regular file.

```cpp
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <iostream>

using StormByte::Buffer::FIFO;
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
	std::cout << ToString(got.status) << " " << got.count << "\n";
	in.Close();
}
```

Missing path is `State::Missing`. A directory is `State::Directory`. No read permission is `State::Permission`.

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

### Bridge

`StormByte::Buffer::Bridge` moves **octets** from a readable tip to a writable tip. It is not tied to one buffer class.

**Sources (in)** — anything that can yield bytes:

- `ExternalReader` (and leaves such as `ExternalBufferReader` over a `FIFO` / `Consumer`)
- `const IO::BufferedReader&` (and leaves such as `IO::BufferedFileReader`, or a future Network/Multimedia reader)

**Sinks (out)** — anything that can take bytes:

- `ExternalWriter` (and leaves such as `ExternalBufferWriter`)
- `IO::BufferedWriter&` (and leaves such as `IO::BufferedFileWriter`, or a future Network writer)

A reader never goes on `out`. A writer never goes on `in`. The four legal pairings are:

| in | out |
| --- | --- |
| `ExternalReader` | `ExternalWriter` |
| `IO::BufferedReader` | `IO::BufferedWriter` |
| `ExternalReader` | `IO::BufferedWriter` |
| `IO::BufferedReader` | `ExternalWriter` |

`Hopper` / `Sink` are item queues. They are not Bridge tips. A Multimedia remux of raw bytes is a Bridge; a graph of `shared_ptr<Item>` stays on Hopper.

The Bridge holds **references only**. Tips must already be armed (`Open` on IO) and must outlive every `Passthrough`. There is no configured chunk and no local cache: `Passthrough(n)` is the unit. `n == 0` moves whatever is available on the source **now** (IO without prefetch: the current window, which may be empty). External sources are consumed with `Extract`. IO sources use `Read`, which already consumes.

`Passthrough` blocks and is transactional: `IsWritable` / `WillWrite` is checked before the source is pulled. If the sink cannot take the request, the source is left untouched. A short read at end-of-origin is success and is not `Failed`. IO `EoF` is marked when a read returns `End` (asking past the last byte), not necessarily after an exact-size read.

`Flush()` is a no-op on an External sink and `BufferedWriter::Flush` on an IO sink. `FlushAndClose()` closes only an External writer. `SetError()` is External only. The destructor calls `Flush`. The type is movable, not copyable; `Passthrough` on a moved-from instance is a no-op.

```cpp
#include <StormByte/buffer/bridge.hxx>
#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/buffer/io/buffered_file_reader.hxx>
#include <StormByte/buffer/io/buffered_file_writer.hxx>

using StormByte::Buffer::Bridge;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::IO::BufferedFileReader;
using StormByte::Buffer::IO::BufferedFileWriter;

void buffer_to_buffer(FIFO& src, FIFO& dst) {
	ExternalBufferReader in(src);
	ExternalBufferWriter out(dst);
	Bridge bridge(in, out);
	(void)bridge.Passthrough(0); // everything available on src now
}

void file_to_file() {
	BufferedFileReader in("in.bin");
	BufferedFileWriter out("out.bin");
	if (!in.Open() || !out.Open())
		return;
	Bridge bridge(in, out);
	while (!in.EoF() && static_cast<bool>(in))
		(void)bridge.Passthrough(4096);
	(void)bridge.Flush();
}

void fifo_to_file(FIFO& src) {
	ExternalBufferReader in(src);
	BufferedFileWriter out("out.bin");
	if (!out.Open())
		return;
	Bridge bridge(in, out);
	(void)bridge.Passthrough(src.AvailableBytes());
	(void)bridge.Flush();
}

void file_to_fifo(FIFO& dst) {
	BufferedFileReader in("in.bin");
	ExternalBufferWriter out(dst);
	if (!in.Open())
		return;
	Bridge bridge(in, out);
	(void)bridge.Passthrough(64);
}
```

A Network `BufferedReader` or `BufferedWriter` leaf drops into the same constructors without changing Bridge.

## Contributing

Issues only on this repository. Fork and open a pull request against `master`.

## License

GNU Lesser General Public License version 3 or later. See [LICENSE](LICENSE) and <https://www.gnu.org/licenses/lgpl-3.0.html>.

## Support

StormByte is developed in spare time. Sponsorship is optional and does not buy features, priority or support.

- [GitHub Sponsors](https://github.com/sponsors/StormBytePP)
- [PayPal](https://paypal.me/StormBytePP)
