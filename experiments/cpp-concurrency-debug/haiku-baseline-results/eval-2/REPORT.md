# Haiku baseline (no skill) — debug eval-2, abandoned lock
Transcribed by the orchestrator: the agent returned text only and wrote no files.

## Diagnosis
Mutex never unlocked on the early-exit path. Thread acquires with .lock(), returns
without .unlock(), mutex stays locked forever, every later caller blocks in
m_mutex.lock(). CORRECT.

## How I know
Manual lock/unlock creates two exit paths; the added validation introduced an early
exit that skips the unlock. Explains "started after the check was added".
Did NOT use the CPU-usage discriminator. Did NOT mention the throwing-push_back path.
Did NOT reproduce empirically. Did NOT name gdb/helgrind/TSan confirmation.

## Fix
lock_guard (both variants offered: guard covering the whole body, and validate-before-lock).
CORRECT, matches the skill's preferred answer.

## Red flag
"Never use manual .lock()/.unlock()" + two failure modes (forgetting to unlock,
unlocking from the wrong thread). CORRECT.

## Non-fixes
Not addressed — the prompt asked for the red flag rather than non-fixes on this eval.
