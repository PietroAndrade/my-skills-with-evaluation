# compute::parallelSum - bounded fork-join sum

## Files

- `include/compute/parallel_sum.hpp` - public API (`parallelSum`, `sequentialSum`, `ParallelSumConfig`)
- `src/parallel_sum.cpp` - implementation
- `src/main.cpp` - demo / sequential-vs-parallel check
- `CMakeLists.txt` - builds `compute` static library + `compute_demo`

Build and run:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/compute_demo 2000000000
```

## Mechanism

`parallelSum` recurses over a raw `(pointer, count)` range instead of iterators, so nothing is
copied and each recursive call is a plain halving of the range.

At each level the range is split in two. The left half is handed to
`std::async(std::launch::async, ...)` and the right half is computed on the calling thread by
direct recursion. The caller then blocks on `future::get()` for the left half and adds the two
partial sums. This is the standard fork-join shape: one fork per level, and the forking thread
never idles while its child runs.

`std::launch::async` is passed explicitly. Without it the implementation is free to pick
`deferred`, in which case the left half would only run inside `get()` - correct, but completely
serial.

## How the thread count is bounded

Two independent limits, both in `ParallelSumConfig`.

### 1. `maxSplitDepth` - the hard bound

`forkJoinSum` carries a `remainingDepth` counter that is decremented on every split. When it
reaches zero the range is summed sequentially with `std::accumulate`, no matter how large it
still is. Because exactly one thread is spawned per split and the recursion tree is binary, the
number of live threads is bounded by

```
threads <= 2^maxSplitDepth
```

independently of the input size. This is the key property: the recursion depth on several
billion elements is driven by the data, but the *spawning* depth is not.

`makeDefaultParallelSumConfig()` sets `maxSplitDepth = ceil(log2(hardware_concurrency))`, so the
tree has roughly one leaf per core. On a 20-core machine that is depth 5, i.e. at most 32
concurrent workers - never one thread per recursive call, which on 2e9 elements with an
unbounded recursion would be millions of threads and an immediate
`resource_unavailable_try_again`.

### 2. `sequentialCutoff` - the granularity floor

A second stop condition ends the splitting once a subrange is at or below `sequentialCutoff`
(default 65536 elements). Its job is not safety but efficiency: a thread handoff costs on the
order of microseconds, so splitting ranges that take less than that is pure loss. On the
several-billion-element case the depth limit is what fires; the cutoff matters for small or
medium inputs, where it degrades the call gracefully to a plain sequential sum rather than
paying for threads that cannot amortise themselves.

### Fallback

If `std::async` still fails to get an OS thread it throws `std::system_error`. That is caught at
the split site and the whole subrange is summed on the current thread, so the result is always
produced - the parallelism degrades, the correctness does not.

## Correctness notes

- Summation is over disjoint subranges and the two partials are combined by a single thread, so
  there is no shared mutable state and no synchronisation beyond the futures. No mutex or atomic
  is needed.
- The vector is taken by `const&` and only read, so the child threads observe it safely; the
  `future::get()` join also provides the happens-before edge for each partial result.
- `std::int64_t` accumulation of several billion values can overflow; that is inherent to the
  requested type and behaves identically in the sequential and parallel versions (integer
  addition regroups exactly under two's-complement wraparound, so the parallel regrouping gives
  the same answer as the sequential loop - unlike floating point, where grouping would change
  the result).

## Measured

100,000,000 elements, 20 cores, depth 5: sequential 34.3 ms, parallel 13.3 ms, identical totals.
The workload is memory-bandwidth bound, so the speedup saturates well below the core count -
expected for a sum.
