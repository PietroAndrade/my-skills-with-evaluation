# ReadyGate (baseline, Opus, no skill)

## Primitive chosen
std::mutex + std::condition_variable + sticky bool m_ready — hand-rolled one-shot latch (std::latch is C++20).

Rationale: sticky false->true state; mutex supplies happens-before edge; wait-with-predicate lets N waiters block without busy-wait; notify_all broadcasts. Semaphore rejected (permits are consumed, late waiter blocks). promise/shared_future rejected as heavier for one bool.

## Pitfalls avoided (self-reported)
- Lost wakeup: flag written under the same mutex the predicate is evaluated under.
- Spurious wakeup: predicate overload of wait.
- Late waiters: state sticky, not consumed.
- Repeated signalling: signalReady idempotent, early return.
- notify_all called after the scoped lock is released.
- RAII only (lock_guard/unique_lock).
- Copy ctor/assign deleted; m_mutex mutable for isReady() const.

## API
void wait(); void signalReady(); bool isReady() const.
Guard ENGINE_READY_GATE_H, namespace engine, class final, members m_mutex/m_condition/m_ready, no comments.

## Note
Compiles clean with g++ -std=c++17 -Wall -Wextra (per agent). Agent was blocked from writing this file itself; transcribed by the orchestrator.
