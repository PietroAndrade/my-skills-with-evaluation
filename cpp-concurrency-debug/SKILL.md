---
name: cpp-concurrency-debug
description: Diagnose and fix concurrency bugs in C++ — hangs, freezes, deadlocks, livelocks, thread starvation, torn/garbled shared values, and results that change from run to run. Use whenever multithreaded C++ misbehaves: "my program hangs and pins one CPU", "two threads freeze forever", "this counter gives a different total every time", "works in debug but not release", "sometimes deadlocks", "one thread never gets to run", "added a mutex but the result is still wrong", "intermittent crash under load". Starts from the symptom, finds the cause, applies the fix. This is the debugging counterpart to cpp-synchronization (which writes synchronization code); to choose a primitive from scratch see docs/choosing-concurrency-primitives.md.
---

## Goal

A concurrency bug shows up as a *symptom* — a hang, a wrong number, an intermittent crash — and the job is to work backward to the *cause*, then apply the right *fix*. These bugs are timing-dependent ("heisenbugs"): they hide under a debugger, vanish when you add a print, and pass in testing then fail in production. So diagnose by reasoning about thread interleavings, not by trial and error.

For the deeper discipline of forming and testing hypotheses, pair this with `superpowers:systematic-debugging`. This skill supplies the concurrency-specific knowledge: which interleaving produces which symptom, and what removes it.

Baseline: **C++17**. Style/naming: `docs/cpp-conventions.md`.

---

## Step 1 — classify the symptom

The symptom narrows the cause fast. Start here.

| Symptom | Most likely cause | Jump to |
|---|---|---|
| Program freezes, CPU near 0% | **Deadlock** — threads blocked forever waiting on each other | [Deadlock](#deadlock) |
| Program freezes, CPU near 100% | **Livelock** — threads active but repeatedly undoing each other | [Livelock](#livelock) |
| Runs, but one/some threads never make progress | **Starvation** — a greedy thread monopolizes a lock | [Starvation](#starvation) |
| Wrong/inconsistent result, changes run to run | **Data race** — unsynchronized concurrent access, one is a write | [Data race](#data-race) |
| Result wrong even *with* a mutex; depends on who runs first | **Race condition** — ordering flaw, not an access flaw | [Race condition](#race-condition) |
| Freeze that appears only after a thread exits/throws | **Abandoned lock** — a thread died holding a lock | [Abandoned lock](#abandoned-lock) |

Two of these are constantly confused, and the confusion sends people to the wrong fix:

- **Data race** = *two threads touch the same memory, ≥1 writes, no synchronization.* Fix by adding mutual exclusion.
- **Race condition** = *the outcome depends on the order threads run.* A mutex can make every access safe and the result can still be wrong. Fix by redesigning so order doesn't matter (preferred — it stays parallel), or, only when the phases genuinely must be ordered, by enforcing ordering with a barrier/latch.

You can have either without the other. Adding a mutex to a race condition is the classic wasted fix — see [Race condition](#race-condition).

**A silent ThreadSanitizer is evidence, not a blind spot.** If TSan is clean and the result still varies, that *rules out* the data race and points at the race condition. Do not reach for "some access must be unlocked" — that hypothesis contradicts the evidence you already have.

---

## Deadlock

**Symptom:** total freeze, no CPU use. Every involved thread is blocked in `lock()`.

**Cause:** a cycle in lock acquisition. The textbook case is two threads taking two mutexes in opposite orders:

```cpp
// thread A: first_chopstick.lock(); second_chopstick.lock();
// thread B: second_chopstick.lock(); first_chopstick.lock();
// A holds first, waits for second; B holds second, waits for first — forever.
```

See `references/examples/deadlock/begin_deadlock.cpp`.

**Four conditions must all hold for deadlock** (breaking any one prevents it): mutual exclusion, hold-and-wait, no preemption, circular wait. The practical levers are the last two.

**Fix — pick one:**

1. **Acquire all locks atomically** with `std::scoped_lock`, which uses a deadlock-avoidance algorithm internally:
   ```cpp
   std::scoped_lock lock(first_chopstick, second_chopstick); // takes both or neither
   ```
   This is the preferred fix — see `references/examples/deadlock/end_deadlock.cpp`.
2. **Impose a global lock order** — every thread locks mutexes in the same fixed order (e.g. by address or id). Breaks circular wait.
3. **Keep critical sections short and avoid nested locks** — the less overlap, the less surface for a cycle.

Note: `recursive_mutex` prevents the *self*-deadlock of re-locking on one thread, but does nothing for the multi-mutex cycle above.

---

## Livelock

**Symptom:** freeze-like *lack of progress*, but CPU is busy — threads are running, just not accomplishing anything.

**Cause:** threads react to each other and keep retrying in lockstep, each politely backing off at the same time so no one wins. Common when a naive deadlock-recovery scheme has every thread release and retry simultaneously:

```cpp
first_chopstick.lock();
if (!second_chopstick.try_lock()) {
    first_chopstick.unlock();
    std::this_thread::yield();   // both threads do this in sync → livelock
}
```

See `references/examples/livelock/begin_livelock.cpp` and `end_livelock.cpp` (the "polite philosophers").

**Fix:** break the symmetry so the retries aren't synchronized.
- **Randomized backoff** — sleep a random interval before retrying, so threads desync.
- **Asymmetric priority** — give threads a fixed order/priority so one consistently wins.
- Or sidestep the retry loop entirely with `std::scoped_lock` (atomic multi-acquire), which removes the release-and-retry dance.

---

## Starvation

**Symptom:** the program progresses overall, but specific threads rarely or never get the lock. With many threads, work is wildly uneven.

**Cause:** a "greedy" thread reacquires a lock so quickly that the scheduler keeps handing it back before waiting threads run. Thread priorities amplify this — low-priority threads may never get a turn. Rare with a few equal-priority threads; likely with many contending threads.

See `references/examples/starvation/` — scaling from 2 to 200 philosophers on one mutex makes the uneven distribution visible (the `end` version prints how much each thread ate).

**Fix:**
- **Shrink the critical section** so the lock is released promptly and often — the biggest lever.
- **Fair scheduling** — use a fair lock / ticket lock, or a queue that serves waiters in FIFO order, instead of a plain mutex that makes no fairness promise.
- **Rebalance priorities** — don't starve threads by over-prioritizing others.

---

## Data race

**Symptom:** wrong or inconsistent results that change between runs. A shared counter that should reach 20,000,000 lands on some smaller, random number.

**Cause:** two+ threads access the same memory concurrently and at least one writes, with no synchronization. `count++` is not atomic — it's load, increment, store; two threads interleave and lose updates.

See `references/examples/data_race/begin_data_race.cpp`. (Note: the `end` version in the corpus just raises the loop count to make the race *visible* — it demonstrates the bug, it is not the fix.)

**Fix — enforce synchronized access:**
- Single scalar, single op → `std::atomic<T>`; use `count++` / `fetch_add`.
- Multi-step update or multiple variables → `std::mutex` + `std::scoped_lock`.

For choosing between them, see `docs/choosing-concurrency-primitives.md`; for writing it, `cpp-synchronization`.

**Detection:** data races *are* tool-detectable — build with **ThreadSanitizer** (`-fsanitize=thread`) and run. TSan reports the racing accesses with stack traces, which beats staring at the code.

---

## Race condition

**Symptom:** result depends on which thread happens to run first. Crucially, it can be wrong **even when every memory access is properly locked** — so adding a mutex "correctly" doesn't help.

**Cause:** the *logic* depends on ordering that the scheduler doesn't guarantee. Example: one thread doubles a value, another adds 3. Both are mutex-protected (no data race), but `(1*2)+3 = 5` versus `(1+3)*2 = 8` depending on order.

See `references/examples/race_condition/` — note `begin` and `end` are **identical**: the point is that the mutex protects the data race but leaves the ordering bug untouched.

### The most common instance: a non-deterministic reduction

"Every access is locked, TSan is silent, the total still moves in the last digits" is almost always threads summing into a shared accumulator. The mechanism is worth naming exactly, because the usual shorthand gets it backwards:

**Floating-point `+` is commutative but *not associative*.** `(a+b)+c ≠ a+(b+c)` in general. A mutex serializes the additions but does not fix *which grouping* happens — that follows lock-acquisition order, which the scheduler picks. So the sum is reassociated differently every run.

Do not "verify" a shared accumulator by calling `+` commutative; commutativity is not the property in question. Integer and fixed-point accumulation genuinely is order-independent (modulo overflow, which wraps identically either way) — floating point is not.

**Discriminator — this separates it from a real data race without a debugger:**

| Observation | Meaning |
|---|---|
| Bounded jitter in the **low bits** (`12.783290810429849` vs `...628`), every value plausible | reduction order — no race |
| **Torn**, wildly wrong, or impossible values; counts that drop | a real data race — go back to [Data race](#data-race) |

### Fix — remove the ordering dependence; enforcing order is the worse option

- **Per-thread partials over *static* index ranges, merged in a fixed order after `join()`.** ← prefer this. Thread `t` owns `[t*n/T, (t+1)*n/T)`, writes only `partials[t]`, and the main thread folds `partials` in index order. Deterministic *and* fully parallel, and faster than the original because the per-element lock leaves the hot loop. No mutex is needed on `partials[t]`: distinct elements, never resized, and `join()` is the barrier.
  - Fix the **partial count**, not `hardware_concurrency()`, or the result changes between machines.
  - A work queue or an atomic cursor handing out chunks **reintroduces** the nondeterminism — which range a thread gets must not depend on timing.
- **Barrier / latch** — only when the phases genuinely must be ordered (all "adds" before any "double"). Understand the cost: a barrier, a turn-taking token or a single-threaded critical section buys determinism **by serializing**, so it trades away the parallelism you came for. Per-thread partials get the same determinism at full speed. `std::barrier`/`std::latch` are C++20; on C++17 use the fallback `cpp-synchronization` generates.
- **Exactness is a different goal from repeatability.** If the answer must be *accurate*, not merely reproducible, use Kahan/Neumaier summation or fixed-point integers. Per-thread partials give you the same total every run; they do not make that total the exactly-rounded one.

**Other order-sensitive combines to audit once you've found one:** shared `push_back` (output order), `min`/`max` with ties, `unordered_map` insertion order, first-writer-wins flags, seeding from thread id or clock, and any work-stealing schedule.

**Non-fixes, and why:**
- **More mutexes, or a finer/coarser one** — exclusion was never the missing piece.
- **`std::atomic<double>` `fetch_add`, or a CAS loop, or `memory_order_seq_cst`** — a total order of operations is not a *predetermined* order. Memory ordering is about visibility, not priority.
- **`-ffp-contract=off`, `-fno-fast-math`, `-O0`, `long double`** — these shrink the discrepancy, which looks like progress. The reassociation is happening at runtime, not in the compiler.
- **More sanitizer runs** — TSan's silence here is *evidence*, not a false negative.
- **Pinning threads, changing priorities, sorting the output at the end** — hides it on one machine; it returns in CI.

**Detection:** hard in general. Race conditions are the true "heisenbugs" — inserting sleeps can *shift* the interleaving and hide them. For the reduction case above there is a cheap decisive test: run the same input 20+ times and watch whether the variation is confined to the low bits, and diff against a single-threaded run. Reason about the interleaving; don't rely on reproduction alone.

---

## Abandoned lock

**Symptom:** a freeze that appears only after some thread exits early, hits a `break`/`return`, or throws — other threads then block forever on a lock that will never be released.

**Cause:** a thread acquired a lock with a manual `lock()` and left its scope (exception, early return) before the matching `unlock()`:

```cpp
chopsticks.lock();
if (done) return;   // <-- lock never released; everyone else hangs
chopsticks.unlock();
```

See `references/examples/abandoned_lock/begin_abandoned_lock.cpp`.

**Fix:** never hand-manage lock/unlock. Use an RAII guard so the lock is released on *every* exit path — normal, `break`, `return`, or exception:

```cpp
std::scoped_lock lock(chopsticks);  // released automatically at scope end
if (done) return;                   // safe — destructor unlocks
```

See `end_abandoned_lock.cpp`. This is why manual `lock()`/`unlock()` is a red flag in review — prefer `scoped_lock`/`lock_guard`/`unique_lock` everywhere.

---

## Quick fixes reference

| Cause | One-line fix |
|---|---|
| Deadlock (multi-mutex) | `std::scoped_lock(a, b)` or consistent global lock order |
| Livelock | randomized backoff / asymmetric priority / `scoped_lock` |
| Starvation | shrink critical section; fair/FIFO lock |
| Data race | `std::atomic` (scalar) or `mutex`+`scoped_lock` (multi) |
| Race condition | per-thread partials over static ranges, merged in fixed order (barrier/latch only if the phases must really be ordered — it serializes) |
| Abandoned lock | RAII guard, never manual `lock`/`unlock` |

For the code itself, route to `cpp-synchronization`. To choose a primitive from scratch, see `docs/choosing-concurrency-primitives.md`. To measure whether a fix hurt throughput, `cpp-parallel-benchmark`.
