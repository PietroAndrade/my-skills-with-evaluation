#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

class Bank final
{
public:
    void transferToLedger(int amount)
    {
        std::lock_guard<std::mutex> accountsLock(m_accounts);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        std::lock_guard<std::mutex> ledgerLock(m_ledger);
        m_accountsTotal -= amount;
        m_ledgerTotal += amount;
    }

    bool reconcile()
    {
        std::lock_guard<std::mutex> ledgerLock(m_ledger);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        std::lock_guard<std::mutex> accountsLock(m_accounts);
        return m_accountsTotal + m_ledgerTotal == m_initialTotal;
    }

private:
    std::mutex m_accounts;
    std::mutex m_ledger;
    long m_accountsTotal = 1000000;
    long m_ledgerTotal = 0;
    long m_initialTotal = 1000000;
};

int main()
{
    Bank bank;
    std::thread worker_a([&bank] {
        for (int i = 0; i < 100000; ++i)
        {
            bank.transferToLedger(1);
        }
        std::puts("A finished");
    });

    std::thread worker_b([&bank] {
        for (int i = 0; i < 100000; ++i)
        {
            bank.reconcile();
        }
        std::puts("B finished");
    });

    worker_a.join();
    worker_b.join();
    std::puts("done");
    return 0;
}
