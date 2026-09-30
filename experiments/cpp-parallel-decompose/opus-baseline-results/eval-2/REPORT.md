# Why 16 threads gave you 3x — and how to find the real ceiling

## 0. First, what your number already tells you

Parallel efficiency: `E = S/N = 3/16 = 18.8%`. You are wasting 13 of 16 cores.

Invert Amdahl's Law to see what serial fraction *would* explain it:

```
S(N) = 1 / (s + (1-s)/N)
3    = 1 / (s + (1-s)/16)   =>   s = 0.289
```

So the observation is *consistent* with ~29% of your runtime being
unparallelizable. But it is equally consistent with a hard resource ceiling
(memory bandwidth) or with per-thread overhead that grows with N. Amdahl alone
cannot distinguish these, and picking the wrong one sends you optimizing the
wrong code. The single most valuable measurement (Section 2.1) separates them.

**Use the Karp–Flatt metric**, which is the experimentally determined serial
fraction at each thread count:

```
e(N) = (1/S(N) - 1/N) / (1 - 1/N)
```

Then read its *trend*:

| Trend of `e(N)` as N grows | Diagnosis |
|---|---|
| Roughly constant | Genuine serial section — Amdahl is really your limit. Attack the serial code. |
| Rising steadily | Per-thread overhead: synchronization, contention, false sharing, allocator, or a shared bandwidth/port resource saturating. |
| Falling | Granularity/startup dominated at low N; your chunks are too small relative to fixed costs. |

At N=16, S=3, `e = (0.3333 - 0.0625)/0.9375 = 0.289`. One data point is
useless. You need the curve.

---

## 1. The likely causes, ordered by how often they are the real answer in image pipelines

### 1.1 DRAM bandwidth saturation — the #1 suspect (probability: high)

Image processing is almost always **memory-bound, not compute-bound**. A
typical filter does a handful of operations per byte touched: arithmetic
intensity of roughly 0.05–1 FLOP/byte. The roofline for such a kernel is the
memory ceiling, and **memory bandwidth is a shared resource that does not scale
with core count.**

The characteristic signature is exactly what you are seeing: speedup climbs
near-linearly to 3–5 threads, then *flattens into a plateau*. One modern core
can already pull 10–20 GB/s of streaming traffic; a 2-channel desktop socket
tops out near 50–80 GB/s. 80/20 ≈ 4 cores to saturate. After that, extra
threads only add queuing delay at the memory controller — sometimes making it
slightly *slower*.

This is not a bug in your threading. It is a property of your algorithm's data
movement, and no amount of thread tuning fixes it. The fix is to move less
data (Section 4).

### 1.2 Multi-pass pipeline structure (probability: high, and compounds 1.1)

If your pipeline is `load -> convert -> blur -> sharpen -> resize -> encode`
and each stage is its own parallel loop over the whole image, you are streaming
the entire frame from DRAM and back **once per stage**. With 6 stages on a
frame far larger than L3, you multiply your bandwidth demand by ~6 and hit 1.1
that much sooner. You also pay a full barrier between every stage, and each
barrier costs you the slowest thread, not the average.

### 1.3 Hidden serial sections (probability: high)

Amdahl's serial fraction is usually not where people think. Audit for:

- **Image decode/encode.** libjpeg, libpng, zlib are single-threaded per image.
  If you parallelize only the filtering and decode one frame serially, decode is
  your `s`. For a 30%-serial signature this is the prime candidate.
- **File I/O and disk.** Reading frames, writing results.
- **Memory allocation of output buffers**, zeroing, `cv::Mat` creation.
- **Setup/teardown**: building LUTs, kernels, histogram merge, final reduction.
- **The main thread gathering results** while the workers idle.
- **Logging or progress reporting** on a shared stream — a global lock in an
  inner loop can serialize nearly everything.

### 1.4 Load imbalance (probability: medium-high)

Splitting an image into 16 equal row bands gives 16 equal *pixel counts*, not
16 equal *work items*. Imbalance arises from:

- Content-dependent work: early-exit paths, contour tracing, feature detection,
  variable-size search windows, entropy coding.
- Border/halo handling making edge tiles cheaper or more expensive.
- One thread also doing the merge or the I/O.
- Static scheduling with an odd chunk count: 17 chunks over 16 threads means a
  second round where 15 threads idle — an instant 2x loss.

A barrier costs you `max(thread_time)`. If one thread takes 3x the mean, your
speedup is capped near `N/3` regardless of everything else.

### 1.5 SMT / core-count confusion (probability: medium — check this in 10 seconds)

"16-core machine" is often 8 physical cores × 2 hyperthreads, or a hybrid
P-core/E-core part (e.g. 8P+8E) where the E-cores are 2–3x slower and, if you
give them equal-size chunks under static scheduling, they become your
stragglers and set the barrier time. Two SMT siblings sharing one core's
execution ports typically yield 1.1–1.3x, not 2x — on a memory-bound kernel,
often ~1.0x. If you truly have 8 physical cores, the realistic ceiling is ~8x
and 3x is a much smaller shortfall than you think.

### 1.6 False sharing (probability: medium)

Threads writing to distinct elements that land on the same 64-byte cache line
cause the line to ping-pong between cores. Costs a hundred-plus cycles per
write and scales *badly* with N, so it shows up as `e(N)` rising sharply.
Classic instances:

- Per-thread accumulators in an array: `results[tid]++`, `histograms[tid][bin]`.
- Partitioning an image by *columns* instead of rows in a row-major buffer —
  adjacent threads write the same lines on every row.
- Tile boundaries not aligned to cache lines (or, worse, to page boundaries)
  when threads write across the seam.
- Row stride not padded, so tile edges alias.

### 1.7 Synchronization overhead and granularity (probability: medium)

- A mutex around anything in the inner loop.
- One task per pixel or per row: task dispatch costs ~0.1–1 µs, so work items
  under ~10–100 µs are dominated by overhead. Rule of thumb: chunk work so each
  item is ≥ 50–100 µs.
- Barriers per row instead of per frame.
- Atomic counters on a shared work queue hammered by 16 threads.
- `std::thread` creation per frame instead of a persistent pool.

### 1.8 Allocator contention (probability: medium)

Per-tile or per-frame `new`/`malloc` from 16 threads serializes in glibc's
arena locks and generates page faults. Every freshly `mmap`ed buffer costs a
minor page fault per 4 KiB page on first touch, and the kernel's `mmap_lock`
is per-process. Pipelines that allocate a new output `Mat`/`vector` per work
item often lose half their scaling here.

### 1.9 Nested/accidental parallelism — oversubscription (probability: medium)

If you call into OpenCV, IPP, MKL, OpenBLAS, libvips, or an OpenMP-enabled
library from inside your own 16 threads, that library spawns *its own* pool.
16 × 16 = 256 runnable threads, massive context-switching, and cache thrash.
Fix: `cv::setNumThreads(0)`, `omp_set_num_threads(1)`,
`OPENBLAS_NUM_THREADS=1` inside worker threads — or drop your own threading and
let the library do it.

### 1.10 Frequency and thermal effects (probability: medium — it is a real, often
overlooked 10–30%)

All-core turbo is lower than single-core turbo. A part that boosts to 5.0 GHz
on one core may hold only 4.0 GHz on all 16 — a built-in 20% haircut on your
ideal speedup *before* anything else. AVX-512/AVX2 heavy code triggers further
licence-based downclocking. Power and thermal limits (PL1/PL2, `tau`) mean a
long run throttles further than a short one, so a 10-second benchmark and a
10-minute production run give different answers.

### 1.11 LLC capacity contention (probability: medium)

At 1 thread your working set may fit in L3. At 16 threads, 16 tiles compete for
the same L3, each thread's effective share drops 16x, the set spills to DRAM,
and you land back in 1.1. Signature: cache-misses per instruction rises sharply
with N.

### 1.12 NUMA / first-touch (probability: low on 1 socket, high on 2)

On a multi-socket or chiplet machine (or any machine with NPS/NUMA-per-socket
enabled), pages live where they were *first touched*. If the main thread
allocates and zeroes the whole image, all pages sit on node 0 and half your
threads read across the interconnect at 1.5–2x the latency and a fraction of
the bandwidth. Fix: allocate then have each worker first-touch its own tile, or
interleave.

### 1.13 Measurement artifacts (probability: low, but free to rule out)

- Sequential baseline built with different flags/optimization than the parallel
  build.
- Timing includes process startup, thread pool creation, or first-run page-cache
  misses; no warm-up.
- A single run, not a median of several; noise from other load on the box.
- The parallel version does extra work (redundant halo recomputation, copies
  into and out of per-thread buffers) that the sequential one does not.
- Wall clock vs CPU time confusion.

---

## 2. The measurements to take, cheapest and most informative first

### 2.1 The scaling curve + Karp–Flatt table — do this before anything else

Run at `N = 1, 2, 3, 4, 6, 8, 10, 12, 16` (and 20, 24 — oversubscribing is
diagnostic). Fixed input, 3+ repetitions, report the **median**, discard a
warm-up run. Same binary throughout; the N=1 run should be the *parallel* code
with one thread, and you should *also* have the true sequential time to see the
parallelization overhead at N=1.

Produce:

| N | time | S(N) | E(N) | e(N) Karp–Flatt |
|---|---|---|---|---|

Read it:

- **Plateaus at ~3–4 and stays flat** → shared resource saturated. Go to 2.2.
- **Rises linearly then bends gradually** → Amdahl serial fraction. Go to 2.4.
- **Peaks then *declines*** → contention/oversubscription. Go to 2.5/2.6.
- **Never even reaches 2x at N=2** → something is serializing at the root; a
  global lock or the whole workload is one serial stage.

This one experiment, which costs an afternoon at most, eliminates 80% of the
hypothesis space. Everything below is conditional on its shape.

### 2.2 Roofline: measure arithmetic intensity against the bandwidth ceiling

**Step 1 — find the machine's ceiling.** Run STREAM triad scaled across thread
counts, or `mlc --max_bandwidth` (Intel MLC), or
`likwid-bench -t load -w S0:1GB:16`. You get a GB/s number *and* the thread
count at which it saturates. If STREAM itself saturates at 4 threads, no
memory-bound code of yours will beat 4-thread scaling. That is your answer.

**Step 2 — find your pipeline's actual traffic.**

```sh
# DRAM traffic via the memory controller (Intel; names vary by uarch)
perf stat -a -e uncore_imc/data_reads/,uncore_imc/data_writes/ -- ./pipeline
# or, portable and easier:
likwid-perfctr -C 0-15 -g MEM_DP ./pipeline     # prints GB/s directly
```

Compute `bytes/sec` at N=16 and compare to the STREAM ceiling. **If you are
within 80% of it, you are bandwidth-bound and thread tuning is a dead end.**

**Step 3 — arithmetic intensity.**

```sh
perf stat -e instructions,fp_arith_inst_retired.scalar_double,\
fp_arith_inst_retired.256b_packed_single ./pipeline
```
`AI = FLOPs / DRAM_bytes`. Below ~1 FLOP/byte on a typical machine means the
memory roof binds, and the only fix is reducing traffic (Section 4).

### 2.3 Top-down stall breakdown at N=1 vs N=16

```sh
perf stat -d -d -d ./pipeline
perf stat --topdown --td-level 2 ./pipeline     # or: toplev.py -l3 ./pipeline
```

Compare IPC, `cache-misses`, and the topdown categories between 1 and 16
threads. The *change* is the signal:

- Memory-bound share jumps, IPC collapses → bandwidth or LLC capacity (1.1/1.11).
- Backend-bound flat but wall time up → synchronization/idle, not memory.
- `cache-misses` per instruction rises superlinearly → false sharing or LLC
  thrash; disambiguate with 2.6.

### 2.4 Decompose the pipeline: time every stage separately

Instrument each stage with `std::chrono::steady_clock` and record per-stage,
per-thread durations. Then build the table that actually matters:

| stage | serial time | parallel time @16 | stage speedup | % of parallel wall time |
|---|---|---|---|---|

Sort by "% of parallel wall time". Amdahl's real lesson is that **the stage
that refused to speed up now dominates**, and it is usually one you did not
parallelize (decode, encode, I/O, the final merge). A stage at 5% of sequential
time that got no speedup is 44% of your runtime at 16x on everything else.

Also measure, explicitly and separately:
- Time inside barriers/joins (`t_barrier_exit - t_thread_done` per thread).
- Time from `t_start` to the first worker doing useful work (pool spin-up).
- Time after the last worker finishes (merge/teardown).

### 2.5 Load imbalance and the timeline

Per work item, log `(thread_id, item_id, t_start, t_end)` into a
preallocated per-thread vector (no locks, no allocation in the hot path), dump
at exit, and compute:

```
imbalance = max(thread_busy_time) / mean(thread_busy_time)
idle_fraction = 1 - sum(thread_busy) / (N * wall_time)
```

`imbalance` is a direct upper bound on your speedup: `S <= N / imbalance`.

Then *look* at it. Load the trace into Perfetto / Chrome tracing
(`chrome://tracing`), or instrument with Tracy, Intel VTune's timeline, or
`perf sched`. Gaps, stragglers, staircase patterns and serial ramps are obvious
visually and nearly invisible in aggregate numbers. Test the hypothesis by
switching static → dynamic/guided scheduling with small chunks; if the time
drops, it was imbalance.

### 2.6 False sharing

```sh
perf c2c record ./pipeline && perf c2c report --stdio
```
Look for HITM (cache-line transfers from another core's modified line). It
reports the exact cache line, the offsets, and the source line of the
conflicting accesses — the most direct tool that exists for this. Cheaper
proxy:

```sh
perf stat -e mem_load_l3_hit_retired.xsnp_hitm,\
mem_load_l3_hit_retired.xsnp_hit ./pipeline
```
Rising HITM with N is conclusive. Confirm by padding suspected shared structures
to 64 bytes (`alignas(std::hardware_destructive_interference_size)`) or moving
accumulators into thread-local variables and merging once at the end; if the
time drops, that was it.

### 2.7 Lock contention and off-CPU time

If threads are *blocked* rather than *slow*, CPU profilers show nothing. You
need off-CPU analysis:

```sh
perf stat -e 'syscalls:sys_enter_futex' ./pipeline     # cheap smoke test
perf record -e sched:sched_switch -g ./pipeline        # who blocks, and where
bpftrace -e 'kprobe:futex_wait { @[ustack] = count(); }'
# or offcputime-bpfcc -p <pid> 10
perf lock record ./pipeline && perf lock report
```
Millions of futex waits means real mutex contention. Also check
`perf stat -e context-switches,cpu-migrations` — high migration counts point to
scheduler churn, which pinning fixes.

### 2.8 Verify your topology assumptions

```sh
lscpu                      # sockets, cores per socket, threads per core
lscpu --extended           # per-CPU core id, and MAXMHZ (hybrid cores differ)
cat /sys/devices/system/cpu/cpu*/topology/thread_siblings_list
numactl --hardware
```
Then re-run the scaling curve **pinned to physical cores only** (one thread per
core, no siblings):

```sh
taskset -c 0,2,4,6,8,10,12,14 ./pipeline      # if siblings are N, N+1 pairs
# or: OMP_PLACES=cores OMP_PROC_BIND=close
# or: likwid-pin -c E:N:8:1:2 ./pipeline
```
If 8 pinned physical cores nearly match 16 unpinned threads, SMT was giving you
nothing and your real target is ~8x, not ~16x.

### 2.9 Frequency and power

```sh
turbostat --interval 1 -- ./pipeline
```
Record Bzy_MHz at 1 thread vs 16 threads, plus `PkgWatt` and the throttle
columns. Normalize: `ideal_speedup = N * (f_allcore / f_1core)`. Also check
`grep MHz /proc/cpuinfo` during the run and
`/sys/devices/system/cpu/cpu0/thermal_throttle/*`. Run long enough to reach
steady state (≥60 s) so PL1 throttling is included.

### 2.10 Allocation, page faults, and NUMA

```sh
perf stat -e page-faults,minor-faults,major-faults ./pipeline
strace -c -f ./pipeline 2>&1 | head -30      # mmap/munmap/brk/futex counts
numastat -p <pid>
perf stat -e node-loads,node-load-misses ./pipeline
```
High minor-fault counts scaling with N → buffers are being reallocated per work
item; preallocate and reuse. Then A/B the allocator, which is a one-line test
and occasionally a large win:

```sh
LD_PRELOAD=/usr/lib/libjemalloc.so ./pipeline
LD_PRELOAD=/usr/lib/libtcmalloc.so ./pipeline
```
For NUMA, compare `numactl --interleave=all ./pipeline` against
`numactl --cpunodebind=0 --membind=0 ./pipeline` at 8 threads. A big difference
means first-touch placement is wrong.

### 2.11 Rule out I/O

Preload every input into RAM, drop the output write (or write to `/dev/null` or
tmpfs), and re-run the scaling curve. If scaling suddenly improves, your limit
is storage, not cores. Cross-check with `iostat -x 1` and
`perf stat -e block:block_rq_issue`.

### 2.12 Rule out measurement bugs

- Confirm sequential and parallel produce **bit-identical output** — a parallel
  version that skips work is "faster" for the wrong reason, and one that does
  extra work is slower for a reason you will never find in a profiler.
- Confirm identical compiler flags (`-O2/-O3`, `-march`) for both.
- Check that the sequential baseline is itself optimized and vectorized
  (`perf stat -e fp_arith_inst_retired.*`, or `-fopt-info-vec`). A poor
  baseline inflates speedup; a *well*-vectorized baseline that already
  saturates bandwidth on one core deflates it — and that is exactly the
  situation in which 3x is the honest ceiling.
- Report variance, not just the median. If run-to-run spread is ±30%, you are
  measuring the machine's mood.

---

## 3. A decision table

| Measurement | What it means | Where to go |
|---|---|---|
| `e(N)` roughly constant | True serial fraction | 2.4 stage table; parallelize or overlap the serial stage |
| `e(N)` rising | Contention or saturation | 2.2 bandwidth, then 2.6 false sharing, then 2.7 locks |
| DRAM GB/s ≥ 80% of STREAM | Bandwidth-bound | Section 4 — reduce traffic; extra cores cannot help |
| STREAM itself saturates at ~4 threads | Hardware ceiling ≈ your ceiling | Section 4, or a different machine/GPU |
| `imbalance` > 1.5 | Load imbalance | Dynamic scheduling, smaller chunks, work stealing |
| HITM high and rising with N | False sharing | Pad to 64 B; thread-local accumulators |
| Millions of futex waits | Lock contention | Shrink critical section, atomics, per-thread state |
| `threads per core = 2` in lscpu | Only 8 real cores | Recalibrate the target to ~8x |
| All-core MHz much below 1-core MHz | Frequency headroom | Normalize expectations; that part is physics |
| Minor faults scale with N | Per-item allocation | Preallocate/reuse buffers, arena allocator |
| `node-load-misses` high | NUMA misplacement | First-touch per worker, or interleave |
| Scaling improves with I/O removed | Storage-bound | Overlap I/O with compute (see below) |

---

## 4. If it is bandwidth (and it probably is), the fixes are algorithmic

Adding threads cannot raise the memory roof. Lower your demand instead:

1. **Fuse the stages.** Instead of N passes over the whole frame, apply the
   whole chain to one tile while it is resident in L2, then move to the next
   tile. This is the single biggest win available in multi-stage image
   pipelines — it converts N DRAM round-trips into one, and it raises
   arithmetic intensity by the same factor.
2. **Tile for cache, not for threads.** Pick tile sizes so a tile plus its halo
   plus the intermediate buffers fit in per-core L2 (typically 256 KiB–2 MiB).
   Then assign whole tiles to threads. Get the tile size from a sweep, not from
   theory.
3. **Shrink the data.** `uint8`/`uint16` instead of `float` where precision
   allows; 3 channels instead of 4 (or 4 for alignment if that vectorizes
   better — measure); avoid promoting to float and back per stage.
4. **Operate in place** where the algorithm permits, halving traffic.
5. **Avoid gratuitous copies** — into per-thread scratch, out of it, and at
   library boundaries. Pass views/spans, not owned buffers.
6. **Use streaming stores** (`_mm256_stream_si256`, non-temporal) for
   write-only output to avoid read-for-ownership traffic — up to 33% less
   traffic on pure-write passes.
7. **Overlap I/O with compute.** Decode frame `n+1` on dedicated threads while
   filtering frame `n`; a producer-consumer pipeline hides serial decode behind
   parallel compute and beats Amdahl's bound on that stage without making the
   stage itself parallel.
8. **Parallelize across frames, not within one frame.** Frame-level parallelism
   has near-zero synchronization, perfect load balance, and each frame's working
   set is independent. Bandwidth still binds, but the overhead disappears — and
   throughput, not per-frame latency, is usually what you actually want.
9. **Reconsider the target.** If the kernel is genuinely bandwidth-bound and
   already fused, a GPU (an order of magnitude more bandwidth) is the real
   answer, not more CPU threads.

---

## 5. Recommended order of work

1. Ten minutes: `lscpu` (2.8), `turbostat` (2.9), confirm identical outputs and
   flags (2.12). Recalibrate the target from 16x to whatever the hardware can
   actually give.
2. One afternoon: the scaling curve and Karp–Flatt table (2.1). This decides
   everything that follows.
3. Half a day: STREAM ceiling vs measured DRAM traffic (2.2) and the per-stage
   table (2.4). One of these two almost always contains the answer.
4. Only then: `perf c2c` (2.6), off-CPU/lock analysis (2.7), allocator and NUMA
   A/B tests (2.10) — each a targeted test of a specific named hypothesis, never
   a fishing expedition.
5. Fix the one dominant cause, re-run the scaling curve, and repeat. The curve
   is the scoreboard.

**The mindset that matters:** stop asking "why is it only 3x" and start asking
"what resource is at 100%". Speedup is a symptom; saturation is a cause. Every
measurement above exists to find the one resource — DRAM channels, a lock, a
cache line, one straggling thread, one serial stage — that is already full.
