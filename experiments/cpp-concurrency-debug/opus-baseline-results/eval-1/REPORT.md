# Mutexes everywhere, TSan silent, result still varies

## Short answer

You do not have a data race. You have a **non-deterministic reduction order**.

A mutex gives you **mutual exclusion** — no two threads touch the shared result at the
same time. It does not give you **ordering** — it says nothing about *which* thread gets
the lock next. The order in which contributions land in the shared result is decided by
the OS scheduler, and it is different on every run.

That only matters if your combine step is **order-sensitive**. The overwhelmingly common
case is floating-point accumulation: `+` on `double` is commutative but **not
associative**, so `(a+b)+c != a+(b+c)` in the last bits. Same inputs, same arithmetic,
different grouping, different answer.

This is exactly consistent with your three observations:

| Observation | Explanation |
|---|---|
| TSan reports nothing | Correct — every access *is* properly synchronized. TSan detects unsynchronized access, not wrong order. |
| Result differs every run | Lock acquisition order differs every run. |
| A second mutex did not help | More locks add more exclusion. The problem is not a lack of exclusion. |

## How I know

`repro.cpp` in this directory is your bug in 70 lines: 8 threads, one `std::mutex`,
every access to `total` under `std::scoped_lock`. Deterministic input, deterministic
arithmetic.

```
run 0  buggy = 12.783290810429849   fixed = 12.783290810429628
run 1  buggy = 12.783290810429836   fixed = 12.783290810429628
run 2  buggy = 12.783290810429849   fixed = 12.783290810429628
run 3  buggy = 12.78329081042984    fixed = 12.783290810429628
run 4  buggy = 12.783290810429829   fixed = 12.783290810429628
```

Built with `-fsanitize=thread`, TSan prints **zero** reports and the buggy column still
wanders. That is the signature: *clean sanitizer + unstable result = ordering, not
racing.*

Note the shape of the noise. It is in the low bits, it is bounded, and it does not grow
without limit. A real race on a `double` would occasionally give you garbage, a torn
value, or a wildly wrong magnitude. Low-bit jitter is a reduction-order fingerprint.

Reproduce:

```sh
g++ -std=c++17 -O2 -pthread repro.cpp -o repro && ./repro
g++ -std=c++17 -O1 -g -fsanitize=thread -pthread repro.cpp -o repro_tsan && ./repro_tsan
```

## The fix

Make the combine order a property of the **data**, not of the **schedule**. Give each
worker a private slot, then merge in fixed index order on one thread:

```cpp
std::vector<double> partials(kThreadCount, 0.0);

for (std::size_t t = 0; t < kThreadCount; ++t)
{
    threads.emplace_back([&input, &partials, t] {
        const std::size_t begin = input.size() * t / kThreadCount;
        const std::size_t end = input.size() * (t + 1) / kThreadCount;
        double local = 0.0;
        for (std::size_t i = begin; i < end; ++i)
            local += input[i];
        partials[t] = local;
    });
}
for (auto& thread : threads)
    thread.join();

const double total = std::accumulate(partials.begin(), partials.end(), 0.0);
```

Three properties make this deterministic:

1. **Static ranges.** Thread `t` owns `[begin, end)`, computed from `input.size()` and
   `t` only. Not a work queue, not an atomic cursor — those hand out different ranges
   each run and reintroduce the problem.
2. **Private accumulator.** Each thread sums its own range in index order.
3. **Fixed-order merge.** `partials` is merged by index after `join()`, so the grouping
   of the additions is identical on every run.

`partials[t] = local` needs no mutex: distinct threads write distinct elements of a
`vector<double>` that is never resized, and `join()` is the synchronization point before
the merge. It also runs faster than the original, because the hot loop no longer takes a
lock per element.

If you also need the result to be independent of **thread count**, the merge must be a
fixed-shape tree (or a sequential pass over a fixed number of partials) rather than
`kThreadCount` partials — the value above is stable per run but changes if you go from 8
threads to 16. Pin the partial count to a constant, not to `hardware_concurrency()`.

If you need exactness rather than just repeatability, change the arithmetic: accumulate
in a wider type, use Kahan/Neumaier compensated summation, or — best when your domain
allows it — use fixed-point integers (e.g. cents as `std::int64_t`). Integer addition
*is* associative, so order stops mattering at all.

## Other order-sensitive combines to check

Floating point is the usual culprit, but the same diagnosis applies to any of these,
even fully mutex-protected:

- `result.push_back(x)` into a shared container — arrival order becomes element order.
- `if (candidate < best) best = candidate;` where **ties** exist — whichever thread wins
  the race to the tie decides. Break ties on a stable key (index, id).
- Inserting into `std::unordered_map` — iteration order can depend on insertion order,
  so a later fold over it is order-dependent.
- "First writer wins" flags, `bestPath`, `firstError`, reservoir sampling.
- Anything seeded from `std::this_thread::get_id()`, a clock, or an atomic counter.
- Work-stealing / dynamic chunking, which changes *which* values each thread groups.

Rule of thumb: for every shared mutation, ask "if these operations arrived in the
reverse order, would the final value be bit-identical?" If no, the mutex is not the
mechanism you need.

## Fixes that sound right and will not work

**Adding another mutex / a finer-grained lock.** You already proved this. Exclusion was
never missing.

**`std::recursive_mutex`, or holding the lock for a longer critical section.** Changes
nothing about who gets the lock first. A coarser lock makes it *slower* and no more
deterministic.

**Making the accumulator `std::atomic<double>` with `fetch_add`, or using
`memory_order_seq_cst`.** Atomics and seq_cst give you atomicity and a total order of
*operations*, but not a **predetermined** one. The total order is still chosen by the
hardware and scheduler, so the grouping of the floating-point additions still varies.
Memory ordering is about visibility, not about which thread goes first.

**`std::atomic<double>` with a compare-exchange loop.** Same reason, plus it retries
under contention, which if anything makes arrival order *more* schedule-dependent.

**Running more TSan / Helgrind / valgrind passes.** They are already telling you the
truth. A race detector cannot flag correct synchronization, and this code is correctly
synchronized. Silence here is evidence, not a false negative.

**`-ffp-contract=off`, `-fno-fast-math`, `-O0`, disabling FMA, `long double`
intermediates.** All worth doing for other reasons, and they *reduce* the magnitude of
the discrepancy, which makes it look like progress. They do not remove it: the
non-determinism comes from runtime accumulation order, not from compile-time
reassociation. `-O0` with 8 threads still varies.

**Reducing the thread count to 1.** Makes the result stable, which will tempt you to
call it fixed. It removes the parallelism, not the bug — the code is still wrong at 2
threads.

**Pinning threads to cores / raising priorities / `sched_setaffinity`.** Makes the
schedule *more* repeatable on your machine and hides the symptom locally. It will come
back on different hardware, under load, or in CI.

**Sorting the output at the end.** Fixes the *container-order* variant above, but not
the floating-point sum — by then the additions have already happened in the wrong order.

**Adding a `std::condition_variable` to sequence the threads,** or a mutex-based token
that forces thread 0, then 1, then 2. This *does* produce a deterministic result, and it
is the trap worth naming explicitly: you have serialized the program. You are paying for
8 threads to run one at a time plus wakeup latency. Per-thread partials get the same
determinism with actual parallelism.

## Files

- `repro.cpp` — minimal reproduction (`buggySum`) next to the fix (`fixedSum`).
