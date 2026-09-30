# C++17 fallbacks for C++20 synchronization types

`std::counting_semaphore`, `std::barrier`, and `std::latch` are C++20. On a C++17 target, generate these small classes built from `std::mutex` + `std::condition_variable`. They follow `docs/cpp-conventions.md` (namespace, `m_` members, guard, `final`, no comments). Replace `<lib>`/`<LIB>` with the module name.

The `<condition_variable>::wait` predicate form is used throughout so a spurious wake-up re-checks the condition instead of proceeding wrongly.

## Counting semaphore

```cpp
#ifndef <LIB>_COUNTING_SEMAPHORE_H
#define <LIB>_COUNTING_SEMAPHORE_H

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace <lib>
{

class CountingSemaphore final
{
public:
    explicit CountingSemaphore(std::size_t initialCount)
        : m_count(initialCount)
    {
    }

    CountingSemaphore(const CountingSemaphore&) = delete;
    CountingSemaphore& operator=(const CountingSemaphore&) = delete;

    void acquire()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_available.wait(lock, [this] { return m_count > 0; });
        --m_count;
    }

    bool tryAcquire()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_count == 0) { return false; }
        --m_count;
        return true;
    }

    void release()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_count;
        }
        m_available.notify_one();
    }

private:
    std::mutex              m_mutex;
    std::condition_variable m_available;
    std::size_t             m_count;
};

} // namespace <lib>

#endif /* <LIB>_COUNTING_SEMAPHORE_H */
```

Pair with an RAII guard so a permit is always returned, even on exception:

```cpp
class SemaphoreGuard final
{
public:
    explicit SemaphoreGuard(CountingSemaphore& semaphore)
        : m_semaphore(semaphore)
    {
        m_semaphore.acquire();
    }

    ~SemaphoreGuard()
    {
        m_semaphore.release();
    }

    SemaphoreGuard(const SemaphoreGuard&) = delete;
    SemaphoreGuard& operator=(const SemaphoreGuard&) = delete;

private:
    CountingSemaphore& m_semaphore;
};
```

## Latch (one-shot countdown)

```cpp
#ifndef <LIB>_LATCH_H
#define <LIB>_LATCH_H

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace <lib>
{

class Latch final
{
public:
    explicit Latch(std::size_t count)
        : m_count(count)
    {
    }

    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    void countDown()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_count > 0 && --m_count == 0) { m_released.notify_all(); }
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_released.wait(lock, [this] { return m_count == 0; });
    }

private:
    std::mutex              m_mutex;
    std::condition_variable m_released;
    std::size_t             m_count;
};

} // namespace <lib>

#endif /* <LIB>_LATCH_H */
```

## Barrier (reusable rendezvous)

The generation counter is what makes it reusable: threads released in one phase don't fall straight through the next `wait`.

```cpp
#ifndef <LIB>_BARRIER_H
#define <LIB>_BARRIER_H

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace <lib>
{

class Barrier final
{
public:
    explicit Barrier(std::size_t threadCount)
        : m_threshold(threadCount)
        , m_waiting(threadCount)
        , m_generation(0)
    {
    }

    Barrier(const Barrier&) = delete;
    Barrier& operator=(const Barrier&) = delete;

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const std::size_t currentGeneration = m_generation;
        if (--m_waiting == 0)
        {
            ++m_generation;
            m_waiting = m_threshold;
            m_allArrived.notify_all();
        }
        else
        {
            m_allArrived.wait(lock, [this, currentGeneration] { return currentGeneration != m_generation; });
        }
    }

private:
    std::mutex              m_mutex;
    std::condition_variable m_allArrived;
    const std::size_t       m_threshold;
    std::size_t             m_waiting;
    std::size_t             m_generation;
};

} // namespace <lib>

#endif /* <LIB>_BARRIER_H */
```

When the target moves to C++20, delete these and switch to `std::counting_semaphore`, `std::latch`, `std::barrier` — the call sites (`acquire`/`release`, `countDown`/`wait`, `wait`) map directly.
