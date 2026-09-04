# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte Buffer is the byte-buffer module of the StormByte C++ suite.

It depends on StormByte Base and optionally StormByte Logger. This repository is not Base, Config, Crypto, Database, Logger, Multimedia, Network or System.

Public headers under `StormByte/buffer/` cover FIFO, SharedFIFO, Ring, Producer/Consumer, Bridge and Pipeline.

If you landed here from a release link and have not read the tree:

- What this module is, how to build it, and short examples: [README.md](https://github.com/StormBytePP/StormByte-Buffer/blob/master/README.md)
- License: GNU Lesser General Public License version 3 or later, [LICENSE](https://github.com/StormBytePP/StormByte-Buffer/blob/master/LICENSE)

## [1.0.0] - 2026-09-04

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
