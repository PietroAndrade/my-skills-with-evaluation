#include "engine/ready_gate.h"

namespace engine {

void ReadyGate::wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this] { return m_ready; });
}

void ReadyGate::signalReady() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_ready) {
            return;
        }
        m_ready = true;
    }
    m_condition.notify_all();
}

bool ReadyGate::isReady() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ready;
}

}
