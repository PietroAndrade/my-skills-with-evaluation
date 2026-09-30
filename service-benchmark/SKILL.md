---
name: service-benchmark
description: Design a benchmark for a request/response service — DNS, HTTP, gRPC, a message broker, any protocol where a request gets a correlated response. Covers the choice that decides everything (closed-loop vs open-loop load generation), correlating responses to requests, interleaving A/B runs, reporting percentiles, and reading throughput curves to locate the bottleneck. Use this whenever the user wants to measure a running service rather than a function: "benchmark my API", "load test this server", "measure requests per second", "what's the throughput of X", "measure packet loss", "find the saturation point", "is the new version faster under load", "why does latency spike at high concurrency", "compare two versions of this service", "write a load generator". Reach for it even when the user only says "benchmark" but the thing under test is a process they talk to over a socket — the harness design differs enough from in-process timing that the usual advice quietly produces wrong numbers. For timing two implementations inside one binary, use cpp-parallel-benchmark instead.
---

## Goal

Produce numbers that answer the question actually being asked. For a service, the
harness shape decides which questions are answerable at all — a well-written
harness of the wrong shape returns confident, wrong answers rather than errors.

The dangerous failures here are silent. A load generator that self-throttles
reports zero loss from a service that is dropping most of its traffic. A harness
that ignores response correlation reports a smeared latency distribution that
looks like ordinary noise. Neither produces an error message.

Related: `cpp-parallel-benchmark` for timing two implementations inside one
process; `cpp-parallel-decompose` for predicting speedup before building.

---

## The choice that decides everything: closed-loop or open-loop

**Closed-loop.** N client threads; each sends a request and waits for its
response before sending the next. Concurrency is pinned at N, and the offered
rate is whatever the service permits.

Measures well: cost of a single request, latency at a chosen concurrency,
throughput ceiling.

**Cannot measure: loss, or any overload behaviour.** The client only sends after
the service replies, so it never offers more load than the service is already
absorbing. Under overload the requests queue and latency climbs — but nothing is
lost, because nothing was offered that could be lost. Loss comes back as zero
from a service that is collapsing.

**Open-loop.** A sender emits at a fixed rate regardless of responses; a separate
receiver collects whatever comes back. Loss is `sent − received` inside a drain
window.

Measures well: loss, the saturation knee, latency under a *specified* offered
load — which is what a real client population produces, since real clients do not
wait for your service before deciding to send.

Choosing: if the question is about what happens when the service *cannot keep
up* — loss, saturation, overload latency — it needs open-loop. If the question is
the cost of one request, closed-loop is simpler and adequate.

The trap worth naming: seeing `loss = 0%` from a closed-loop run and concluding
the service drops nothing. It means the harness never posed the question. Any
loss figure from a closed-loop harness should be reported as "not measured",
never as zero.

Open-loop needs two decisions stated in the report, because both change the
result: how long to drain after the send phase, and whether a response arriving
after its deadline counts as received or lost.

---

## Correlate responses to requests

Any protocol with more than one request in flight carries a correlation
identifier — DNS transaction id, HTTP/2 stream id, gRPC call id, a broker's
message id. Index send timestamps by it and match on arrival.

Skipping this is tempting because a naive harness appears to work: it sends,
reads the next thing off the socket, and calls that the response. The cost shows
up only under load. One slow response is credited to the following request, whose
own response is then credited to the next, and the error walks forward through
the run. The distribution widens in a way that reads as ordinary jitter, so
nothing looks wrong — the numbers are just quietly incorrect, and they get more
incorrect exactly when the system is most interesting.

---

## Interleave the variants

Comparing A against B: alternate A, B, A, B rather than running all of A and then
all of B.

A benchmark run takes minutes, and machines drift over minutes — CPU temperature
and clock throttling, background processes, page cache warmth. Run sequentially
by variant, every bit of that drift is attributed to whichever variant ran
second. Alternating cancels it to first order and costs nothing but ordering.

---

## Report the distribution, not just the mean

Mean, standard deviation and minimum are the right summary for a pure function.
For a service, add **p50 and p99**.

Two reasons. First, users of a service experience the tail; p99 is frequently the
number that decides whether the thing is usable, and a mean can hide it
completely. Second, mean and median *disagreeing* is itself a measurement:

- mean ≫ p50 → a minority of requests are far slower than typical. The service is
  usually fine and periodically stalls. That is a different defect, with a
  different fix, than being uniformly slow.
- mean moves between two versions but p50 does not → the change is in the tail,
  not the common path.
- p50 rises while the mean holds → the typical request got worse; something else
  got better enough to offset it.

Reporting only the mean makes all three indistinguishable.

---

## Reading the curves: diagnostics that need no profiler

Vary two knobs — client concurrency and workload cost — and the shape of the
response localises the bottleneck.

**Throughput flat as concurrency rises** → serialised. Added clients only queue.
The signature is unmistakable: throughput pinned while mean latency scales
linearly with concurrency (double the clients, double the latency).

**Throughput flat as workload cost changes** → the bottleneck is not the work.
If a cheap path and an expensive path sustain the same requests per second
despite genuinely different per-request work, the limit lies somewhere other than
the work itself. This is a strong signal and easy to collect: it needs only two
workloads and no instrumentation.

**Throughput peaks then falls as worker threads increase** → contention, not a
fixed serial section. Amdahl's Law cannot express negative scaling; a serial
fraction estimated from such data will rise with N, which is the model reporting
that it no longer applies. Quote the estimate at the optimum as an order of
magnitude and say plainly that the values past the peak are not interpretable.

**Latency flat across a wide load range, then a sharp knee** → healthy. The knee
is the saturation point, and it is the number to report as capacity — not the
absolute maximum, which is already past collapse.

---

## Isolate the path, pin the dependencies

A service normally has several paths with different costs: cache hit, cache miss,
forwarded to a backend, error. Driving a blend produces a number that describes no
real workload and moves whenever the blend moves. Drive each path deliberately,
then read component costs by difference — `miss − hit` is the cache-miss penalty,
`forwarded − hit` is the forwarding overhead.

Pin anything downstream that you are not trying to measure. A real upstream over
the internet contributes tens of milliseconds of variance that swamps whatever is
being compared; a deterministic local stand-in keeps the measurement about the
service under test. State this in the report — it is a genuine limitation, since
it removes real cost from the picture, not just noise.

Verify each path actually goes where intended before timing it. Asserting that
the cache-miss workload really misses is cheap; discovering afterwards that every
request was served from cache invalidates the run.

---

## Verify correctness before reporting speed

Confirm that the variants return the *same responses* before comparing their
speed. A version that is fast because it fails early, truncates, or skips work is
not faster in any useful sense.

For a service this is easy and worth automating: drive a fixed set of inputs
through each variant, record the responses, and diff. Do it before the timing
runs, so a mismatch stops the experiment instead of contaminating a conclusion.

---

## Workflow

1. State the question. Latency of one request, capacity, or overload behaviour —
   they need different harnesses.
2. Choose harness shape from the question. Overload or loss → open-loop.
3. Identify the correlation id and index send timestamps by it.
4. Enumerate the distinct code paths and build a workload that drives each one
   separately. Pin downstream dependencies.
5. Verify responses match across variants.
6. Warm up, discard the warm-up, then measure with variants interleaved.
7. Report mean, stddev, min, p50, p99 — plus loss and offered rate for open-loop.
8. Vary concurrency (and workers, if configurable) to locate the knee.
9. Write down the limitations, especially what the harness could not measure.

---

## Reporting template

```
## Method
harness shape (closed-loop N threads / open-loop R req/s), warm-up, rounds,
interleaving, workload per path, pinned dependencies

## Correctness
responses compared across variants before timing: pass/fail

## Results
per path and per concurrency: mean, stddev, min, p50, p99, throughput
open-loop additionally: offered rate, achieved rate, loss %

## Reading
what the numbers say, including any mean/p50 divergence and what the
throughput curves imply about the bottleneck

## Limitations
what was pinned or simulated, what the harness shape could not measure,
single-host effects, anything estimated rather than measured
```

Record enough to re-run it: commit of each variant, host CPU and core count,
build type, and the service's own configuration (thread/worker counts). A
throughput number without the worker count is not reproducible.

---

## Before returning a harness or a report

- Harness shape matches the question; loss is only claimed if the generator was open-loop.
- Responses correlated by protocol id, not by arrival order.
- Correctness compared across variants before any timing.
- Warm-up discarded; variants interleaved.
- p50 and p99 reported alongside mean, stddev and min.
- Each measured path driven separately and verified to reach that path.
- Limitations stated, including what the harness could not measure.

A worked example, including an open-loop generator and the path-isolation setup,
is in `references/harness-design.md`.
