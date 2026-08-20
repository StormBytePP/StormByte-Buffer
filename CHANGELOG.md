# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-08-20

Initial public release of **StormByte-Buffer**: a C++23 byte-buffer library with FIFO, thread-safe rings, producer/consumer handles, external I/O adapters and multi-stage pipelines.

### Added

- Hierarchical buffer interfaces: `Generic`, `ReadOnly`, `WriteOnly`, `ReadWrite`
- `FIFO` – contiguous grow-on-demand byte buffer (single-threaded)
- `SharedFIFO` – thread-safe FIFO with blocking reads/extracts
- `Ring` – highly concurrent ring buffer (`shared_mutex`, many-to-many)
- `Producer` / `Consumer` – shared write/read handles over `Ring`
- `ExternalReader` / `ExternalWriter` – abstract I/O adapters (plus buffer-backed implementations)
- `Bridge` – chunked passthrough from reader to writer with optional flush-on-destroy
- `Pipeline` – multi-stage processing with `ExecutionMode` flags:
  - `Sync` – sequential on caller thread
  - `Async` – background worker(s), non-blocking return
  - `Parallel` – one thread per stage (SPSC intermediates)
  - Combinable (`Async | Parallel`)
- Private `LockFreeRing` – SPSC lock-free ring used between pipeline stages
- Lifecycle signalling: `Close()`, `SetError()`, `EoF()`, `IsReadable()`, `IsWritable()`
- Non-destructive `Read` / `Peek` and destructive `Extract`, plus `*UntilEoF` variants
- Seek (absolute / relative), Drop, Clean, Clear
- HexDump diagnostics
- Integration with StormByte Base and optional Logger in pipeline stages
- Comprehensive unit tests (FIFO, SharedFIFO, Ring, Producer/Consumer, Bridge, Pipeline)

### Fixed

- `Ring::operator==` now compares closed and error flags (lifecycle state)
- Corrected `FIFO::HexDump` parameter spelling (`columns`)
- Deleted invalid `SharedFIFO` move constructor/assignment (mutex is not movable)

### Notes

- `FIFO` is not thread-safe; use `SharedFIFO` or `Ring` for concurrent access.
- `LockFreeRing` is private and only safe under single-producer / single-consumer use (as in `Pipeline`).
- Pipeline stages should always call `out.Close()` or `out.SetError()` when finished.
- Requires a C++23 compliant compiler and StormByte Base ≥ 1.0.0.

[1.0.0]: https://github.com/StormBytePP/StormByte-Buffer/releases/tag/1.0.0
