# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte Buffer is the byte-buffer module of the StormByte C++ suite.

It depends on StormByte Base and optionally StormByte Logger. This repository is not Base, Config, Crypto, Database, Logger, Multimedia, Network or System.

Public headers under `StormByte/buffer/` cover FIFO, SharedFIFO, Ring, Producer/Consumer, Hopper, Sink, Bridge and Pipeline.

If you landed here from a release link and have not read the tree:

- What this module is, how to build it, and short examples: [README.md](https://github.com/StormBytePP/StormByte-Buffer/blob/master/README.md)
- License: GNU Lesser General Public License version 3 or later, [LICENSE](https://github.com/StormBytePP/StormByte-Buffer/blob/master/LICENSE)

## [Unreleased]

### Added

### Changed

### Fixed

### Removed

- `Sink::Bind` and `Sink::Bind(int, Sink&)`. Wire with `To(key)` / `>>` / `<<`.

[Unreleased]: https://github.com/StormBytePP/StormByte-Buffer/compare/1.3.0...HEAD

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
