# Haiku WITH SKILL — debug eval-2, abandoned lock
Transcribed by the orchestrator.

## Diagnosis — CORRECT
Classified as "Abandoned Lock" by name, citing the skill's pattern catalogue.
Named the zero-CPU tell ("blocked, not spinning").
## Fix — CORRECT
std::scoped_lock, RAII on every exit path incl. exception.
## Red flag — CORRECT
Manual lock()/unlock() pair requires tracking every exit path.
## Grade
Correct. Haiku baseline was ALSO correct on this prompt -> zero lift.
Difference is only naming the pattern and reaching for scoped_lock (C++17) instead of
lock_guard; both fixes are valid.
