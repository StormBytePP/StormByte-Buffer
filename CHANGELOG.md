# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte Buffer is the byte-buffer module of the StormByte C++ suite.

It depends on StormByte Base and optionally StormByte Logger. This repository is not Base, Config, Crypto, Database, Logger, Multimedia, Network or System.

Public headers under `StormByte/buffer/` cover FIFO, SharedFIFO, Ring, Producer/Consumer, Hopper, Sink, Bridge, Pipeline and `StormByte::Buffer::IO` (buffered binary sources and sinks).

If you landed here from a release link and have not read the tree:

- What this module is, how to build it, and short examples: [README.md](https://github.com/StormBytePP/StormByte-Buffer/blob/master/README.md)
- License: GNU Lesser General Public License version 3 or later, [LICENSE](https://github.com/StormBytePP/StormByte-Buffer/blob/master/LICENSE)

## [Unreleased]

### Added

### Changed

### Fixed

### Removed

[Unreleased]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.4.0...HEAD

## [1.4.0] - 2026-09-22

### Added

- `StormByte::Buffer::IO`. Buffered binary sources and sinks, separate from FIFO / Ring / Hopper. Public surface: `Status`, `State`, `Result`, `ToString`, `BufferedReader`, `BufferedWriter`, `BufferedFileReader`, `BufferedFileWriter`. `IO::Backend` is the PIMPL and is not a public include.
- `Status` / `Result` / `State`. `Ok`, `End`, `Error`, `Failed`, `TryAgain` plus a byte `count`. `TryAgain` is backpressure or a bounded `MaxWait`. `State`: `Idle`, `Missing`, `Directory`, `Permission`, `NotWritable`, `Fault`, `Unavailable`. `constexpr ToString` for `Status` and `State`.
- `BufferedReader`. Public base for a binary origin. Leaves implement `OriginOpen`, `OriginClose`, `OriginPull`, `OriginCanSeek`, `OriginSeek`, `OriginHasSize` and `OriginSize`. Optional `Setup()` runs once from `Open` before `OriginOpen`. Construction is `Unavailable`; a successful `Open` is `Idle`. `Close` is idempotent; `Open` is not. `operator bool` is Idle and not `EoF`.
- Reader `Read` / `Peek` into a `FIFO` or a writable `std::span<std::byte>`. Destination overwritten only on `Ok` / `End` with a non-zero count. Empty span is `{Ok, 0}`. FIFO `n == 0` serves the cached span at `Tell`. `MaxWait` `0ms` waits without limit.
- Reader `Seek` / `Tell` / `IsSeekable` / `IsSized` / `Size`. Absolute or relative only. End-relative is `Seek(*Size() + off, Absolute)` when sized. Seekable `Seek` always calls `OriginSeek`, including a cache hit. Non-seekable `Seek` is `Failed` and does not call the hook. Seek is not O(1). Cache is a map of owned spans; overlap merges; `MaxMemory` evicts farthest from `Tell`; `0` stores nothing and still serves from the origin. Prefetch stops on `Seek` and on move (`Rebind`); the next `Read` / `Peek` requests it again.
- `BufferedFileReader`. `ifstream` leaf, seekable and sized. Path-only constructor probes device throughput at `Setup` and sets `ReadAhead` (window clamped 16 KiB–1 MiB) with `MaxMemory` 1 MiB. Explicit `(path, read_ahead, max_memory)` keeps those knobs. Does not open in the constructor.
- `BufferedWriter`. Public base for a binary sink. Leaves implement `OriginOpen`, `OriginClose`, `OriginPush`, `OriginFlush` and `OriginTruncate`. Optional `Setup()` and `WillWrite`. No `Seek`. `Tell` is bytes accepted since `Open` or `Truncate`. `Close` flushes then closes; a flush failure is `Fault`.
- Writer `Write(const FIFO&)`, `Write(FIFO&)` and `Write(std::span<const std::byte>)`. Atomic. `WriteChunk` and `BackPressure` (in chunks): either knob `0` is direct; both `> 0` use an SPSC `LockFreeRing` capped at `BackPressure * WriteChunk` bytes. Overflow is `TryAgain`. `Dirty()` is unread ring bytes. `Flush()` drains the ring and calls `OriginFlush`.
- `BufferedFileWriter`. `ofstream` leaf, binary append. Creates the file when the parent exists (no `mkdir -p`). Path-only constructor probes the device at `Setup` and sets `WriteChunk` plus `BackPressure` 4. Explicit `(path, write_chunk, backpressure)` keeps those knobs. `Truncate` overwrites.
- Device throughput probe (private): Linux / Windows / macOS classification (HDD, SATA SSD, NVMe gen, USB, network at 80 % of NIC). Nominal rates, not a benchmark. Device knobs have no setters; `MaxMemory` and `MaxWait` stay settable.
- `LockFreeRing::FrontSpan`, `Consume` and `Write(std::span<const std::byte>)`.
- `ExternalWriter::Occupied`.
- `Bridge` pumps any `ExternalReader` / `IO` reader into any `ExternalWriter` / `IO` writer. `Drain` respects sink backpressure. Worker auto-drains; public `Passthrough` is gone. `high_water == 0` means no extra occupancy cap.

### Removed

- `Sink::Bind` and `Sink::Bind(int, Sink&)`. Wire with `To(key)` / `>>` / `<<`.

### Tests

- `BufferedFileReaderTests`. Fixtures under `test/files/`. Span `Read` / `Peek`, `Tell`, `Seek` (absolute, relative, end via `Size`, cache hit, `MaxMemory` 0), path-only vs explicit constructors, move with prefetch stopped.
- `BufferedFileWriterTests`. Temp files via `StormByte::System::TempFileName`. Direct `(path, 0, 0)`, path-only device knobs, Dirty / Flush / BackPressure / Truncate / move.
- `BufferedMeteredFileTests`. Selective override example (`BytesRead` / `BytesWritten`).
- Bridge coverage for pipe close-while-started and `high_water` 0.

[1.4.0]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.3.0...1.4.0

## [1.3.0] - 2026-09-20

### Added

- `Sink::Keys`, `Sink::Buckets`, `Sink::Contains`, `Sink::Empty(key)`, `Sink::EoF(key)` and `Sink::Ready(key)`. Query only. Missing key: `Empty` is true, `EoF`/`Ready`/`Contains` are false, `Buckets` is the wired count.
- `Sink::Pop(int key)`. Reads that hopper only. Waits until the key is wired or the Sink is closed. Empty hopper returns default `T` (same as `Hopper::Pop`). Does not interpret the key.
- `Hopper::Writers`, `Hopper::Ready` and `Hopper::Front`. `Front` copies the next item and does not dequeue; requires `std::copy_constructible<T>` (`shared_ptr`). Not a deep copy of the payload. `Writers` is the live writer count (starts at 1).

### Tests

- `test_hopper_front_peek`, `test_hopper_writers_and_ready`.
- `test_sink_pop_key`, `test_sink_query_unwired`, `test_sink_query_wired`.
- Hopper and Sink test files ordered by section name, then by test name.

[1.3.0]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.2.0...1.3.0

## [1.2.0] - 2026-09-17

### Added

- `Hopper::Unnotify` and `Sink::Unnotify`. `Notify(cv&)` does not own the
  condition variable. After wiring, the Hopper outlives the consumer; the
  consumer must `Unnotify` before that CV is destroyed so a later producer
  `Eof` does not signal a freed object. `SignalConsumer` is a no-op when
  the pointer is null.
- `Sink::To(key)`, `Sink::operator>>` and `Sink::operator<<`. Same wiring
  as `Bind` / `Bind(key)` (writer, reader, co-writer). `Bind` stays as a
  `[[deprecated]]` wrapper for one or two releases.
- `Hopper::operator<<` / `Hopper::operator>>` and `item >> hopper`. Same
  as `Push` / `Pop`. Those methods stay.
- `Pipeline::Process` scopes a non-null logger with `Scope("Buffer/Pipeline")`
  before handing it to stages. `%c` identifies this module without using the
  thread-local component stack. Nested `log->Scope("Decode")` inside a stage
  becomes `Buffer/Pipeline/Decode` (or `Multimedia/Buffer/Pipeline/Decode`
  if the caller already scoped a parent). Pass the application or parent-module
  logger; do not pre-scope `Buffer/Pipeline`.

### Changed

- Optional Logger pin is [1.2.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.2.0)
  (`Scope` and hierarchical components).

### Tests

- `test_hopper_unnotify_before_cv_dies`, `test_hopper_notify_after_unnotify`,
  `test_hopper_stream_members`, `test_hopper_stream_item_into`.
- `test_sink_unnotify_before_cv_dies`, `test_sink_stream_operators`.
  Existing Sink tests use `To` / `>>` / `<<` instead of `Bind`.

[1.2.0]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.1.2...1.2.0

## [1.1.2] - 2026-09-16

### Deprecated

- `Sink::Bind` (both overloads). See Unreleased TODO.

### Fixed

- `Sink::Bind(key)` of a hopper this Sink already holds attaches the other Sink as a co-writer. `Sink::Eof` then closes that hopper only when the last writer closes, so extra producers can still `Push`. No new public methods.
- `Sink::Bind(key)` does not add a writer if the other Sink already writes that hopper. A second Bind of the same producer no longer leaves the hopper open after one `Eof`. `test_sink_rebind_same_writer_eof` waits up to 1s for that EoF (the remuxer hang).

[1.1.2]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.1.1...1.1.2

## [1.1.1] - 2026-09-15

### Changed

- Bundled StormByte Logger is [1.1.1](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.1.1) (and transitively [StormByte Base 1.1.1](https://github.com/StormBytePP/StormByte/releases/tag/1.1.1)). The declared requirement stays Logger [1.1.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.1.0) or newer.

[1.1.1]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.1.0...1.1.1

## [1.1.0] - 2026-09-13

### Added

- `Hopper<T>` — single-producer single-consumer (SPSC) typed queue (`Type::MoveConstructible`) with optional capacity ceiling, non-blocking `Pop`, `Push` wait when full, `Eof` signaling, consumer condition variable notification (`Notify`), and automatic null item discarding for `Type::SmartPointer` types.
- `Sink<T>` — integer key map of `Hopper<T>` buckets supporting `Bind` hopper sharing, terminal producer `Drain` mode, `Ready` check, Round-Robin or custom `Select` index chooser popping, and per-key `Capacity`, `Size`, and `Full` queries.
- Header implementation files `hopper.txx` and `sink.txx` installed alongside public headers (`*.txx` in `cmake/install.cmake`).

### Changed

- Updated dependency requirement to [StormByte Logger 1.1.0](https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.1.0) (and transitively [StormByte Base 1.1.0](https://github.com/StormBytePP/StormByte/releases/tag/1.1.0)).
- Routed range and iterator byte APIs through StormByte Base type concepts (`Type::ByteInputRange`, `Type::ByteInputIterator`, `Type::SentinelFor`, `Type::SameAs`) instead of local standard-library constraints.
- Limited `Hopper<T>` and `Sink<T>` null-item filtering to `Type::NullablePointer` so only pointer-like values that can be tested for emptiness use the `!item` path.
- Documented `Sink::EoF()` contract: with hoppers, evaluates `true` when all hoppers are empty and `Hopper::EoF()` is `true`, even if this `Sink` itself did not call `Eof()`; binding a new key after `EoF()` returned `true` may return `EoF()` to `false`.

### Fixed

- Ported Buffer exceptions to `StormByte::Component`, preventing format-string overload ambiguity and correctly prefixing `ReadError` and `WriteError` messages.
- Moved virtual override definitions and destructors to compiled translation units so shared-library builds export stable vtables, RTTI and non-virtual thunks for GCC consumers, with polymorphic consumer coverage in each owning class test.
- Closed `Sink` and marked hoppers `Eof` atomically under `m_mutex` in `Sink::Eof` and checked closed state and `consumer.m_consumer` under lock in `Sink::Bind` to prevent concurrent `Bind` calls from creating un-marked hoppers or skipping consumer `Notify`.
- Marked `Sink` closed in destructor to safely unblock threads waiting in `Push` and `Pop`.

[1.1.0]: https://github.com/StormBytePP/StormByte-Buffer/releases/tag/1.0.0..1.1.0

## [1.0.0] - 2026-09-05

Initial public release of StormByte Buffer.

### Added

- Hierarchical interfaces: `Generic`, `ReadOnly`, `WriteOnly`, `ReadWrite`
- `FIFO` — grow-on-demand byte buffer (single-threaded)
- `SharedFIFO` — thread-safe FIFO with blocking reads/extracts
- `Ring` — concurrent ring (`shared_mutex`, many-to-many)
- `Producer` / `Consumer` — write/read handles over `Ring`
- `ExternalReader` / `ExternalWriter` — I/O adapters
- `Bridge` — chunked passthrough with optional flush-on-destroy
- `Pipeline` with `ExecutionMode`: `Sync`, `Async`, `Parallel` (combinable)
- Private `LockFreeRing` — SPSC intermediates between pipeline stages
- Lifecycle: `Close()`, `SetError()`, `EoF()`, `IsReadable()`, `IsWritable()`
- Non-destructive `Read` / `Peek` and destructive `Extract`, plus `*UntilEoF`
- Seek, Drop, Clean, Clear, HexDump
- Unit tests (FIFO, SharedFIFO, Ring, Producer/Consumer, Bridge, Pipeline)
- Project version read from the `VERSION` file
- CMake 3.28 floor

### Notes

- `FIFO` is not thread-safe; use `SharedFIFO` or `Ring` for concurrent access.
- `LockFreeRing` is private and only safe under single-producer / single-consumer use.
- Pipeline stages must `out.Close()` or `out.SetError()` when finished.
- Needs a C++26 compiler and StormByte Base ≥ 1.0.0.

[1.0.0]: https://github.com/StormBytePP/StormByte-Buffer/releases/tag/1.0.0
