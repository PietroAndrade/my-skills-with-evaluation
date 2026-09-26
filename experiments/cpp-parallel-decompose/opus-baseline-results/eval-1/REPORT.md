# Parallel plan: 1000x1000 dense double matrix multiply on 8 cores

## 0. Numbers first (everything below follows from these)

| Quantity | Value |
|---|---|
| Output elements | N^2 = 1,000,000 |
| Work | 2N^3 = 2.0 GFLOP (1e9 FMAs) |
| Data footprint | A + B + C = 3 x 8 MB = 24 MB |
| Bytes touched per FLOP, naive | ~8 B/FLOP (intensity ~0.125 FLOP/B) |
| Bytes touched per FLOP, 64x64 blocked | ~0.05 B/FLOP (intensity ~20 FLOP/B) |
| Single-core peak, AVX2 FMA @ ~4 GHz | ~64 GFLOP/s (16 FLOP/cycle) |
| Realistic 1-thread naive `ikj` | 2-4 GFLOP/s -> 0.5-1.0 s |
| Realistic 1-thread blocked+vectorized | 15-30 GFLOP/s -> 70-130 ms |

The decisive fact: 24 MB of data does not fit in cache, and **B (8 MB) is re-read
N times** in any non-blocked loop order. That single number, not thread count,
decides your speedup.

## 1. How to split the work

**Domain (data) decomposition over the output, by row bands of C.**

Thread t owns rows `[t*N/P, (t+1)*N/P)` of C and computes them completely:

```
for i in my_rows:
    for k in 0..N:
        a = A[i][k]
        for j in 0..N:          # contiguous, vectorizable
            C[i][j] += a * B[k][j]
```

Why this axis:

- **Writes are disjoint.** Every thread writes a private, contiguous slice of C.
  No shared mutable state at all, therefore no locks, no atomics.
- **A is read privately** (each thread only reads its own rows of A).
- **B is read-only and shared** - shared reads need no synchronization and
  actively help: all cores want the same B lines, so they share L3 residency
  instead of fighting for it.
- **Perfectly balanced by construction.** Every row of C costs exactly 2N FLOPs;
  there is no data-dependent work as in sparse or branchy code.
- 1000 / 8 = 125 rows each, exact, no remainder logic. (Write the split as
  `begin = t*N/P; end = (t+1)*N/P` anyway so it stays correct for any P.)

**Do not split along k.** The k loop is the reduction dimension; splitting it
gives multiple threads write access to the same C element and forces atomics or
per-thread partial matrices (8 x 8 MB extra plus a 1M-element merge). It also
changes summation order, so results stop being bit-reproducible.

**Do not split along j (columns) alone.** Row bands are 8000 B = 125 whole cache
lines, so band boundaries land exactly on 64 B line boundaries: no false
sharing. Column bands share the cache line straddling each boundary and two
cores will ping-pong it.

2D tile decomposition of C (e.g. a 4x2 grid of 250x500 blocks) is the better
choice at much larger core counts, because it shrinks the per-thread B
footprint. At P=8 and N=1000 it buys little and costs complexity - keep row
bands.

## 2. Task graph / how much parallelism exists

- Work `W = 2N^3 = 2e9`.
- Span (critical path) `T_inf`: the only real dependency is the k-reduction per
  output element, so `T_inf = O(N) = 1000` sequential FMAs (O(log N) if you tree
  reduce, which you should not bother to do).
- Available parallelism `W / T_inf ~ 2N^2 = 2,000,000`.

You have 2 million independent dot products and 8 cores. **Parallelism is not
the constraint, by six orders of magnitude.** Every remaining decision is about
granularity, memory traffic, and overhead - not about exposing concurrency.

## 3. How many chunks

Two defensible answers; pick by how noisy the machine is.

**(a) Static, 8 chunks of 125 rows - the default.** Work per chunk is identical
and known in advance, so static partitioning is optimal: zero scheduling
overhead, best locality, each thread touches one contiguous 1 MB slab of C.
Chunk runtime ~10-100 ms, versus ~20-50 us to start a thread - overhead below 0.1%.

**(b) Dynamic with ~4x oversubscription of chunks (not threads): 32 chunks of
~32 rows, 8 worker threads.** Use this if the box is shared, is a laptop with
thermal or asymmetric cores (P-cores vs E-cores make "8 cores" unequal), or runs
anything else. Each chunk is ~64 MFLOP -> ~3-20 ms, still ~1000x the ~1-10 us
scheduling cost, so the load-balancing insurance is nearly free.

Granularity rule applied: **keep each chunk >= ~100 us of work** (>= 1000x the
per-task overhead), and use the smallest chunk that still satisfies that when
balance is uncertain. 32-64 rows per chunk fits comfortably.

Thread count: **8 threads, not more.** Threads beyond cores only add context
switches and cache pressure. On an 8-core/16-thread SMT part, benchmark 8 vs 16:
a tuned, port-saturated kernel gains ~0% from SMT, while a naive
memory-latency-bound loop can gain 10-20% because the sibling hides misses.
Measure, do not assume. Pin threads (`taskset` / `pthread_setaffinity_np`, or
`OMP_PROC_BIND=close OMP_PLACES=cores`) so the scheduler stops migrating them
and invalidating L2.

## 4. Synchronization you actually need

| Concern | Mechanism | Why |
|---|---|---|
| C writes | **nothing** | disjoint row bands |
| A, B reads | **nothing** | read-only after init, published before threads start |
| "all results ready" | **one join / barrier** at the end | the only real sync point |
| Work handout (option b) | one mutex-protected chunk counter, or an atomic `fetch_add` index | ~32 acquisitions total, uncontended |
| Visibility of A/B init | thread creation and join already impose the happens-before edge | no fences needed |

So: `std::vector<std::jthread>` (or `std::thread` + join loop, or
`#pragma omp parallel for schedule(static)`, or
`std::for_each(std::execution::par, ...)` over a row-index range) and nothing
else. **If you find yourself adding a mutex inside the loop nest, the
decomposition is wrong.** A mutex around a `+=` on C would serialize the entire
inner loop and run slower than one thread.

Correctness check that costs nothing: because row bands keep the per-element k
order identical to the sequential version, the parallel result should be
**bit-identical** to sequential. Compare with `memcmp`, not a tolerance. If it
differs you have a real bug (overlap or race), not floating-point reassociation.

## 5. What will actually limit the speedup

Ranked by how much it will cost you.

**1. Memory / L3 bandwidth - the dominant limit.**
A non-blocked `ikj` loop streams all 8 MB of B once per `i`, i.e. ~8 GB of reads
for the whole product. One core can pull that from L3; eight cores demanding it
simultaneously cannot, because L3 and the DRAM controller are *shared,
non-scaling* resources. This is why naive parallel matmul typically saturates at
**3-5x on 8 cores** - bandwidth flattens around 3-4 active cores and extra
threads add nothing. Arithmetic intensity 0.125 FLOP/B against ~50 GB/s of DRAM
caps you at ~6 GFLOP/s no matter how many cores you own.

Fix: **cache blocking.** Tile i, j, k (~64x64x64, or size blocks so one A-block +
B-block + C-block fits in L2; check L2 with `lscpu`). That raises intensity to
~20 FLOP/B, moves you off the bandwidth-limited slope of the roofline onto the
compute ceiling, and *then* the row-band parallelism scales nearly linearly.
Blocking is worth more than threading here: it is often a 3-5x single-thread win
before any parallelism.

**2. Single-thread inefficiency masquerading as good scaling.**
Beware the trap: a slow baseline makes speedup look great. Naive `ijk` (walking
B down a column, stride 8000 B, one useful double per 64 B line) wastes ~87% of
every line fetched. Loop order `ikj`, or transposing B once up front (10^6
element copy, ~0.1% of total work, itself parallelizable), plus `-O3
-march=native` so the compiler emits AVX2/FMA, matters more than thread count.
Report GFLOP/s alongside speedup so a bad baseline cannot hide.

**3. Amdahl's Law on setup.**
Allocation, initialization and result checking are serial unless you parallelize
them. Initializing 3M doubles is ~3-10 ms. Against a ~100 ms blocked kernel that
is a ~5-8% serial fraction, and Amdahl gives `1 / (0.06 + 0.94/8) = 5.7x` - you
lose over 2x of your 8x purely to setup. Against a 700 ms naive kernel the same
setup is ~1% and caps you at 7.4x. Action: parallelize (or first-touch) the
initialization with the same row-band split, and exclude allocation from the
timed region if you are measuring the kernel.

**4. Turbo / thermal clock drop.**
8 cores under a sustained AVX2 FMA load run at a lower all-core frequency than
one core at max turbo - commonly 10-25% lower on a laptop, less on a desktop.
An invisible haircut on measured speedup that no code change fixes; budget for
it rather than hunting it.

**5. Load imbalance - small but real.**
Not from the math (perfectly regular), but from OS noise, interrupts, and
P-core/E-core asymmetry. A single 125-row chunk finishing 20% late delays the
join for everyone. Mitigation is option (b): 32 chunks, dynamic.

**6. Thread startup - negligible, but only if you get it right.**
~20-50 us per thread; 8 threads = ~0.4 ms against >=100 ms of work, <0.5%. It
becomes fatal only if you create threads *inside* a loop (e.g. per row: 1000
spawns of ~16 us of work each). Create the pool once.

**7. False sharing / cache-set aliasing - avoided by design, with one gotcha.**
Row bands sidestep false sharing. But a leading dimension of exactly 1000
doubles (8000 B) can cause cache-set conflicts in tiled kernels; padding the row
stride to 1008 or 1032 doubles is a cheap experiment if blocked performance
disappoints.

### Bottom-line prediction

| Version | 1 thread | 8 threads | Speedup |
|---|---|---|---|
| Naive `ijk` (column-walking B) | ~3-6 s | ~1-1.5 s | 3-4x (bandwidth + latency bound) |
| `ikj` / transposed B, vectorized | ~0.5-1.0 s | ~0.15-0.25 s | 3.5-5x (bandwidth bound) |
| Cache-blocked + vectorized | ~70-130 ms | ~12-25 ms | **5.5-7x** (near compute bound) |

**Go / no-go: GO.** 2 GFLOP of embarrassingly parallel work with zero
synchronization in the hot path is close to the ideal case for threading.

Sequencing matters more than any single step: (1) fix loop order / transpose B,
(2) add cache blocking, (3) *then* parallelize by row bands, (4) measure GFLOP/s
and speedup at 1/2/4/8 threads. If the scaling curve flattens between 4 and 8
threads, that is the bandwidth wall, and the answer is a better block size - not
more threads.

One honest caveat: for production use, `dgemm` from OpenBLAS/BLIS/MKL already
does all of the above with hand-tuned micro-kernels and will beat a from-scratch
version by 2-5x even after you parallelize. Write this yourself to learn the
decomposition; link a BLAS if you need the throughput.
