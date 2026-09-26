---
name: cpp-parallel-decompose
description: Decide whether and how to parallelize an algorithm before writing threads — estimate the achievable speedup with Amdahl's Law, decompose the problem (domain vs functional), reason about dependencies with a task graph (work/span/critical path), choose granularity, and map tasks to cores. Use whenever the user is planning parallel work rather than coding a primitive: "is it worth parallelizing this", "how should I split this across cores", "what speedup can I expect", "how do I break this problem into parallel tasks", "will more threads help here", "design a parallel version of this algorithm", "why did adding threads barely help". Produces a decomposition plan and a go/no-go call, then routes to cpp-async-tasks or cpp-synchronization to implement.
---

## Goal

Parallelism has a cost — threads, communication, synchronization — and a ceiling. Before writing any threaded code, decide whether the speedup justifies the cost and, if so, how to carve up the work. Skipping this step is how people end up adding threads that make code slower or barely faster.

This skill is advisory: it produces a *plan and a recommendation*, not code. Once the plan is set, implement it with `cpp-async-tasks` (task/fork-join/pool) or `cpp-synchronization` (if shared state needs guarding), and validate the result with `cpp-parallel-benchmark`.

Worked decompositions to reference: `references/examples/` (row-wise matrix multiply = domain decomposition; recursive merge sort and sum = divide-and-conquer).

---

## Step 0 — the Amdahl gate: is it even worth it?

Before designing anything, estimate the ceiling. **Amdahl's Law** bounds speedup by the *serial* fraction of the program:

$$\text{Speedup}_\max = \frac{1}{(1-P) + \frac{P}{S}}$$

where `P` is the parallelizable fraction and `S` the speedup on that part (≈ core count).

The sobering consequence — the serial part dominates fast:

| Parallelizable `P` | Max speedup (S → ∞) |
|---|---|
| 50% | 2× |
| 75% | 4× |
| 90% | 10× |
| 95% | 20× |
| 99% | 100× |

So a program that's 95% parallel can *never* exceed 20×, no matter how many cores. With 8 cores and P=0.95, actual speedup ≈ 5.9×.

**Use this as a gate:**
- Small `P`, or a large unavoidable serial section (I/O, dependent steps) → parallelizing may not be worth the complexity. Say so.
- Large `P` and a real per-core win → proceed to decomposition.

### Before using `1-P`: serial *today* is not the same as inherently serial

This is the most common way the gate produces a wrong number, and the failure is silent — the arithmetic is right and the answer is still wrong. A profile that says "40% of wall time is reading files" is reporting **how the program is written now**, not a dependency. If the 50,000 files are independent, the reads parallelize too, and `1-P = 0.40` is simply false.

Only count as serial what *cannot overlap with anything*: startup, enumerating the work, the final merge, writing the single output. In a batch job over independent items that is typically **1–3%**, not 40%.

**When a phase is hardware-bound rather than dependency-bound, one serial fraction cannot express the ceiling — use a two-roof bound.** With `C` cores and `D` = effective device concurrency (how many times more throughput the device delivers with many outstanding requests than with one):

$$T_\text{parallel} \ge s + \max\left(\frac{f_\text{cpu}}{C},\ \frac{f_\text{io}}{D}\right)$$

Worked on the 40% I/O / 60% CPU job at 8 cores, with `s = 0.02`:

| `D` | CPU term | I/O term | Bound by | Ceiling | Derated |
|---|---|---|---|---|---|
| 1 (HDD, or a single reader) | 0.075 | 0.400 | I/O | 2.5× | ~2.4× |
| 2 | 0.075 | 0.200 | I/O | 5.0× | ~4× |
| 4 | 0.075 | 0.100 | I/O | 10.0× | — |
| ≥6 (NVMe at QD 16+) | 0.075 | ≤0.067 | CPU | ~13.3× | ~5–7× |

The crossover — where extra worker threads stop buying anything — is at `f_io/D < f_cpu/C`, here `D > 5.3`. **Plugging 0.40 into Amdahl gives 2.1×, which is only correct for `D = 1`.** On flash it under-predicts by 2–3×.

Two consequences to state whenever I/O is in the picture:

- **`D` is measurable in an afternoon, and it decides the answer.** A read-and-discard pass at 1, 2, 4, 8 readers, recording aggregate MB/s, *is* the `D` curve. Measure it before writing any threads, and write the predicted ceiling down first.
- **More readers can be slower.** On a spinning disk, many threads opening across tens of thousands of files turn near-sequential reads into seek thrash; `D` drops *below* 1 and the parallel version loses. On an HDD keep **one** reader and parallelize only the CPU phase — which reaches the 2.4× ceiling anyway.

Also check the working set against RAM: 50,000 × 100 KB = 5 GB on a 32 GB box means the second run reads from page cache and the job becomes purely CPU-bound. Cold and warm are different problems with different answers — measure both, and control the cache (`drop_caches`, or a working set several times RAM) or the numbers are fiction.

The same reasoning applies to any shared, saturable resource: memory bandwidth, a network link, a database connection pool. Ask *which resource is already at 100%*, not *what is the serial fraction*.

### When the model stops applying

Amdahl assumes the serial section is a *fixed* cost — the same whether you run 2 threads or 32. Contention is not: lock waiting, cache-line ping-pong and allocator pressure all grow with thread count. Two symptoms say the model has stopped describing the system, and both show up once a benchmark sweeps thread count:

- **Speedup peaks and then declines.** Amdahl is monotonic in `S`; it cannot produce negative scaling. A throughput curve that rises to 4 or 8 threads and falls after is contention, not a serial fraction.
- **The implied serial fraction drifts upward with `N`** (see the Karp–Flatt sweep below).

When either appears, report the estimate at the peak as an order of magnitude, say plainly that the values past it are not interpretable, and treat the finding as "locate and reduce the contention" rather than "the serial fraction is X". `cpp-concurrency-debug` and `cpp-synchronization` cover where to look.

### Diagnosing an observed speedup: Karp–Flatt across a sweep, never one point

When the question is "I got `S`× on `N` threads, why so little?", inverting Amdahl at that single point is not enough to answer it. The **Karp–Flatt metric** is that inversion — the experimentally determined serial fraction —

$$e(N) = \frac{1/S(N) - 1/N}{1 - 1/N}$$

but its value at one `N` is ambiguous. 3× on 16 threads gives `e = 0.289`, and that is equally consistent with three different problems needing three different fixes:

| Trend of `e(N)` as `N` grows | Diagnosis | Where to look |
|---|---|---|
| Roughly **constant** | a genuine serial section — Amdahl really is the limit | find and shrink the serial code |
| **Rising** | per-thread overhead: contention, false sharing, allocator, or a shared resource saturating | `cpp-concurrency-debug`; measure the resource against its roofline |
| **Falling** | granularity/startup dominated at low `N` — chunks too small for the fixed costs | agglomerate (Step 3) |

So: **measure `S` at N = 1, 2, 3, 4, 6, 8, 12, 16…, tabulate `e(N)`, and read the trend.** Report the trend, not the single number. One data point is a value; the curve is a diagnosis.

Two ways to grow `P`: **strong scaling** (fixed problem, more cores → finish faster, hits the Amdahl wall) vs **weak scaling** (grow the problem with the cores → keep per-core work constant, sidesteps the wall). If the user's goal is "handle bigger inputs" rather than "finish this input faster," weak scaling reframes the whole analysis.

---

## Step 1 — partition: expose maximum concurrency

Break the problem into the *smallest* independent pieces first; agglomerate later (Step 3). Two complementary lenses:

- **Domain (data) decomposition** — split the *data* into chunks, run the same operation on each. The default and usually the foundation. Example: matrix multiply splits the output rows across workers (`references/examples/matrix_multiply_domain_decomp.cpp`); each worker computes a disjoint band of rows.
- **Functional decomposition** — split the *work* into distinct stages/tasks that do different things. Example: a pipeline where stage A's output feeds stage B. Often layered on top of domain decomposition.

Start with domain decomposition; check whether functional decomposition exposes extra parallelism or a natural pipeline.

---

## Step 2 — dependencies: the task graph, work, and span

Model the computation as a **DAG**: nodes = tasks, directed edges = "must finish before." This tells you the *inherent* parallelism, independent of core count:

- **Work (T₁)** — total time on one core = sum of all task times.
- **Span (T∞)** — the critical path = longest chain of dependent tasks = the fastest you could *ever* go with unlimited cores.
- **Ideal parallelism = T₁ / T∞** — the maximum useful core count. Beyond this, more cores sit idle because the critical path can't be shortened.

If the span is long (a mostly-sequential chain of dependencies), parallelism is limited *regardless* of Amdahl — the dependency structure itself is the bottleneck. Independent tasks (no edges between them) are the ideal case: work spreads freely.

**Classify the communication** between tasks, because it's where parallel overhead hides:
- **Independent** — no data shared (frost each cupcake alone). Cheapest; parallelize freely.
- **Point-to-point** — each task talks to a few neighbors.
- **Collective** — broadcast/scatter/gather across many tasks.
- **Centralized coordinator** — one task feeds many workers; can become a bottleneck as workers grow (mitigate with divide-and-conquer).

---

## Step 3 — agglomeration: choose granularity

Now combine the fine pieces from Step 1 into right-sized tasks. This is the central performance tradeoff:

| Granularity | Tasks | Pro | Con |
|---|---|---|---|
| **Fine-grained** | many small | great load balancing | high communication/sync overhead, low compute-to-communication ratio |
| **Coarse-grained** | few large | low overhead, more time computing | load imbalance — some workers idle while others grind |
| **Medium** | balanced | usually best on general-purpose CPUs | — |

The lever is the **compute-to-communication ratio**: agglomerate until each task does enough work to dwarf the cost of coordinating it. Matrix multiply does this by giving each worker a *band of rows* (chunk ≈ `rows / hardware_concurrency`) rather than one cell per task — coarse enough to amortize thread cost, fine enough to keep all cores busy.

**Don't hard-code the task count.** Tie it to `std::thread::hardware_concurrency()` (or a runtime parameter) so the program adapts to the machine.

---

## Step 4 — mapping: place tasks on cores

Only relevant for distributed systems or manual affinity — for ordinary multithreaded C++ on one machine, the OS scheduler handles this and you can skip it. When it does matter, two goals that often conflict:
- **Maximize concurrency** — put independent tasks on different cores.
- **Maximize locality** — put frequently-communicating tasks on the same core/cache.

Dynamic workloads may need load balancing (e.g. work-stealing) rather than a static mapping.

---

## Output: the decomposition plan

Deliver a short, concrete recommendation:

1. **Go / no-go**, justified by Amdahl `P` and the span. If no-go, say why (serial-bound, dependency chain, overhead > gain).
2. **Decomposition** — domain and/or functional, and what the unit of work is.
3. **Granularity** — task size and count, tied to `hardware_concurrency`.
4. **Dependencies/communication** — independent, point-to-point, collective? Any barrier/ordering needed?
5. **Expected speedup** — at the target core count, so the benchmark has something to confirm. Give a *range keyed to the binding resource*, not a single Amdahl number, whenever I/O, memory bandwidth or any other shared device is in play (e.g. "5–7× on NVMe, ~4× on SATA, ~2.4× on a spinning disk — measure `D` first").
6. **Implement with** — `cpp-async-tasks` (fork-join for recursive splits, fan-out/pool for independent chunks) or `cpp-synchronization` (if chunks touch shared state); confirm the win with `cpp-parallel-benchmark`.

### Before returning the plan — checklist

- [ ] Did I count as serial only what **cannot overlap**, rather than what happens to be serial in the current code?
- [ ] If a shared device (disk, network, memory bandwidth) is involved, did I give a **two-roof bound** and name `D` as the thing to measure first — instead of one Amdahl number?
- [ ] Did I name the case where **parallelizing loses** (HDD seek thrash, oversubscription, page-cache effects)?
- [ ] For "why only `S`×?", did I prescribe a **Karp–Flatt sweep and its trend**, not a single inverted point?
- [ ] Is the expected speedup a **range tied to a measurable quantity**, so the benchmark can falsify it?
