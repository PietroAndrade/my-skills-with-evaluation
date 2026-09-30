# Haiku WITH SKILL — debug eval-0, deadlock
Transcribed by the orchestrator.

## Diagnosis — CORRECT
Circular wait on two mutexes, opposite orders, 5-step interleaving, intermittency
explained as scheduler-dependent.

## Differential table (skill's Step 1) — present, and this is the visible skill artifact
Ruled out livelock (burns CPU), starvation (uneven but nonzero progress),
abandoned lock (affects only dependent threads), data race (wrong numbers, not hangs).
"0% CPU while frozen is the smoking gun."

## Fix — CORRECT and current
std::scoped_lock(m_accounts, m_ledger) in BOTH functions; explained the internal
deadlock-avoidance ordering. Alternative: documented global lock order.

## Non-fixes (5, with reasons)
recursive_mutex; naive try_lock retry -> LIVELOCK at 100% CPU (cites the skill's
"naive deadlock-recovery scheme"); random sleeps; shrinking critical sections
(reduces the window, does not remove the cycle); single super_mutex (safe but kills
concurrency).

## Grade
Correct. Haiku BASELINE was also correct on this prompt; differences are
(a) scoped_lock vs the older std::lock+defer_lock form, (b) the explicit
rule-out table, (c) try_lock->livelock named. Small lift, not a correctness lift.
