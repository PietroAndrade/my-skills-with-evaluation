# Harness design

Skeletons for both load-generation shapes, the correlation table, and a worked
example. Read this when writing the generator; `SKILL.md` covers the decisions.

## Contents

- [Closed-loop generator](#closed-loop-generator)
- [Open-loop generator](#open-loop-generator)
- [Correlation identifiers by protocol](#correlation-identifiers-by-protocol)
- [Statistics to compute](#statistics-to-compute)
- [Path isolation](#path-isolation)
- [Worked example: DNS proxy, two binaries](#worked-example-dns-proxy-two-binaries)

---

## Closed-loop generator

N threads, each with its own connection and its own correlation-id range so ids
never collide between threads. Every thread sends, waits, records, repeats.

```cpp
struct Outcome {
    std::vector<long> rttsUs;
    int timeouts = 0;
};

void worker(const Target &target, int warmup, int runs,
            uint16_t idBase, Outcome *out) {
    Connection conn = connect(target);

    for (int i = 0; i < warmup; ++i) {
        exchange(conn, requestFor(i), idBase + i);
    }

    uint16_t id = idBase + warmup;
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::steady_clock::now();
        auto reply = exchange(conn, requestFor(i), id++);
        auto end = std::chrono::steady_clock::now();

        if (reply.timedOut) ++out->timeouts;
        else out->rttsUs.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
}
```

Throughput is `runs * threads / wallSeconds`. It is the rate the service
permitted, not a rate you chose — which is exactly why this shape cannot
measure loss.

`exchange` must not accept the first thing off the socket. It reads until the
correlation id matches, bounded by a retry count:

```cpp
Reply exchange(Connection &conn, const Request &req, uint16_t id) {
    send(conn, encode(req, id));
    for (int attempt = 0; attempt < kMaxStaleReplies; ++attempt) {
        auto raw = receive(conn);
        if (raw.empty()) return Reply::timeout();
        if (correlationId(raw) == id) return decode(raw);
    }
    return Reply::timeout();
}
```

## Open-loop generator

One sender pacing against a schedule, one receiver draining. The sender never
waits for a reply, so the offered rate is the rate you asked for.

```cpp
Result openLoop(const Target &target, int ratePerSec, int durationSec,
                int drainMs) {
    Connection conn = connect(target);
    const int planned = ratePerSec * durationSec;

    std::vector<long> sendTimeUs(kIdSpace, -1);
    std::vector<long> rttsUs;
    std::atomic<bool> senderDone{false};
    auto start = std::chrono::steady_clock::now();
    auto sinceStart = [&] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
    };

    std::thread receiver([&] {
        while (true) {
            auto raw = receive(conn);
            if (!raw.empty()) {
                uint16_t id = correlationId(raw);
                if (sendTimeUs[id] >= 0) {
                    rttsUs.push_back(sinceStart() - sendTimeUs[id]);
                    sendTimeUs[id] = -1;
                }
            }
            if (senderDone.load() &&
                sinceStart() > durationSec * 1000000L + drainMs * 1000L) break;
        }
    });

    const double intervalUs = 1000000.0 / ratePerSec;
    int sent = 0;
    for (int i = 0; i < planned; ++i) {
        while (sinceStart() < static_cast<long>(i * intervalUs)) {
            std::this_thread::yield();
        }
        uint16_t id = static_cast<uint16_t>(i % kIdSpace);
        sendTimeUs[id] = sinceStart();
        if (send(conn, encode(requestFor(i), id))) ++sent;
    }
    senderDone.store(true);
    receiver.join();

    return { ratePerSec, sent, static_cast<int>(rttsUs.size()),
             sent - static_cast<int>(rttsUs.size()), std::move(rttsUs) };
}
```

Three things to get right:

**The id space bounds the run.** With 16-bit ids, `rate * duration` must stay
under the space or ids wrap and late replies are matched to the wrong request.
Either cap the run or add a generation counter alongside the id.

**Achieved throughput divides by the send duration, not wall time.** Wall time
includes the drain, which would understate the rate.

**The drain window is a judgement call.** A reply arriving 5 seconds late is
useless to a real client, so counting it as received flatters the service.
State the window in the report.

## Correlation identifiers by protocol

| protocol | identifier | space |
|---|---|---|
| DNS | transaction id, first 2 bytes | 16 bits |
| HTTP/1.1 keep-alive | none — responses are ordered per connection | n/a |
| HTTP/2, HTTP/3 | stream id | 31 bits |
| gRPC | stream id of the underlying HTTP/2 | 31 bits |
| AMQP, MQTT | correlation-id / packet identifier | protocol-defined |
| custom RPC | whatever the framing carries | check the framing |

HTTP/1.1 is the exception: without pipelining there is at most one request in
flight per connection, so arrival order *is* the correlation. Concurrency comes
from more connections, and each needs its own timestamp slot.

## Statistics to compute

Per run, over successful requests only, with timeouts and losses reported
separately rather than folded in as some large latency:

| statistic | why |
|---|---|
| mean | overall level; sensitive to the tail |
| stddev | how noisy — a large value means the mean is not trustworthy |
| min | best case with no interference; the most stable comparison point |
| p50 | the typical request; compare against the mean to detect tail effects |
| p99 | what users notice; usually the number that decides usability |
| throughput | achieved requests per second |
| loss | open-loop only; "not measured" for closed-loop |

Percentiles need the sorted vector. Nearest-rank is fine at these sample sizes:
`sorted[static_cast<size_t>(p * (sorted.size() - 1))]`.

## Path isolation

Build one workload per path and confirm each reaches its path before timing.

| path | how to drive it | check |
|---|---|---|
| cache hit | small input set, repeated | second pass much faster than first |
| cache miss | unique inputs, more than the cache holds | latency stable across the run |
| backend forward | inputs the service cannot answer locally | backend shows the traffic |
| error | malformed or rejected inputs | error status returned |

For cache-miss, "more than the cache holds" matters: with a 2048-entry cache and
1000 unique inputs, the second pass is all hits and the run silently measures the
wrong path.

Costs then come out by subtraction — `miss − hit` is the lookup penalty,
`forward − hit` is the forwarding overhead. Each difference is only meaningful if
the two workloads differ in exactly one respect.

## Worked example: DNS proxy, two binaries

Comparing a rewritten DNS filter against the original. Both are separate
processes, so the harness is an external UDP client rather than in-process
timing.

Setup: both binaries in one container image selected by an environment variable;
a deterministic local resolver standing in for the upstream; a generated
classification database so the lookup path does real work; three workloads
(cache hit, cache miss, forwarded).

What each measurement step revealed:

**Closed-loop, one thread, per path.** The old binary's *mean* was identical to
three decimal places across all three workloads while its *median* tracked the
workload correctly. A cost independent of the work was dominating the mean. Mean
alone would have supported "the old one is 16x slower"; mean with p50 showed it
was usually comparable and periodically stalling — a different problem.

**Closed-loop, concurrency 1 → 32.** The old binary held ~226 req/s at every
concurrency, and the same ~226 req/s on both a cheap and an expensive workload,
while latency doubled with each doubling of concurrency. Throughput invariant to
both concurrency and workload: serialised. The new binary's throughput responded
to workload, which is what a work-bound service looks like.

**Open-loop, rising offered rate.** The closed-loop runs had reported 0% loss
everywhere, including at 128 ms mean latency — the harness could not pose the
question. Open-loop found the old binary losing a quarter of traffic at 500
req/s and all of it by 2000, against a new binary flat to 4000.

**Worker sweep.** Throughput peaked at 4 workers on one path and 8 on another,
then declined. The serial fraction implied by Amdahl rose with N, which is the
model signalling that growing contention — not a fixed serial section — is the
constraint.

The general lesson: each harness shape answered one class of question and was
silent on the others, and the silence did not look like an error.
