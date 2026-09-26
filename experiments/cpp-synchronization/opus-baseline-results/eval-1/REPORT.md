# DownloadLimiter Report

## Primitive chosen

A counting semaphore built from `std::mutex` + `std::condition_variable`, with a
`std::size_t` slot counter (`m_availableSlots`, initialised to 4).

## Why

- The toolchain is C++17, so `std::counting_semaphore` (C++20) is unavailable.
  `mutex` + `condition_variable` is the standard C++17 way to express "at most N
  threads past this point".
- The wait is a blocking wait on a predicate (`m_availableSlots > 0`), which is
  exactly what `condition_variable::wait` with a predicate lambda expresses. The
  predicate overload also absorbs spurious wake-ups.
- A plain `std::atomic` counter was rejected: it can express the count but not
  the blocking, and spinning on it would burn CPU while many worker threads
  queue for a slot.
- `notify_one` is used on release because a single freed slot can only admit one
  waiter; waking all of them would cause a thundering herd where all but one
  immediately re-wait.
- `release()` increments under the lock and then notifies after the lock scope
  ends, so the woken thread does not immediately block on a mutex still held by
  the releaser.

## How a permit leak on exception is prevented

`DownloadLimiter::Permit` is an RAII guard:

- its constructor calls `acquire()` (blocking until a slot is free);
- its destructor calls `release()`.

Because the destructor runs during stack unwinding, a throwing download returns
the slot automatically - the exception propagates with the count already
restored. There is no code path where a caller can forget to release, and no
try/catch-and-rethrow boilerplate is needed. The guard is non-copyable so a
single acquire can never be released twice.

All locking is RAII as well: `std::unique_lock` where the condition variable
needs to unlock and relock, `std::lock_guard` elsewhere. No manual
`lock()`/`unlock()` calls appear, so an exception thrown while the mutex is held
still unlocks it.

## Files

- `downloader/include/downloader/download_limiter.h`
- `downloader/src/download_limiter.cpp`
- `examples/download_limiter_example.cpp` - 20 worker threads, limit 4, every
  5th download throws; prints the observed peak concurrency (never above 4) and
  the final slot count (back to 4, proving no leak).
