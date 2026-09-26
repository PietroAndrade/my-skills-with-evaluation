# Worker threads hang after adding the validation check

## Verdict

The early `return` leaves `m_mutex` **locked forever**. The first invalid job
takes the lock, returns without ever reaching `m_mutex.unlock()`, and every
later caller blocks on `m_mutex.lock()` for the lifetime of the process. This
is a lost unlock on an early-exit path, not a lock-ordering deadlock.

```cpp
m_mutex.lock();          // acquired
if (!job.isValid()) {
    return;              // <-- leaves the function still holding the lock
}
m_queue.push_back(job);
m_mutex.unlock();        // only reached on the valid path
```

## How I know

1. **The symptom fits exactly.** "Every other thread that calls `process`
   afterwards freezes" is the signature of a mutex that is held and never
   released: one thread got through, all subsequent ones are parked in
   `lock()`. A genuine deadlock needs two or more locks and would freeze the
   thread that "won" as well; here the thread that returned early keeps
   running happily.
2. **It appeared with the change.** Before the early return, every path
   through the function reached `unlock()`. The new `return` is the only path
   that does not.
3. **Reproduced it.** `repro_main.cpp` feeds one invalid job and then starts
   4 worker threads pushing valid jobs.
   - `processor_buggy.cpp`: killed by `timeout 5` -> exit code 124 (hung).
   - `processor_fixed.cpp`: prints `all workers finished`, exit code 0, clean
     under ThreadSanitizer.
4. **Confirmation on a live process.** Attach with gdb and run `thread apply
   all bt`: the stuck threads all sit in `__lll_lock_wait` /
   `std::mutex::lock` inside `Processor::process`, and no thread owns the
   lock in a running frame. `gdb` on the mutex object shows `__owner` set to
   the tid of a thread that is idle or gone. `helgrind` also flags it as
   "mutex still locked at exit".

The same defect also breaks on an exception: if `push_back` throws
`std::bad_alloc`, or `isValid()` throws, the manual `unlock()` is skipped and
the process wedges the same way. The early `return` just made it happen every
run instead of rarely.

## Fix

Take the lock with an RAII guard. The destructor unlocks on **every** exit
path — `return`, `break`, `goto`, exception — so there is nothing left to
forget.

Preferred: validate *before* locking. `job` is a parameter, not shared state,
so `isValid()` needs no mutual exclusion, and doing it outside the critical
section keeps the lock as short as possible (`processor_fixed.cpp`):

```cpp
void Processor::process(const Job& job)
{
    if (!job.isValid())
    {
        return;
    }

    const std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(job);
}
```

If validation genuinely has to read shared state, keep it inside the guarded
region — the guard still releases on the early return
(`processor_fixed_locked_validation.cpp`):

```cpp
void Processor::process(const Job& job)
{
    const std::lock_guard<std::mutex> lock(m_mutex);

    if (!job.isValid())
    {
        return;
    }

    m_queue.push_back(job);
}
```

Notes:
- `std::lock_guard` for one mutex; `std::scoped_lock` (C++17) when you may
  need to take several at once, since it also handles deadlock-free ordering.
  `std::unique_lock` only when you need to unlock early or hand the lock to a
  condition variable.
- Do not "fix" it by adding `m_mutex.unlock();` before the `return`. That
  works until the next early exit or the first exception, and it is the same
  bug waiting to be re-introduced.
- If other code depends on invalid jobs being reported, return a `bool` or
  throw — but do that *after* the lock handling is RAII-based.

## The general red flag

**A raw `lock()` / `unlock()` pair in a function body.** The moment a
function manages a mutex by hand, its correctness depends on every exit path
reaching the `unlock()`, and the number of exit paths grows silently as the
function is edited (a new `return`, a `throw`, a `continue`). Reviewers see
"added a validation check", not "added an unlock-free return".

Concretely, the things to flag in review or in CI:

- Any call to `mutex.lock()` — in application code it should essentially
  never appear. Grep for `\.lock()` and `\.unlock()`; the only legitimate
  places are inside a locking utility or a `try_lock` loop.
- More than one `return` in a function between a `lock()` and its `unlock()`.
- Any non-trivial call (allocation, container insertion, callback,
  user-supplied predicate) between `lock()` and `unlock()`, because it can
  throw past the `unlock()`.

The rule to adopt: **mutex ownership is always expressed as a scope, never as
a pair of statements.** Enforceable automatically —
`clang-tidy`'s `cppcoreguidelines-*` / `bugprone-*` sets and
`clang`'s `-Wthread-safety` analysis flag hand-rolled locking, and a
ThreadSanitizer build of the test suite catches the hang in CI.

## Files

- `processor.hpp` — minimal stand-in declarations used to build the repro.
- `processor_buggy.cpp` — the reported code; hangs.
- `processor_fixed.cpp` — recommended fix (validate before locking).
- `processor_fixed_locked_validation.cpp` — fix when validation must be
  inside the critical section.
- `repro_main.cpp` — one invalid job, then 4 threads x 1000 valid jobs.

Build and run:

```sh
g++ -std=c++17 -pthread -o repro_buggy processor_buggy.cpp repro_main.cpp
timeout 5 ./repro_buggy            # exit 124: hung

g++ -std=c++17 -pthread -fsanitize=thread -o repro_fixed processor_fixed.cpp repro_main.cpp
./repro_fixed                      # prints "all workers finished"
```
