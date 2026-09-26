# Freeze at 0% CPU with two mutexes: lock-order inversion (AB-BA deadlock)

## Verdict

Your two threads acquire `m_accounts` and `m_ledger` in **opposite orders**.

- Thread A (transfer): locks `m_accounts`, then wants `m_ledger`.
- Thread B (reconcile): locks `m_ledger`, then wants `m_accounts`.

If the scheduler interleaves them so that each thread takes its first mutex before
either takes its second, both block forever, each holding the mutex the other is
waiting for. That is a classic AB-BA deadlock. Nothing in the program can break
it, which is why it "never recovers".

This explains every symptom in the report, including the ones that rule out other
causes (see *Why this and not something else*).

## How I know

### 1. The symptom signature points at blocking, not spinning

`top` showing ~0% CPU is the decisive clue. The threads are parked in the kernel,
not burning cycles. Reproduced locally (`deadlock_repro.cpp`, built with
`g++ -std=c++17 -g -O0 -pthread`), the hung process looks like this:

```
tid 2053686 state=S wchan=futex_do_wait
tid 2053687 state=S wchan=futex_do_wait
tid 2053688 state=S wchan=futex_do_wait
utime=0 stime=0          # zero CPU consumed, 3 seconds after the hang
```

State `S` + `futex_do_wait` + zero accumulated CPU time = every thread is blocked
on a futex (what `std::mutex` uses underneath) and no thread is making progress.

### 2. "Not on every run" is exactly the shape of a lock-order inversion

The deadlock needs a specific interleaving: A must grab `m_accounts` and lose the
CPU (or simply not reach `m_ledger` yet) while B grabs `m_ledger`. The window is
narrow, so the hang is timing-dependent — it shows up after a few seconds of
iterations, on some runs and not others, and more often on a loaded machine or a
release build (tighter loops, more iterations per second, different scheduling).
A deterministic bug would hang on run 1 every time.

### 3. Confirm it in your own process in under a minute

```sh
# The authoritative check: where is each thread blocked?
gdb -p $(pgrep your_program) -batch -ex 'thread apply all bt'
```

You are looking for two threads both inside `__lll_lock_wait` /
`pthread_mutex_lock` under `std::mutex::lock`, with the *frames above* showing
`transfer(...)` in one and `reconcile(...)` in the other — and each of those
functions already holding a `lock_guard` on the other mutex. That backtrace pair
*is* the proof: it names both the two blocked threads and the two mutexes, in the
two orders.

If ptrace is blocked in your environment (it was in mine), the poor-man's version
above works: `cat /proc/<pid>/task/*/wchan` and `/proc/<pid>/stat` fields 14/15.

Tooling that finds it *before* it hangs:

- `valgrind --tool=helgrind ./prog` — reports "lock order violated" on the first
  inverted acquisition, even on runs that do not deadlock. This is the best tool
  for this specific bug.
- `g++ -fsanitize=thread` — TSan's deadlock detector also flags lock-order
  inversions (`ThreadSanitizer: lock-order-inversion (potential deadlock)`).
- Grep-level audit: every place that holds one of the two mutexes while taking the
  other. There should be exactly one order across the whole program.

## The fix

Two acceptable options. Both are in `deadlock_fixed.cpp`.

### Option 1 (preferred): let the library order the locks — `std::scoped_lock`

```cpp
void transferToLedger(int amount)
{
    std::scoped_lock lock(m_accounts, m_ledger);
    m_accountsTotal -= amount;
    m_ledgerTotal  += amount;
}

bool reconcile()
{
    std::scoped_lock lock(m_accounts, m_ledger);
    return m_accountsTotal + m_ledgerTotal == m_initialTotal;
}
```

`std::scoped_lock` (C++17; `std::lock` pre-17) acquires multiple mutexes with a
deadlock-avoidance algorithm: it does not blindly block on the second mutex, it
try-locks and backs off, releasing what it holds. It is therefore **safe even if
two call sites pass the mutexes in different orders** — which is what makes it the
robust choice for code that will be edited by someone who does not know the rule.

Verified: the fixed binary runs 200,000 iterations on each thread and exits
cleanly, `invariant held: yes`. The buggy binary hangs and is killed by
`timeout 5` (exit 124).

### Option 2: impose and document a global lock order

Pick one canonical order — `m_accounts` before `m_ledger`, always — and change
`reconcile()` to follow it:

```cpp
std::lock_guard<std::mutex> accountsLock(m_accounts);
std::lock_guard<std::mutex> ledgerLock(m_ledger);
```

Cheaper at runtime (no try/backoff), but it is a convention, not a guarantee: the
next function that takes them the other way reintroduces the bug. If you go this
way, write the order down next to the member declarations and enforce it in review.

### Worth considering as well

Since both operations need a consistent view of *both* resources, the two-mutex
split may be buying you nothing. If no code path ever touches only one of them,
one mutex covering both is simpler and strictly correct. Keep two only if there
are genuinely independent accounts-only and ledger-only hot paths.

## Which plausible-sounding fixes would NOT work

| Proposed fix | Why it fails |
|---|---|
| **Swap to `std::recursive_mutex`** | Recursive mutexes solve *self*-reentrancy (one thread locking the same mutex twice). Here two *different* threads each want a mutex the other holds. The ownership cycle is unchanged. Hangs identically. |
| **`std::unique_lock` instead of `std::lock_guard`** | Purely an RAII/flexibility difference. Same blocking acquisition, same order, same cycle. |
| **`std::this_thread::yield()` or a `sleep` between the two locks** | Changes the timing, so the hang gets rarer — which is worse, not better: it converts a reproducible-in-seconds bug into one that surfaces in production once a month. The cycle is still reachable. |
| **`std::atomic` on the balances, or `volatile`** | Addresses a *data race* on individual variables. Your bug is not a race on a value; it is a blocking cycle in lock acquisition, and the invariant spans two variables so atomics can't guarantee it anyway. Atomics on the data do not remove the two `lock()` calls. |
| **A `condition_variable` so threads "wait for the other to finish"** | A CV does not resolve a cycle, it adds one. The deadlock is in mutex acquisition; a thread parked in `cv.wait()` while holding the other mutex deadlocks the same way, plus you now risk a lost wakeup. (Note: a lost wakeup also shows 0% CPU — distinguish via the backtrace, which would show `condition_variable::wait` rather than `pthread_mutex_lock`.) |
| **Make B's locks `std::shared_lock` on `std::shared_mutex` since reconcile only reads** | Still deadlocks. A shared lock blocks while an exclusive lock is held. A holds `m_accounts` exclusively and waits for `m_ledger`; B holds `m_ledger` shared and waits to share `m_accounts` — blocked by A's exclusive hold. Same cycle, now harder to see. Reader-writer locks reduce contention, never fix ordering. |
| **Naive `try_lock` retry that keeps the first lock** | `lock(m_accounts); while (!m_ledger.try_lock()) {}` spins forever holding `m_accounts` — and now at **100% CPU**, so it no longer even matches your symptom. A correct retry must *release the first mutex* before retrying, which is precisely what `std::scoped_lock` already implements (and a hand-rolled version without backoff can livelock). |
| **Reduce to one worker thread, or lower a thread's priority** | Hides the bug by removing the concurrency you need. Priority changes only reshape the window; both threads still block, and priority cannot preempt mutex ownership. |
| **`timed_mutex` + `try_lock_for` and log on timeout** | A useful *detector* and a legitimate last-resort mitigation for third-party code you cannot change, but here it turns a hang into a silent failed transfer or an abandoned reconciliation. It treats the symptom; the ordering bug remains. |
| **Fixing the order in only one of the two functions you happened to look at** | Correct direction, incomplete. Any *other* call path that takes both mutexes (a helper, an audit routine, a destructor) must obey the same order. Audit all of them — helgrind will list the ones you missed. |

## Files

- `deadlock_repro.cpp` — minimal reproduction; hangs within a few seconds.
- `deadlock_fixed.cpp` — `std::scoped_lock` fix; completes and verifies the invariant.

```sh
g++ -std=c++17 -O1 -pthread -o repro deadlock_repro.cpp && timeout 5 ./repro   # exit 124 (hung)
g++ -std=c++17 -O2 -pthread -o fixed deadlock_fixed.cpp && ./fixed            # "invariant held: yes"
```
