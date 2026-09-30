# Haiku WITH SKILL — debug eval-1, non-deterministic result
Transcribed by the orchestrator.

## Diagnosis — CORRECT
"This is a race condition, not a data race. ThreadSanitizer reports nothing because
there is no data race — every memory access is properly synchronized. The bug is in
the logic, not the synchronization."
Explained why more mutexes cannot help (problem is order, not corruption).
Explicitly cited the skill's data-race vs race-condition table.

## Fix
Option 1: barrier/latch (C++20 std::barrier, C++17 condvar latch) -- this is the
  KNOWN SKILL DEFECT: buys determinism by serializing.
Option 2: compute locally, join, combine single-threaded -- CORRECT, and it is the
  same shape as the Opus baseline's per-thread-partials answer.
Option 3: serialize through a queue -- also serializing.

## Missed
FP associativity mechanism absent. No discriminator (low-bit jitter vs torn value).
No repro. Example used "counter could be 1, 2 or 3" which is a poor illustration.

## Grade
CORRECT classification and correct fix available (option 2), ranked above nothing wrong.
Contrast with the Haiku BASELINE, which led with "unprotected final read" (a data race,
contradicting the TSan evidence) and certified `result += value` as safe.
