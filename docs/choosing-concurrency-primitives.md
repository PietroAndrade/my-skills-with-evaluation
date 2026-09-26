# Choosing a C++ concurrency primitive

Decision reference for picking the right primitive before writing thread code. This is shared data, not a skill — the generator/diagnostic skills link here for the "which primitive?" step:
- Writing the synchronization code → `cpp-synchronization`
- Task-based parallelism (async/futures/thread pools) → `cpp-async-tasks`
- A hang, torn value, or nondeterministic result → `cpp-concurrency-debug`
- Deciding whether/how to parallelize a whole algorithm → `cpp-parallel-decompose`
- Measuring speedup → `cpp-parallel-benchmark`

Wrong choice is expensive: an atomic where you needed a mutex gives torn updates; a mutex where an atomic would do adds needless contention; a busy-loop where a condition variable belongs burns a core.

Baseline: **C++17**. `std::barrier`, `std::latch`, `std::counting_semaphore` are C++20 — note the fallback when the target is C++17. Naming/style: `cpp-conventions.md`.

---

## The decision map

Start from what the code is actually trying to do, not from the primitive name.

| I need to… | Use | Why not the alternatives |
|---|---|---|
| Protect a multi-step update to shared data (invariant spans >1 operation) | `std::mutex` + `std::scoped_lock` | An atomic only makes *one* operation indivisible, not a sequence |
| Update a single scalar (counter, flag, pointer) shared across threads | `std::atomic<T>` | A mutex works but adds lock/unlock overhead for one op |
| Let many threads read concurrently, block only when one writes | `std::shared_mutex` | A plain mutex serializes readers that could safely run together |
| Lock the *same* mutex again from a nested/recursive call on one thread | `std::recursive_mutex` | Re-locking a plain mutex on the same thread self-deadlocks |
| Try to get the lock, do other work if it's taken (never block) | `mutex::try_lock()` | Plain `lock()` blocks; try_lock keeps the thread useful |
| Acquire two+ mutexes at once without risking deadlock | `std::scoped_lock(a, b)` | Locking them one-by-one in different orders → deadlock |
| Wait until a condition becomes true (queue non-empty, work ready) | `std::condition_variable` | Busy-polling a flag burns CPU; condvar sleeps until signaled |
| Allow up to N threads into a limited resource pool at once | `std::counting_semaphore<N>` (C++20) | A mutex allows exactly 1; a semaphore counts N permits |
| A simple gate: one-time signal from thread A to thread B | `std::binary_semaphore` / `std::latch` | Any thread may release a semaphore, unlike a mutex owner |
| Make a group of threads all wait at a rendezvous, then proceed | `std::barrier` (reusable) / `std::latch` (one-shot) | Guarantees ordering that scheduling alone can't |
| Run a function on another thread and get its result later | `std::async` + `std::future` | See `cpp-async-tasks` — task-level, not lock-level |

---

## How to reason about the close calls

### mutex vs atomic
The real question: **how many memory locations must change together, indivisibly?**
- One scalar, one operation (`count++`, set a flag, swap a pointer) → `std::atomic`. Lock-free, no blocking.
- Two operations that must appear atomic together (`if (x) y = f(x)`), or more than one variable, or a container → `std::mutex`. The atomic guarantee is per-operation; it can't span a read-decide-write sequence.

A telltale: if you catch yourself wanting "an atomic, but the check and the update must not be interrupted between them," you actually need a mutex.

### mutex vs shared_mutex
`shared_mutex` pays off **only when reads vastly outnumber writes** and reads are non-trivial. It has more overhead than a plain mutex, so if writes are frequent or the critical section is tiny, a plain mutex is often faster. Rule of thumb: mostly-readers, occasional writer (config, cache, calendar) → `shared_mutex`; otherwise `mutex`.

### semaphore vs mutex
A mutex has an *owner* — only the locker unlocks it, and it means "exactly one thread here." A semaphore is a *counter of permits* — any thread can release it, and it means "up to N threads here" or "signal from one thread to another." Use a semaphore for resource pools (N connections, N slots) and for cross-thread signaling; use a mutex for mutual exclusion of a critical section.

### condition variable vs polling a flag
If a thread needs to *wait for something to become true*, a condition variable lets it sleep and be woken exactly when the state changes — no wasted cycles. Always pair it with a mutex and a predicate (`wait(lk, []{ return ready; })`) so it re-checks on wake-up and survives spurious wake-ups. Reach for the producer-consumer topology (`cpp-producer-consumer`) when the "condition" is "the queue has an item."

### barrier vs latch
Both make threads wait for others. A **latch** counts down once and is done (spawn N workers, main waits for all to finish). A **barrier** resets after each release, so the same threads can rendezvous repeatedly across phases (iterative simulations). One-shot → latch; per-iteration → barrier.

---

## C++17 fallbacks

The target is C++17, but the natural answer is sometimes a C++20 type. When that happens, name the fallback so the choice is still actionable:

| C++20 answer | C++17 fallback |
|---|---|
| `std::counting_semaphore` | small class: `mutex` + `condition_variable` + count |
| `std::barrier` | class: `mutex` + `condition_variable` + arrival count + generation |
| `std::latch` | class: `mutex` + `condition_variable` + countdown |
| `std::jthread` | `std::thread` + explicit `join()` + `std::atomic<bool>` stop flag |

`cpp-synchronization` generates these fallbacks (`references/cpp17-fallbacks.md`). Point there rather than inlining a half-implementation.

---

## Worked examples in the corpus

Each primitive has a runnable demo under `Parallel and Concurrent Programming examples/` (the `end/` folder is the working version):

| Primitive | Demo |
|---|---|
| mutex (mutual exclusion) | `CH03/03_06/end/mutual_exclusion_demo.cpp` |
| atomic | `CH03/03_07/end/atomic_object_demo.cpp` |
| recursive_mutex | `CH04/04_03/end/recursive_mutex_demo.cpp` |
| try_lock | `CH04/04_06/end/try_lock_demo.cpp` |
| shared_mutex | `CH04/04_09/end/shared_mutex_demo.cpp` |
| condition_variable | `CH06/06_03/end/condition_variable_demo.cpp` |
| semaphore | `CH06/06_09/end/semaphore_demo.cpp` |
| barrier | `CH07/07_06/end/barrier_demo.cpp` |
| latch | `CH07/07_07/end/latch_demo.cpp` |

`cpp-synchronization` bundles project-idiomatic versions of these under its `references/examples/`.
