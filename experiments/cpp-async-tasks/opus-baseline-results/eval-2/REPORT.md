# Why `std::async(processItem, item)` in a loop is not parallel

## The bug

```cpp
for (const auto& item : items) {
    std::async(processItem, item);   // return value discarded
}
```

`std::async` returns a `std::future<T>`. Here that future is a **temporary** that nobody
binds to a variable, so it is destroyed at the end of the full expression — i.e. at the
semicolon, on the very same iteration that created it.

The destructor of a `std::future` whose shared state was created by `std::async` with
`std::launch::async` is special: it **blocks until the task finishes** (C++17
[futures.async]/5 — the shared state is "released" only after the associated thread
completes; the same rule is spelled out for `~future` via the *last reference to an async
shared state* clause). No other future has this behaviour.

So each iteration does:

1. spawn a thread,
2. destroy the temporary future,
3. join that thread,
4. only then start the next item.

The net effect is a sequential run plus one thread creation/destruction per item — often
*slower* than a plain loop. Two related traps:

* **Discarded return value.** `std::async` is `[[nodiscard]]` since C++17, so a decent
  compiler warns here. Never call it for side effects only.
* **Default launch policy.** Calling `std::async(f, x)` without a policy means
  `std::launch::async | std::launch::deferred`; the implementation may legally choose
  `deferred`, in which case *nothing* runs until someone calls `get()`/`wait()`. If you
  want a thread, ask for one: `std::async(std::launch::async, f, x)`.

## The fix

Keep every future alive for the whole fan-out, then gather in a second loop. All tasks
overlap because no destructor runs until the work is done.

```cpp
std::vector<std::future<Result>> futures;
futures.reserve(items.size());

for (const auto& item : items) {
    futures.push_back(std::async(std::launch::async, processItem, item));
}

std::vector<Result> results;
results.reserve(futures.size());
for (auto& future : futures) {
    results.push_back(future.get());   // rethrows exceptions from the task here
}
```

Notes on the corrected version:

* `std::launch::async` is explicit — no deferred surprise.
* Arguments to `std::async` are **copied/moved** into the shared state by default. To pass
  by reference (and take responsibility for the lifetime) use `std::ref(item)`; for a
  member function use `std::async(std::launch::async, &Type::method, &object, args...)`.
* Exceptions thrown inside `processItem` are stored in the future and rethrown by `get()`.
  If you want every task's outcome rather than the first failure, wrap each `get()` in
  `try`/`catch` and record the error per item (see `fan_out_gather.cpp`).
* `future.get()` may be called only once. Use `wait()` when the task returns `void` and you
  only need completion, or `share()` if several consumers need the value.

## Granularity caveat

One `std::async` call per item usually means one OS thread per item. That is fine for a
handful of coarse tasks (each tens of milliseconds or more, or I/O bound), but for
thousands of short tasks the thread creation cost dominates and you lose again. In that
case cap the concurrency: chunk the range into `std::thread::hardware_concurrency()` tasks,
or feed the items to a small thread pool (`thread_pool.cpp` here shows a C++17 pool whose
`submit` returns a future, so the gather loop stays identical).

## Files

| File | Contents |
|---|---|
| `broken_loop.cpp` | The original pattern, instrumented to show it is sequential |
| `fixed_async.cpp` | Minimal correct fan-out/gather with `std::async` |
| `fan_out_gather.cpp` | Reusable `parallelTransform` helper + per-item exception handling |
| `thread_pool.cpp` | Bounded-concurrency alternative for many short tasks |
| `CMakeLists.txt` | Builds all four targets |

Measured on this machine (8 items x 200 ms sleep): broken ~1.6 s, fixed ~0.2 s.
