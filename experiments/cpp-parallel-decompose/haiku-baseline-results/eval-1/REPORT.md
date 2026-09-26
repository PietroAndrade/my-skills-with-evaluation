# Haiku baseline (no skill) — decompose eval-1, 1000x1000 matmul plan
Transcribed by the orchestrator: agent returned text only, wrote no files.

## Decomposition — CORRECT
Row bands of C, 8 chunks of 125 rows, 1:1 threads-to-cores, disjoint writes,
zero false sharing, implicit barrier only. Names B as a shared read-only broadcast.

## Bottlenecks
Ranks memory bandwidth first (correct), then L3 capacity, SIMD/vectorization,
thread overhead (negligible), NUMA. Recommends BLAS, tiling, -O3 -march=native,
thread pinning, non-temporal stores. Estimate 5-7x.

## NUMERIC DEFECTS
- Arithmetic intensity computed as 2e9 ops / 136 MB = 14.7 ops/byte. Wrong for the
  naive loop: non-blocked ikj re-reads B once per row, ~8 GB of traffic, giving
  ~0.125 FLOP/B. It assumed B is read once per thread, which is the BLOCKED ideal.
  Off by roughly two orders of magnitude -- yet it still concluded "bandwidth bound",
  so the conclusion survives its own broken arithmetic.
- "Single-threaded peak (1 core, 1 GHz, 256-bit SIMD, 2 ops/cycle)" -- 1 GHz is not
  a plausible modern clock; the derived 2-4 GFLOP/s figure is coincidentally near the
  real naive number.
- "L3 typically 8-20 MB per core" -- L3 is shared, not per core.

## MISSED (Opus baseline had all of these)
- Never split along k (reduction dimension: atomics or 8 partial matrices).
- Row bands vs column bands specifically because of the straddling cache line.
- Work/span: W=2N^3, span=O(N), parallelism ~2e6.
- Padding the leading dimension (1000 doubles -> cache-set conflicts).
- Reporting GFLOP/s next to speedup so a slow baseline cannot fake good scaling.
- Blocking is worth MORE than threading (3-5x single-thread).

## Grade
Right plan, right primary bottleneck, unreliable numbers.
