#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <mutex>
#include <vector>

class Job
{
public:
    bool isValid() const;

    bool m_valid{true};
};

class Processor
{
public:
    void process(const Job& job);

private:
    std::mutex m_mutex;
    std::vector<Job> m_queue;
};

#endif
