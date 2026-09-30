---
name: cpp-synchronization
description: Generate correct, project-idiomatic C++ synchronization code — mutex/scoped_lock critical sections, std::atomic scalars, recursive_mutex, try_lock non-blocking attempts, shared_mutex reader-writer locks, condition_variable wait/notify, and C++17 fallbacks for semaphore/barrier/latch. Use whenever the user asks to WRITE thread-safety code: "protect this shared data with a mutex", "make this counter thread-safe", "add a reader-writer lock", "wait until the queue has an item", "let up to N threads through", "guard this critical section", "add a condition variable here", "write a semaphore", "synchronize these threads". Emits RAII-based code following docs/cpp-conventions.md. To choose which primitive first, see docs/choosing-concurrency-primitives.md.
---

## Goal

Turn "make this thread-safe" into correct synchronization code that matches the project's style. The two failure modes to avoid: **incorrect** code (missing predicate, notify inside lock, torn multi-step update) and **unidiomatic** code (manual lock/unlock, raw `new`, wrong naming, explanatory comments). This skill produces neither.

If the primitive isn't decided yet, consult `docs/choosing-concurrency-primitives.md` first — generating the wrong primitive well is still wrong. If the code already misbehaves, use `cpp-concurrency-debug`.

Baseline: **C++17**. Conventions (naming, guards, namespace, no comments): `docs/cpp-conventions.md` — read it before emitting code. Working demos of each primitive: `references/examples/`.

---

## Workflow

1. Confirm the primitive (or pick via `docs/choosing-concurrency-primitives.md`).
2. Identify the **shared state** and the exact **critical section** — the smallest span that must be atomic. Over-wide critical sections cause contention and starvation.
3. Confirm the module/lib name for namespace and include guards.
4. Emit code following the rules below and the project conventions. No explanatory comments — names carry intent.

---

## Non-negotiable correctness rules

These hold regardless of primitive, because getting them wrong produces bugs that only appear under load:

- **Always use RAII lock guards**, never manual `lock()`/`unlock()`. A guard releases on every exit path — return, break, exception — which is the only way to avoid an abandoned lock. `scoped_lock` (one or many mutexes), `lock_guard` (one, simplest), `unique_lock` (when you need to unlock early or use a condition variable).
- **Lock the smallest possible critical section.** Do slow work (I/O, callbacks) outside the lock. Copy what you need under the lock, release, then use it.
- **`condition_variable::wait` always takes a predicate.** `cv.wait(lock, [this]{ return ready; })` re-checks on every wake and survives spurious wake-ups. A bare `wait(lock)` is a bug waiting to happen.
- **`notify` outside the lock** where practical — waking a thread that then blocks on the still-held mutex is a wasted round-trip.
- **Multi-step updates need a mutex, not an atomic.** An atomic makes one operation indivisible; if an invariant spans two operations or two variables, it needs a lock.

---

## Patterns

Each entry: when it applies, the idiomatic shape, and the demo to read. Generate in project idiom (namespace, `m_` members, guard, no comments) — the demos use course style (`printf`, globals) and are structural references only.

### Mutex — protect a critical section
Multi-step update or multiple variables that must stay consistent.
```cpp
std::mutex m_mutex;

void addItem(const Item& item)
{
    std::scoped_lock lock(m_mutex);
    m_items.push_back(item);
    ++m_total;
}
```
Demo: `references/examples/mutex_demo.cpp`.

### Atomic — a single shared scalar
Counter, flag, or pointer where each operation stands alone. No lock needed.
```cpp
std::atomic<std::uint64_t> m_count{0};
m_count.fetch_add(1, std::memory_order_relaxed);
```
`memory_order_relaxed` is fine for a pure counter (no ordering dependency on other data); use the default `seq_cst` when the atomic gates access to other state. Demo: `references/examples/atomic_demo.cpp`.

### Recursive mutex — re-lock on the same thread
Only when a function holding the lock calls another that locks the same mutex. Prefer refactoring to avoid nested locking; reach for `recursive_mutex` when that isn't practical. Unlock as many times as you lock. Demo: `references/examples/recursive_mutex_demo.cpp`.

### try_lock — attempt without blocking
Do other useful work when the lock is contended instead of blocking.
```cpp
if (m_mutex.try_lock())
{
    std::lock_guard<std::mutex> lock(m_mutex, std::adopt_lock);
    // got it — do the work
}
else
{
    // busy — do something else this iteration
}
```
Demo: `references/examples/try_lock_demo.cpp`.

### shared_mutex — many readers, rare writer
Readers take a shared lock and run concurrently; a writer takes an exclusive lock. Worth it only when reads dominate.
```cpp
std::shared_mutex m_mutex;

Value read() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_value;
}

void write(const Value& value)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_value = value;
}
```
Demo: `references/examples/shared_mutex_demo.cpp`.

### condition_variable — wait for a condition
Sleep until state changes, instead of busy-polling. Always paired with a mutex and a predicate.
```cpp
std::mutex              m_mutex;
std::condition_variable m_condition;
bool                    m_ready{false};

void waitForReady()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this] { return m_ready; });
}

void signalReady()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready = true;
    }
    m_condition.notify_all();
}
```
Demo: `references/examples/condition_variable_demo.cpp`. For the queue case specifically, use `cpp-producer-consumer`.

### Semaphore / barrier / latch — C++20 types, C++17 fallbacks
On C++20, use `std::counting_semaphore`, `std::barrier`, `std::latch` directly. On the C++17 baseline, generate the small `mutex`+`condition_variable` classes in `references/cpp17-fallbacks.md` (semaphore with RAII guard, one-shot latch, reusable barrier). The course builds the semaphore the same way (`references/examples/semaphore_demo.cpp`) and uses Boost for barrier/latch (`barrier_demo.cpp`, `latch_demo.cpp`) — the fallback file gives project-idiomatic versions with no external dependency.

- **Semaphore** — cap concurrent access to N (resource pool) or signal across threads.
- **Latch** — one-shot: wait for N tasks to finish once.
- **Barrier** — reusable: rendezvous the same threads each phase.

---

## Before returning code

- Every lock is RAII — grep your own output for `.lock()` / `.unlock()` and justify any that remain.
- Every `condition_variable::wait` has a predicate.
- Critical sections are as small as the invariant allows.
- Naming, guards, namespace, and "no comments" match `docs/cpp-conventions.md`.
- If you emitted a C++20 type, confirm the target is C++20 — otherwise switch to the fallback.
