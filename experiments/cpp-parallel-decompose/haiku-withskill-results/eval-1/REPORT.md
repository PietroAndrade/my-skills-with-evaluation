# Parallel Decomposition Plan: 1000×1000 Dense Matrix Multiplication (8-core machine)

## Step 0: Amdahl Gate — Is it Worth It?

**Serial fraction estimate:**
- Setup (thread creation, allocation): O(n²) = ~1M operations
- Compute (matrix multiply): O(n³) = ~1B operations  
- Aggregation (barrier sync): O(n²) = ~1M operations
- **Serial fraction P_serial ≈ 0.1%** (negligible compared to compute)
- **Parallelizable fraction P ≈ 99.9%**

**Speedup ceiling (Amdahl's Law):**
$$\text{Speedup}_{\max} = \frac{1}{(1-P) + \frac{P}{S}} = \frac{1}{0.001 + \frac{0.999}{8}} = \frac{1}{0.125} \approx 7.9×$$

**Gate decision: GO.** The parallelizable fraction is >99%, serial overhead is negligible, and the theoretical ceiling (7.9×) is close to the core count (8×). This is textbook embarrassingly-parallel work.

---

## Step 1: Partition — Expose Maximum Concurrency

**Decomposition strategy: Domain decomposition (row-wise)**

Split the output matrix C by rows across 8 workers:

```
Input:  A (1000×1000), B (1000×1000)
Output: C = A·B (1000×1000)

Task 0: C[0:125, :]     = A[0:125, :]     · B
Task 1: C[125:250, :]   = A[125:250, :]   · B
Task 2: C[250:375, :]   = A[250:375, :]   · B
...
Task 7: C[875:1000, :]  = A[875:1000, :]  · B
```

Each task is independent: one worker computes a horizontal band of the output.

---

## Step 2: Dependencies — Task Graph, Work, and Span

**Task graph structure (DAG):**
- **Nodes**: 8 tasks (one per core)
- **Edges**: NONE. Each task reads from different rows of A and all of B, writes to disjoint rows of C.
- **No dependencies between tasks** — they are fully independent

**Work and span:**
- **Work (T₁)**: All matrix multiplication = 1,000 × 1,000 × 1,000 = 10⁹ operations on 1 core
- **Span (T∞)**: Longest single task = 1 task computes 125 rows × 1000 cols × 1000 reductions ≈ 125M operations (T₁/8 if balanced)
- **Ideal parallelism**: T₁/T∞ ≈ 8 (perfect; more cores would sit idle)

**Communication pattern: NONE.**
- Each task reads a disjoint band of rows from A (no contention)
- All tasks read B (read-only, no synchronization needed)
- Each task writes to disjoint rows of C (no race conditions)
- **Type**: Independent — the ideal case

**Load balance**: Perfect. 1000 rows ÷ 8 tasks = 125 rows/task (uniform work distribution)

---

## Step 3: Agglomeration — Granularity

**Chosen granularity: Coarse-grained (8 tasks total)**

Why this size:
- **Task count**: `hardware_concurrency()` = 8
- **Rows per task**: 1000 ÷ 8 = 125 rows  
- **Work per task**: 125 × 1000 × 1000 = 125M floating-point operations
- **Compute-to-communication ratio**: 125M ops per task with 0 synchronization overhead — ideal

Why NOT finer:
- Fine-grained (e.g., 1 row per task = 1000 tasks) creates massive thread overhead:
  - 1000 thread creates/joins
  - Context switching dominates the 1M op task
  - Speedup collapses from contention and cache misses

Why NOT coarser:
- Fewer than 8 tasks leaves cores idle; defeats the goal of using all 8 cores

**No load balancing needed**: all tasks have identical work; the OS scheduler can spread them freely.

---

## Step 4: Mapping — Place Tasks on Cores

**Decision: Let the OS scheduler handle it.**

For a single shared-memory machine:
- No static core affinity needed (OS does this automatically)
- No communication between tasks → no locality constraint
- Optional: Use `pthread_setaffinity_np()` to bind each thread to a specific core for cache warmth, but gains are minor for this workload

---

## Expected Speedup and Bottlenecks

**Predicted speedup (Amdahl estimate): 7.5–7.9×**

**Actual speedup will be limited by:**

1. **Memory bandwidth** (primary limiter)
   - All 8 cores read B (1000×1000×8 bytes = 8 MB per full pass)
   - Cache contention on L3 (B must fit in L3 for zero-copy; typically 8–32 MB per core)
   - Likely bottleneck: cores competing for memory bus when fetching B's lines
   - Mitigation: ensure B fits in L3; else consider block tiling

2. **Cache coherency overhead** (minor)
   - A is read by different cores from different rows (no false sharing)
   - B is read by all cores (potential L3 contention if B > L3 size)
   - C writes are disjoint (no coherency traffic)

3. **Thread overhead** (negligible for this size)
   - 8 threads, one-time creation cost = ~microseconds
   - Amortized over 125M FLOPs per task = negligible

4. **Load imbalance** (none)
   - Perfect balance; no cores idle during compute

---

## Synchronization Required

**Barriers:**
1. **Start barrier** (optional but recommended): Before compute, ensure all threads have started. Prevents measurement artifacts.
2. **End barrier** (required): After compute, wait for all workers to finish before reading C.

**Locks: NONE.** Each task writes disjoint memory.

**Atomic operations: NONE.** No shared counters or interlocks needed.

**Implementation**: Use `std::barrier` (C++20) or simple `std::condition_variable` with a shared counter.

---

## Summary: Decomposition Plan

| Aspect | Decision |
|--------|----------|
| **Go/no-go** | **GO** — 99.9% parallel, negligible serial overhead, ~7.9× ceiling |
| **Decomposition** | Domain (row-wise): split C into 8 bands, one task per band |
| **Granularity** | 8 tasks, 125 rows per task (matched to core count) |
| **Dependencies** | None — fully independent tasks |
| **Communication** | None — read-only B, disjoint writes to C |
| **Synchronization** | Start/end barriers only; no locks |
| **Expected speedup** | 7.5–7.9× (Amdahl); real speedup ~6.5–7.5× (memory bandwidth) |
| **Primary bottleneck** | Memory bandwidth (L3 cache contention on B) |
| **Implement with** | `cpp-async-tasks` (fork-join or thread pool) — no `cpp-synchronization` needed |

---

## What Actually Limits Speedup?

**Ranked by impact:**

1. **Memory bandwidth** — cores compete to read B from memory; L3 cache misses dominate
2. **Serial fraction** (Amdahl) — setup/teardown, but negligible at 0.1%
3. **Load imbalance** — none; perfect balance
4. **Synchronization overhead** — none; independent tasks

**To push closer to 7.9×:**
- Verify B fits in L3 (1000×1000×8 = 8 MB; most modern L3 = 8–20 MB per core)
- Use cache-aware tiling or block multiply if B doesn't fit
- Profile memory stalls with `perf` or similar

If measured speedup is <6.5×, profile with `cpp-concurrency-debug` to identify contention hotspots.
