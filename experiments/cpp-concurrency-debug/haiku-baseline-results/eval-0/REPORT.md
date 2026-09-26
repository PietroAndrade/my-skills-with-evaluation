# Haiku baseline (no skill) — debug eval-0, deadlock
Transcribed by the orchestrator: agent returned text only, wrote no files.

## Diagnosis
CORRECT: AB-BA lock-order inversion. A takes m_accounts then m_ledger; B the reverse.
Walked the 4-step interleaving.

## How I know
CORRECT and used the CPU discriminator: "0% CPU is the diagnostic fingerprint...
a busy-loop or livelock would consume CPU; blocked mutex waits do not."
Also used the intermittency as evidence of scheduler-dependent interleaving.
Did NOT reproduce empirically. Did NOT name gdb/helgrind/TSan lock-order tooling.

## Fix
Three options: (1) consistent lock order with two lock_guards, (2) std::lock with
defer_lock unique_locks, (3) single mutex if always accessed together.
Did NOT reach for std::scoped_lock (C++17, the one-liner) — offered the pre-C++17
std::lock + defer_lock form instead. Correct but dated.

## Non-fixes (6, with reasons)
recursive_mutex, lock timeouts, more threads, sleep between locks, volatile/atomic,
condition variables. All correct reasons.

## Slip
"Your code violates condition 4" — it SATISFIES the circular-wait condition.
Coffman conditions stated correctly otherwise.
