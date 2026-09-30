#include "processor.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

bool Job::isValid() const
{
    return m_valid;
}

int main()
{
    Processor processor;

    processor.process(Job{false});

    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i)
    {
        workers.emplace_back([&processor] {
            for (int n = 0; n < 1000; ++n)
            {
                processor.process(Job{true});
            }
        });
    }

    for (std::thread& worker : workers)
    {
        worker.join();
    }

    std::printf("all workers finished\n");
    return 0;
}
