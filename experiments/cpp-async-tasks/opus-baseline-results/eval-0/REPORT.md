# Parallel download of 200 URLs — mechanism and rationale

## Files

- `thread_pool.hpp` — fixed-size worker pool; `submit()` returns a `std::future<T>` via `std::packaged_task`.
- `downloader.hpp` — stand-in for the real HTTP client: sleeps 50–500 ms, returns a `DownloadResult` (url, bytes, success flag, error). Replace `SimulatedDownloader::fetch` with your libcurl/socket call; nothing else changes.
- `main.cpp` — builds 200 URLs, fans them out onto the pool, gathers the futures, sums the bytes.
- `CMakeLists.txt` — C++17 + `Threads::Threads`.

Build and run:

```
g++ -std=c++17 -O2 -pthread main.cpp -o parallel_downloads && ./parallel_downloads
# or: cmake -S . -B build && cmake --build build && ./build/parallel_downloads
```

Observed on a 20-core box: 200 downloads, 80 workers, ~0.9 s wall clock against ~55 s if run sequentially.

## Mechanism chosen: bounded thread pool + futures

The work is **I/O-bound and blocking**: each task spends nearly all its time waiting on a socket, not on a CPU. Two consequences drive the design.

1. **Thread count should exceed core count.** For CPU-bound work you size the pool at `hardware_concurrency()`. Here a blocked thread costs no CPU, so oversubscribing is what buys parallelism. `chooseThreadCount()` uses `max(32, hardware_concurrency() * 4)`, capped at the number of tasks.
2. **Thread count must still be bounded.** 200 concurrent connections may trip server rate limits, exhaust file descriptors, or just waste ~8 MB of stack each. A pool gives one knob (`kMaxConcurrentDownloads`) that caps in-flight requests independently of how many URLs there are.

Results come back as `std::future<DownloadResult>`, so the accumulation loop in `main` is single-threaded: **no mutex and no atomic on the byte counter**, and the total is identical on every run. The future also transports any exception thrown inside a worker to the `future.get()` call site, which is why a `catch` there is enough for error handling.

## Alternatives considered

| Option | Why not |
|---|---|
| `std::async(std::launch::async, ...)` × 200 | Simplest code, and correct — but it gives up the concurrency cap: implementations typically spawn a thread per call, so all 200 connections open at once. Also, a `std::future` from `async` blocks in its destructor, so a loop that creates and drops futures silently serializes. Use it for a handful of tasks, not 200. |
| 200 raw `std::thread`s + atomic counter | Same unbounded-concurrency problem, plus manual joining and shared-state synchronization for something futures give for free. |
| `std::for_each(std::execution::par, ...)` | Not in C++17 in practice (libstdc++ needs TBB), and parallel algorithms size their thread count for CPU-bound work — the pool would sit idle waiting on sockets. |
| Single-threaded async I/O (`epoll`, `io_uring`, `curl_multi`) | Technically the best fit for pure I/O: thousands of in-flight requests, no thread stacks. Rejected here because it requires a non-blocking client and an event loop, whereas the pool works with any ordinary blocking download function. Worth revisiting above a few thousand concurrent requests. |
| Coroutines | C++20. |

## Notes on adapting it

- `kMaxConcurrentDownloads` is the politeness/throughput knob; tune it against the target server, not the CPU.
- Failures are counted, not fatal — a single bad URL must not abort the other 199.
- Shutdown drains the queue: `~ThreadPool` (via `shutdown()`) lets workers finish already-queued tasks before joining, so no future is left unfulfilled.
- If the real client is libcurl, initialize it once with `curl_global_init` before creating the pool and give each worker its own `CURL*` handle — handles are not thread-safe.
