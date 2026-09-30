# Diagnosing Parallelization Bottlenecks: 3× Speedup on 16 Cores

## Executive Summary

Getting 3× speedup on a 16-core machine indicates your pipeline is effectively only 71% parallelizable (computed backward from Amdahl's Law). This gap between expected (16×) and actual (3×) speedup reveals a real bottleneck—either contention, granularity, load imbalance, or long critical paths. The measurements below will pinpoint which one.

---

## Step 0: Amdahl's Law Diagnosis

Your observed speedup of **3×** on **16 threads** implies:

```
3 = 1 / ((1 - P) + P/16)
Solving: P ≈ 0.71 (71% parallelizable)
```

**What this means:** Even with infinite cores, you'd never exceed ~3.4× speedup due to serial friction. But the skill framework says Amdahl assumes a *fixed* serial fraction—constant overhead whether running 2 or 16 threads. **If your speedup would rise and then fall as thread count increases, the model is broken**: contention is growing faster than parallelism can compensate, not a fixed serial fraction.

---

## Why You're Not Hitting 16×: Three Hypotheses

### 1. **Contention (Locks, Cache, Allocators)**
- **Signal:** Speedup peaks at 4–8 threads, then *flattens or declines*.
- **Image-processing pipelines** commonly suffer:
  - Shared output buffer (one thread per stripe writes to contiguous memory → cache-line ping-pong).
  - Global memory allocator under load (16 threads allocating/freeing → lock contention).
  - Synchronization at stage boundaries (you said "pipeline"—if stages run in sequence, barriers serialize).

### 2. **Granularity Mismatch (Too Fine-Grained)**
- **Signal:** Kernel launch / task creation overhead > actual image processing work.
- **Image pipelines** often split into per-row or per-tile tasks. If your tiles are too small:
  - Synchronization cost outweighs compute cost.
  - Poor utilization of SIMD (if the kernel can't fill a register before context-switching).

### 3. **Load Imbalance**
- **Signal:** Some threads idle while others process longer.
- **Image processing** is prone to this if:
  - Work is uneven (e.g., some regions have more edges to detect).
  - Static task assignment doesn't adapt to actual work distribution.

### 4. **Long Critical Path (Dependencies)**
- **Signal:** A sequential bottleneck you haven't parallelized (e.g., reading input, writing output, format conversion).
- The skill calls this **span**—the longest chain of dependent tasks. If your pipeline has a `read-process-write` flow and reads/writes are serial, that's your ceiling.

---

## Concrete Measurements to Take

### **Priority 1: Thread Count Sweep (Distinguishes Contention)**

Run your pipeline with N = 1, 2, 4, 8, 12, 16 threads on the same input. Plot speedup vs. thread count.

**What to expect and what it tells you:**

| Shape | Diagnosis |
|-------|-----------|
| Linear to 16 (S ≈ 16) | Lucky! Serial fraction is truly minimal. Look for other issues (I/O, algorithmic). |
| Linear to ~8, then flat | Amdahl at work—71% parallelizable is real. Check if reads/writes are serial. |
| Rises to 8, *then declines* | **Contention.** Lock waiting or cache ping-pong is growing faster than speedup. Reduce lock scope or add padding. |
| Rises to 4, then flat or drops | Contention sets in early; granularity or synchronization cost is too high. |

**Command template (pseudocode):**
```cpp
for (int nthreads : {1, 2, 4, 8, 12, 16}) {
  std::chrono::high_resolution_clock t0;
  run_pipeline(image, nthreads);
  double elapsed = ...;
  printf("N=%d: %.3f ms, speedup=%.2f\n", nthreads, elapsed, baseline / elapsed);
}
```

---

### **Priority 2: Profile Lock Contention**

If the thread-count curve declines after a peak, contention is the villain.

**Tools:**
- **perf** (Linux): `perf record -e lock:contention_begin -c 1 ./your_pipeline` → shows lock wait time.
- **Intel VTune**: Locks and Waits analysis → pinpoint which mutex is hot.
- **Manual inspection**: Add instrumentation around locks:
  ```cpp
  auto t0 = std::chrono::high_resolution_clock::now();
  mutex.lock();
  auto t_acquired = std::chrono::high_resolution_clock::now();
  // ... work ...
  mutex.unlock();
  total_lock_wait_ms += (t_acquired - t0).count() / 1e6;
  ```
  Sum across all threads; if > 5–10% of total time, locks are significant.

---

### **Priority 3: Measure Cache Coherency**

Cache-line ping-pong (threads repeatedly invalidating each other's cache lines) is a silent killer in image pipelines.

**Tools:**
- **perf**: `perf stat -e cache-references,cache-misses,LLC-load-misses ...` → shows L3 cache misses and traffic.
- **Intel VTune**: Memory Access analysis → identify which memory regions are hot.
- **Manual check**: If your output buffer is one contiguous block and all 16 threads write to adjacent rows (strides), they thrash the same cache lines. **Fix:** pad each thread's output region or use thread-local buffers + combine at the end.

---

### **Priority 4: Profile Synchronization Barriers**

If your pipeline has stages (e.g., blur → sharpen → composite), and threads synchronize between stages:

```cpp
{ std::lock_guard<std::mutex> lock(barrier_mutex);
  barrier_count++;
  if (barrier_count == num_threads) {
    barrier_count = 0;
    barrier_cond.notify_all();  // 15 threads wake up serially...
  } else {
    barrier_cond.wait(lock);     // ... while 15 block.
  }
}
```

**Measurement:** Instrument the barrier. Time how long each thread waits. If significant, replace with `std::barrier` (C++20) or a lock-free design (e.g., a work-stealing queue so stages don't synchronously block).

---

### **Priority 5: Granularity and Load Balance**

Measure how much work each thread actually does.

**Approach:**
```cpp
std::vector<std::atomic<uint64_t>> work_done(num_threads);
#pragma omp parallel for
for (int i = 0; i < num_pixels; ++i) {
  work_done[omp_get_thread_num()]++;
  // ... process pixel i ...
}
// Check: are the counts roughly equal?
for (int i = 0; i < num_threads; ++i) {
  printf("Thread %d: %.1f%% of total work\n", 
         i, 100.0 * work_done[i] / total_pixels);
}
```

**Interpretation:**
- Counts within ±10% → load is balanced; granularity is probably fine.
- One thread >> others → static task assignment isn't matching reality; use dynamic scheduling (e.g., `omp schedule(dynamic, chunk_size)`).
- Counts vary wildly → either the problem is heterogeneous (some regions harder to process) or your decomposition is naive.

---

### **Priority 6: Isolate I/O**

Measure the time spent reading input and writing output separately.

```cpp
auto t0 = std::chrono::high_resolution_clock::now();
auto img = read_image(...);  // sequential
auto t1 = std::chrono::high_resolution_clock::now();
run_pipeline_threaded(img, nthreads);
auto t2 = std::chrono::high_resolution_clock::now();
write_image(result, ...);    // sequential
auto t3 = std::chrono::high_resolution_clock::now();

printf("Read: %.1f ms\n", (t1-t0).count() / 1e6);
printf("Process: %.1f ms\n", (t2-t1).count() / 1e6);
printf("Write: %.1f ms\n", (t3-t2).count() / 1e6);
```

If read + write > 25% of total time, they're your Amdahl brake. Parallelize them (e.g., stream reading from disk into processing, overlap I/O with compute).

---

## Diagnostic Decision Tree

1. **Run the thread-count sweep.** Does speedup decline after a peak?
   - **Yes** → Contention. Measure with perf or VTune; look for hot locks and cache misses.
   - **No** → Speedup is monotonic; the 71% serial fraction is structural.

2. **If monotonic**: Is the serial fraction in I/O, initialization, or a sequential algorithm step?
   - Run the Priority 6 measurement above.
   - Check whether stages run sequentially (e.g., `blur.wait_for(process); sharpen.start()`).

3. **If contention is the culprit**: 
   - Is it a lock or cache? Measure Priority 2 and 3.
   - Lock → reduce critical section or use lock-free structures.
   - Cache → add padding, use thread-local buffers, reduce shared state.

4. **If load imbalance**: Use dynamic scheduling (OpenMP `schedule(dynamic)` or a work-stealing queue).

---

## Expected Speedup After Fixes

Once you identify and fix the bottleneck, re-run the thread-count sweep. A healthy parallelization should see:
- Speedup linear to 12–14 threads (approaching 80–90% parallel).
- A modest drop at 16 (NUMA effects or a small true serial fraction).
- Speedup ≥ 10× for a well-tuned image pipeline on 16 cores.

If you hit 10–12× speedup, Amdahl predicts you're ~85–90% parallel—reasonable for image work. Beyond 12×, you're hitting diminishing returns; focus on single-threaded optimization.

---

## Actionable Next Steps

1. **This week:** Run the thread-count sweep (Priority 1). This takes 30 minutes and gives you the shape of the problem.
2. **If declining:** Measure contention (Priority 2, VTune). Lock hotspots are easy to fix (reduce critical section, pad allocations).
3. **If flat after 8 threads:** Check synchronization barriers and I/O (Priority 4, 6). Barriers are often the culprit in image pipelines.
4. **Once fixed:** Validate with `cpp-parallel-benchmark` (the skill references this)—compare the new speedup curve against your estimate.

---

## References

- **Amdahl's Law:** When observed speedup flattens or declines, distinguish fixed serial fraction (Amdahl) from growing contention (not Amdahl; requires architectural change).
- **Task Graph:** If you have a dependency diagram (blur → sharpen → composite), map the critical path. A long sequential chain of stages will limit speedup regardless of core count.
- **Granularity:** Image tiles should be large enough to amortize thread overhead—typically 64×64 or 128×128 pixels per task on modern CPUs.
- **Synchronization:** Barriers between stages are common in pipelines. Use lock-free `std::barrier` (C++20) or work-stealing to avoid serialization.
