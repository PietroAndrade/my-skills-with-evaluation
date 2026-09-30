# Haiku baseline (no skill) — decompose eval-2, 16 threads -> 3x
Transcribed by the orchestrator: agent returned text only, wrote no files.

## Framing
Computed "81% serial or waiting, 19% useful" -- loose, not Amdahl inversion.
Did NOT invert Amdahl to a serial fraction. Did NOT mention Karp-Flatt.

## Causes (6, ranked)
1 memory bandwidth saturation (top suspect, correct for image work)
2 false sharing, 3 lock contention, 4 load imbalance, 5 I/O, 6 thread create overhead.
Missed: SMT/hybrid-core confusion, all-core turbo downclock, NUMA first-touch,
nested parallelism/oversubscription, multi-pass pipeline structure.

## Measurements
Long tool list (perf stat, cachegrind, likwid, strace futex, iostat, scaling sweep,
context-switch counts, TLB misses) + a diagnosis tree.

## Defects
- "Speedup proportional to sqrt(threads) => memory bound" is invented, not a real rule.
- "Miss ratio > 25% => bandwidth saturation" -- arbitrary threshold presented as fact.
- No roofline: never compares measured traffic against a measured bandwidth ceiling.
- Per-core utilization framed as the discriminator; bandwidth-bound code shows 100%
  on all cores, so that test does not separate its own top suspect.

## Grade
Directionally right (bandwidth first, scaling curve, cache profiling) but the
quantitative reasoning is weak and two stated rules are fabricated.
