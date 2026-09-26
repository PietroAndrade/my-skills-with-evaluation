---
name: cpp-async-tasks
description: Generate task-based parallel C++ — std::async/std::future to run work off-thread and collect results, std::promise/std::packaged_task to hand a result across threads, fan-out/gather over a collection, depth-limited fork-join for divide-and-conquer, and a C++17 thread pool for many short tasks. Use whenever the user wants to run work in parallel and get results back rather than manage locks: "run this in the background and get the result later", "download/process these N items in parallel", "parallelize this recursive sum/sort", "use std::async here", "return a future", "set up a thread pool", "fan out these tasks and collect the results", "make this computation use all my cores". For lock-level synchronization instead, see cpp-synchronization; to decide if parallelizing is worth it, see cpp-parallel-decompose.
---

## Goal

Express parallelism at the level of *tasks and their results*, not locks. A task runs somewhere (another thread, a pool), and a `future` is the placeholder for its eventual result. Done right, this is often simpler and safer than manual threads + shared state, because results flow back through futures instead of shared mutable memory — fewer chances for a data race.

The job is to pick the right task mechanism and wire it correctly, avoiding the well-known `std::async` footguns below.

Baseline: **C++17**. Conventions: `docs/cpp-conventions.md`. Working demos: `references/examples/`. C++17 thread pool: `references/thread-pool.md`.

---

## The std::async traps — check these every time

`std::async` is the quickest way to run a task, and the quickest way to get accidentally-sequential code. Two traps dominate:

**1. Default launch policy may not run in parallel.** `std::async(f)` lets the implementation choose `async` (new thread) *or* `deferred` (runs lazily on the calling thread when `.get()` is called — no parallelism at all). If you want parallelism, **always pass `std::launch::async` explicitly**:
```cpp
auto result = std::async(std::launch::async, compute, arg);
```

**2. A discarded `std::async` future blocks in its destructor.** The future returned by `std::async` waits for the task in its destructor. So this is secretly sequential:
```cpp
for (int i = 0; i < n; ++i)
    std::async(std::launch::async, work, i);   // each temp future destructs → blocks → serial!
```
Keep the futures alive until you've launched them all, *then* gather:
```cpp
std::vector<std::future<Result>> futures;
for (int i = 0; i < n; ++i)
    futures.push_back(std::async(std::launch::async, work, i));   // all launched
for (auto& f : futures)
    total += f.get();                                            // then gather
```
This launch-all-then-gather ordering is the single most important pattern in this skill.

---

## Patterns

Generate in project idiom. Demos use course style (`printf`, globals, C-arrays) and are structural references only.

### Run one task, use the result later
When you have independent work to overlap with the result.
```cpp
std::future<int> pantryCount = std::async(std::launch::async, countVegetables);
doOtherWork();
int count = pantryCount.get();   // blocks only if not ready yet
```
`get()` may be called once; it moves the result out and re-throws any exception the task threw. Demo: `references/examples/future_async_demo.cpp`.

### Fan-out / gather over a collection
The bread-and-butter of task parallelism: launch one task per item, then collect. Ideal for independent I/O-bound or CPU-bound items (download N URLs, process N files).
```cpp
std::vector<std::future<std::size_t>> futures;
futures.reserve(items.size());
for (const auto& item : items)
    futures.push_back(std::async(std::launch::async, processItem, item));

std::size_t total = 0;
for (auto& f : futures)
    total += f.get();
```
Watch out: one `std::async` per item spawns one thread per item. Fine for dozens of I/O-bound tasks; for hundreds or for CPU-bound work, use a **thread pool** (below) to cap concurrency. Demo: `references/examples/download_images_fanout_demo.cpp`.

### Divide-and-conquer fork-join (depth-limited)
Recursively split, run one half async and the other inline, then combine. The critical detail is a **depth threshold** — unbounded recursion spawns exponentially many threads and destroys performance. Below the threshold, fall back to sequential.
```cpp
Result solve(Range range, unsigned int depth = 0)
{
    if (depth >= maxDepth() || range.small())
        return solveSequential(range);

    auto [left, right] = range.split();
    auto leftFuture = std::async(std::launch::async, solve, left, depth + 1);
    Result rightResult = solve(right, depth + 1);      // current thread does the right half
    return combine(leftFuture.get(), rightResult);
}
```
A good threshold ties to hardware: stop forking near `std::log2(std::thread::hardware_concurrency())` levels, or when the subproblem is below a size cutoff. Demos: `references/examples/divide_and_conquer_demo.cpp` (async sum) and `merge_sort_forkjoin_demo.cpp` (std::thread fork-join with a `log(hardware_concurrency)` depth cap).

### promise / packaged_task — hand a result across threads manually
When the producer and consumer aren't a simple function call — e.g. a thread computes a value and another waits for it, or you need the future *before* deciding where the task runs.
- `std::promise<T>` — the producer calls `set_value` (or `set_exception`); the consumer holds `promise.get_future()` and calls `get()`. Use for one-off cross-thread handoff.
- `std::packaged_task<Sig>` — wraps a callable so calling it fulfills its `get_future()`. Use to queue work whose result you still want (this is how the thread pool's `submit` works).
```cpp
std::promise<int> promise;
std::future<int> result = promise.get_future();
std::thread producer([&promise] { promise.set_value(compute()); });
int value = result.get();
producer.join();
```

### Thread pool — many short tasks
Creating a thread per task costs more than the task when tasks are short or numerous. A pool reuses a fixed set of workers. C++17 has no standard pool: use `references/thread-pool.md` (or `boost::asio::thread_pool` if Boost is already a dependency). `submit` returns a `std::future` so results and exceptions still flow back. Demo: `references/examples/thread_pool_demo.cpp`.

Rule of thumb: a handful of long/independent tasks → `std::async` fan-out; many or short tasks, or you must cap concurrency → thread pool.

---

## Before returning code

- Every `std::async` meant to run in parallel passes `std::launch::async`.
- Futures from a fan-out are stored and gathered *after* the launch loop — no future is discarded mid-loop (would serialize).
- Any recursive fork-join has a depth or size threshold with a sequential base case.
- Result flow is through futures, not shared mutable state — if you find yourself adding a mutex, reconsider whether a future fits better, or see `cpp-synchronization`.
- Naming, guards, namespace, no comments per `docs/cpp-conventions.md`.
- To judge whether the parallel version is actually faster, generate a harness with `cpp-parallel-benchmark`.
