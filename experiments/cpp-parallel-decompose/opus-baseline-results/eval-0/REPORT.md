# Baseline (Opus, no skill) — 50k log files, 40% serial I/O + 60% CPU, 8 cores
Transcribed by the orchestrator: the agent was blocked from writing this file itself.

## Verdict
Parallelize — but explicitly REFUSES to plug 40% into Amdahl as the serial fraction.
"Serial today" != "inherently serial": the 50,000 files are independent, so reads
parallelize too. Cap is hardware (storage concurrency D, 8 cores), not dependency.

Naive misapplied Amdahl: S(8) = 1/(0.40 + 0.60/8) = 2.1x  <- stated as the WRONG model.
True serial fraction = startup + enumeration + final merge + output = typically 1-3%.

## Model used instead: two-roof bound
T_parallel >= s + max(0.6/C, 0.4/D);  D = effective storage concurrency

| D | bound by | ceiling |
|---|---|---|
| 1 (HDD / 1 reader) | I/O | 2.5x |
| 2 | I/O | 5.0x |
| 4 | I/O | 10.0x |
| >=6 (NVMe QD16+) | CPU | ~13.3x |

Derated (s=0.02, eff 0.75-0.85): NVMe ~8x (plan 5-7x), SATA SSD ~4x, HDD ~2.4x.
Crossover: CPU binds once 0.4/D < 0.6/8, i.e. D > 5.3.
HDD caveat: 8 readers over 50k files = seek thrash, D can fall below 1 and the
parallel version LOSES; keep 1 reader, parallelize parsing only.
Page-cache caveat: 5 GB working set on a 32 GB box makes run 2 a pure CPU problem.

## Decomposition
Domain over files, 1 file = 1 task; batch 32-256 files if parsing < ~100us.
Topology: R reader threads -> bounded blocking queue (cap ~2-4x workers) -> 8 parser
workers with THREAD-LOCAL aggregates -> single-threaded final merge.
R=1 on HDD, R=4..8 on NVMe. Named the shared-mutex aggregate map as the most common
reason this shape shows only ~1.5x. Gave a full BoundedQueue + runParallel listing.
Extras: buffer free-list (allocator as shared resource), pad partials to cache line,
avoid std::regex in the parse loop, posix_fadvise, mmap loses on many small files.

## Verification prescribed
Measure both roofs BEFORE threading (read-and-discard sweep R=1,2,4,8 -> that curve IS D;
cross-check with fio). Control page cache (drop_caches or working set >> RAM).
Scaling curve 1..16 workers, 5+ repeats, median + spread, and read the SHAPE.
Queue-depth sampling as the cheapest bottleneck attribution (empty = I/O bound,
full = CPU bound). iostat -x, perf stat (IPC + stalled-cycles-backend), per-thread
busy/idle for imbalance, longest-processing-time-first for a few huge files.
Correctness: parallel aggregate == sequential (canonicalize key order), TSan, 20+ runs
watching for output variance.

## Notable
Ends on "stop asking why only 3x, ask which resource is at 100%" framing for the general case.
