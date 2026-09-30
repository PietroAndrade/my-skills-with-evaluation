---
name: cpp-producer-consumer
description: Guides the producer-consumer topology in C++: thread-safe blocking queue, I/O thread pushing to queue, N worker threads draining it, and coordinated shutdown. Use when creating a server, background worker service, or any component with I/O threads feeding worker threads. Trigger on: "producer consumer", "worker threads", "thread-safe queue", "background worker", "IO thread", "dispatch queue", "graceful shutdown threads", "blocking queue", "condition variable queue".
---

## Goal

Produce a correct, deadlock-free producer-consumer topology: one or more I/O threads push work onto a thread-safe blocking queue; N worker threads drain it; shutdown unblocks everything and joins cleanly. Read `docs/cpp-conventions.md` for naming conventions.

---

## Workflow

1. Ask what produces the messages (socket recv, file read, external callback, etc.)
2. Ask how many worker threads (default: 2, stored as `const size_t m_workerCount`)
3. Ask what each worker does with a message (dispatch, process inline, etc.)
4. Ask the lib name
5. Generate: `Queue` (header-only), server/service class (header + `.cpp`)

---

## Rules

### Thread-safe blocking queue

The queue is the core artefact. Three correctness properties must hold:

**1. `notify_one()` is called outside the lock.**

```cpp
// push
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_stopped) { return; }
    m_queue.push(std::move(item));
}
m_condition.notify_one();  // outside the lock
```

Calling `notify_one()` inside the lock causes the woken thread to immediately block trying to reacquire the mutex — wasted wake-up.

**2. `stop()` uses `notify_all()`**, not `notify_one()` — every waiting worker must wake up and see the stop signal.

```cpp
void stop()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stopped = true;
    }
    m_condition.notify_all();
}
```

**3. The `wait` predicate covers both cases**: item available OR stopped.

```cpp
MessagePtr pop()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_condition.wait(lk, [this] { return m_stopped || !m_queue.empty(); });

    if (m_queue.empty()) {
        return MessagePtr();  // nullptr = shutdown sentinel
    }

    MessagePtr item = std::move(m_queue.front());
    m_queue.pop();
    return item;
}
```

`pop()` returns `nullptr` when stopped and the queue is empty. Workers detect this and exit their loop — no separate stop flag needed in the worker.

**4. `push()` silently drops when stopped** — producers can keep calling push after stop without crashing.

```cpp
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_stopped) { return; }
    m_queue.push(std::move(item));
}
```

**5. Queue is non-copyable.** Delete copy constructor and copy-assignment — copying a mutex is undefined behavior.

```cpp
Queue(const Queue&) = delete;
Queue& operator=(const Queue&) = delete;
```

---

### Shutdown sequence — order matters

```cpp
~MyServer()
{
    m_stop.store(true);       // 1. signal loops to exit
    m_requestQueue.stop();    // 2. unblock all threads waiting on queue
    if (m_ioThread.joinable()) m_ioThread.join();   // 3. join I/O threads
    for (auto& t : m_workers) { if (t.joinable()) t.join(); }  // 4. join workers
}
```

**Step 2 must come before steps 3 and 4.** If you join before stopping the queue, workers blocked in `pop()` never wake up — deadlock.

For blocking-socket I/O threads: `m_stop.store(true)` alone is not enough — the thread is blocked in `receiveData()`. The I/O loop must check `m_stop` in the catch block after the socket throws on close:

```cpp
void ioLoop(ISocketClient& client)
{
    while (!m_stop.load()) {
        try {
            auto dto = client.receiveData();  // blocks
            m_requestQueue.push(std::make_shared<SocketDto>(std::move(dto)));
        } catch (const std::exception&) {
            if (m_stop.load()) { break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
```

---

### Worker loop

Workers exit when `pop()` returns `nullptr` (shutdown sentinel):

```cpp
void work()
{
    while (!m_stop.load()) {
        auto request = m_requestQueue.pop();
        if (!request) { break; }  // nullptr = queue stopped, exit

        try {
            m_dispatcher.dispatch(request);
        } catch (const std::exception& e) {
            logger::warning("[Server] dispatch error: {}", e.what());
        }
    }
}
```

---

### Thread members

```cpp
private:
    const std::size_t        m_workerCount{2};
    std::atomic<bool>        m_stop{false};
    Queue                    m_requestQueue;
    std::vector<std::thread> m_workers;
    std::thread              m_ioThread;
```

- `m_workerCount` as `const` member — not a `constexpr` at class scope because it may become a constructor parameter later
- `std::atomic<bool>` for `m_stop` — checked from multiple threads without a mutex
- `std::vector<std::thread>` for workers — `reserve(m_workerCount)` before `emplace_back` to avoid moves

---

### Dispatcher: lock-copy-release

When a dispatcher maps IDs to handlers, take the lock only long enough to copy the `shared_ptr`, then call the handler outside the lock. This prevents lock contention during processing.

```cpp
void Dispatcher::dispatch(const std::shared_ptr<Message>& msg)
{
    std::shared_ptr<IController> controller;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_controllers.find(msg->socketId);
        if (it == m_controllers.end()) {
            throw std::runtime_error("no controller for socketId");
        }
        controller = it->second;  // copy shared_ptr, release lock
    }
    controller->processRequest(msg);  // outside the lock
}
```

**Why:** `processRequest` may be slow (DNS resolution, file I/O). Holding the lock during processing would serialize all workers through one mutex.

---

## Variants

| Variant | I/O thread | When to use |
|---------|-----------|-------------|
| **SyncServer** | One blocking thread per socket (`receiveData()` blocks) | Few sockets, simpler code |
| **AsyncServer** | One epoll thread multiplexing N sockets | Many sockets, or sockets added/removed at runtime |

For the AsyncServer variant, the I/O thread runs an epoll scheduler and invokes a callback instead of pushing directly. See `cpp-wrapping-c-pure-posix` for the epoll event loop pattern.

---

## Full queue example

```cpp
#if !defined(<LIB>_QUEUE_H)
#define <LIB>_QUEUE_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace <lib>
{

using MessagePtr = std::shared_ptr<MessageType>;

class Queue final
{
public:
    Queue() = default;
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    MessagePtr pop();
    void push(MessagePtr&& item);
    void stop();

private:
    bool                    m_stopped{false};
    std::condition_variable m_condition;
    std::mutex              m_mutex;
    std::queue<MessagePtr>  m_queue;
};

inline MessagePtr Queue::pop()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_condition.wait(lk, [this] { return m_stopped || !m_queue.empty(); });

    if (m_queue.empty()) { return MessagePtr(); }

    MessagePtr item = std::move(m_queue.front());
    m_queue.pop();
    return item;
}

inline void Queue::push(MessagePtr&& item)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stopped) { return; }
        m_queue.push(std::move(item));
    }
    m_condition.notify_one();
}

inline void Queue::stop()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stopped = true;
    }
    m_condition.notify_all();
}

} // namespace <lib>

#endif /* <LIB>_QUEUE_H */
```
