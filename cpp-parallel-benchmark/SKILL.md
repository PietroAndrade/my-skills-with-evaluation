---
name: cpp-parallel-benchmark
description: Generate a correct benchmark harness to measure whether a parallel C++ implementation is actually faster — time sequential vs parallel with steady_clock, average over multiple runs after a warm-up, verify both produce the same result, and compute speedup and efficiency. Use whenever the user wants to measure parallel performance: "benchmark this parallel code", "measure the speedup", "is my parallel version actually faster", "how do I time sequential vs parallel", "compute efficiency across cores", "why is my benchmark giving inconsistent numbers", "write a timing harness". Emits a harness following docs/cpp-conventions.md. Pairs with cpp-parallel-decompose (which predicts the speedup this confirms).
---

## Goal

Produce a benchmark that gives *trustworthy* numbers. Naive timing lies in predictable ways: a single run is dominated by noise, cold caches make the first run slow, timing the wrong span includes setup, and — the classic — reporting a speedup for a parallel version that computes a different (wrong) answer. This skill generates a harness that avoids all of these.

The output answers two questions: **speedup** (`sequential_time / parallel_time` — is it faster, and by how much?) and **efficiency** (`speedup / core_count` — how well are the cores actually used?). It pairs with `cpp-parallel-decompose`, which *predicts* speedup via Amdahl; the benchmark *confirms* it.

If the thing under test is a *running service* reached over a socket rather than
a function in this process, use `service-benchmark` instead — the load generator's
shape decides which metrics are measurable there, and the rules below do not cover
that.

Baseline: **C++17**. Conventions: `docs/cpp-conventions.md`. Reusable harness: `references/benchmark-harness.md`. Demos: `references/examples/measure_speedup_demo.cpp` (the canonical sum benchmark) and `merge_sort_benchmark_demo.cpp`.

---

## The measurement rules — every one matters

Each rule exists because breaking it produces a specific, misleading result:

1. **Average over multiple runs** (default ≥30 for short tasks; fewer for long ones). A single measurement is mostly scheduler and cache noise. Report the **mean and standard deviation** — a large stddev means the mean is not to be trusted and something (thermal throttling, background load) is interfering. Report the **minimum alongside the mean** too: the mean is pulled up by OS-noise outliers, while the minimum approximates the machine's best-case (interference-free) time and is often the more stable number to compare across versions. Mean+stddev tells you *how noisy*; min tells you *how fast when undisturbed*.
When the workload has a tail — anything with locks, I/O, allocation or scheduling — add **p50 and p99**. Mean and median disagreeing is itself a finding: `mean ≫ p50` means a minority of runs are far slower than typical, which is a periodic stall rather than uniform slowness, and calls for a different fix. Reporting only the mean makes those two cases indistinguishable.

2. **Warm up first, discard the warm-up.** Run the target once before timing so caches, memory pages, and (where relevant) branch predictors reach steady state. The course calls this out explicitly; the first run is otherwise an outlier that inflates the average. (In C++ there's no JIT, but cache/page warm-up is real.)

3. **Time only the measured call.** Put `steady_clock::now()` immediately before and after the call — not around setup, allocation, or input generation. Prepare inputs *outside* the timed region.

4. **Use `steady_clock`, not `system_clock`.** `steady_clock` is monotonic; `system_clock` can jump (NTP, DST) mid-measurement and produce negative or absurd durations.

5. **Verify correctness before trusting speed.** Compare the parallel result against the sequential result. A parallel version that races or drops updates can be fast *and wrong* — its "speedup" is meaningless. Bail out (or flag loudly) on mismatch. This is the rule most naive benchmarks skip.

6. **Control the environment.** Advise the user to close other heavy programs, pin to a consistent power/CPU-governor state, and be aware that "16 logical cores" may be 8 physical + hyperthreading (efficiency computed against logical count will look worse than against physical).

7. **Watch for in-place mutation.** If the measured function changes its input (in-place sort), each run after the first operates on already-processed data. Reset on a fresh copy outside the timed region, or the numbers are garbage.

8. **Interleave the variants when comparing.** Run sequential, parallel, sequential, parallel — not all of one then all of the other. A benchmark takes minutes and machines drift over minutes (clock throttling, background load, page cache warmth); measured in blocks, all of that drift lands on whichever variant ran second and is indistinguishable from a real difference. Alternating cancels it to first order and costs only the ordering.

9. **Prevent dead-code elimination.** At `-O2`/`-O3` the optimizer may delete a computation whose result is never observed — the benchmark then times *nothing* and reports an absurd near-zero. Make the result observable: accumulate it into a `volatile` sink, feed it to a `DoNotOptimize`-style barrier (`asm volatile("" : : "g"(value) : "memory")` on GCC/Clang), or return/print it. And always benchmark an **optimized build** — timing a `-O0` build measures the compiler's laziness, not your algorithm. This is the trap that makes a "10000× speedup" that isn't real.

10. **If the workload touches files, control the page cache — or the numbers are fiction.** The first run reads from disk and every later run reads from RAM, so run 2 is several times faster for reasons that have nothing to do with the code. Left uncontrolled, this either fakes a speedup (the parallel variant happens to run second) or hides one.

    Decide which question you are answering and say which in the output, because they have different answers:
    - **Cold** — drop the cache before every run (`sync; echo 3 | sudo tee /proc/sys/vm/drop_caches`) or use a working set several times RAM. Measures the real I/O path.
    - **Warm** — deliberately pre-load, discard the first run, and report the steady state. Measures the CPU path.

    Rule 8 (interleaving) does not rescue this one: alternating cancels *drift*, not a step change in where the data lives.

    The same caution covers any external state the timed region mutates or warms — a database, a connection pool, a JIT, an allocator's free lists. Reset it, or note that you did not.

    For a request/response service rather than an in-process function, stop here and use `service-benchmark` instead: closed-loop vs open-loop load generation changes the harness shape enough that this skill's advice quietly produces wrong numbers.

---

## Workflow

1. Identify the two implementations to compare (sequential baseline + parallel), or the single function to time.
2. Confirm the input and how to generate/reset it outside the timed region.
3. Confirm the lib name for namespace/guard.
4. Emit the harness — either the reusable `Benchmark` class from `references/benchmark-harness.md` (preferred when timing several functions) or an inline loop for a one-off. Include the correctness check and the speedup/efficiency report.

---

## Output shape

A benchmark run should print enough to interpret and to reproduce:

```
Average Sequential Time: 42.3 ms (± 1.1)
  Average Parallel Time: 8.7 ms (± 0.4)
Speedup: 4.86
Efficiency: 60.8% (8 logical cores)
```

Interpretation to hand back with the numbers:
- **Speedup < 1** → the parallel version is *slower*; the overhead exceeds the gain. Recommend staying sequential or revisiting granularity via `cpp-parallel-decompose`.
- **Speedup > 1 but efficiency low** → real but poor scaling; likely a large serial fraction, contention, or memory-bound work. **One thread count cannot tell you which** — sweep N = 1, 2, 4, 8, 16… and hand back the Karp–Flatt trend (`cpp-parallel-decompose` covers reading it: constant ⇒ serial section, rising ⇒ contention, falling ⇒ granularity). A single efficiency figure is a value; the curve is a diagnosis.
- **High stddev** → environment noise or an unstable algorithm; increase runs and control the machine before drawing conclusions.

---

## Before returning code

- Timing brackets only the measured call; setup/reset is outside it.
- `steady_clock` used throughout.
- A warm-up run precedes the timed loop and is not counted.
- Results are averaged over N runs with stddev reported.
- Sequential and parallel results are compared for equality before the speedup is reported.
- The measured result is observed (sink / `DoNotOptimize` / print) so the optimizer can't delete it, and the build is optimized (`-O2`/`-O3`).
- Minimum is reported alongside mean and stddev, plus p50/p99 if the workload has a tail.
- Variants are interleaved across runs, not measured in separate blocks.
- If files or other external state are involved, the page cache / warm-up state is controlled and the output says whether the numbers are cold or warm.
- Naming, guards, namespace, no comments per `docs/cpp-conventions.md`.
