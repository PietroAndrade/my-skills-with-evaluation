#ifndef ENGINE_READY_GATE_H
#define ENGINE_READY_GATE_H

#include <condition_variable>
#include <mutex>

namespace engine {

class ReadyGate final {
public:
    ReadyGate() = default;

    ReadyGate(const ReadyGate&) = delete;
    ReadyGate& operator=(const ReadyGate&) = delete;

    void wait();

    void signalReady();

    bool isReady() const;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_ready = false;
};

}

#endif
