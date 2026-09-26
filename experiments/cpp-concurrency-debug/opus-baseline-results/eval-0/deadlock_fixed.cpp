#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

class Bank final
{
public:
    void transferToLedger(int amount)
    {
        std::scoped_lock lock(m_accounts, m_ledger);
        m_accountsTotal -= amount;
        m_ledgerTotal += amount;
    }

    bool reconcile()
    {
        std::scoped_lock lock(m_accounts, m_ledger);
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
    bool reconciled = true;

    std::thread worker_a([&bank] {
        for (int i = 0; i < 200000; ++i)
        {
            bank.transferToLedger(1);
        }
    });

    std::thread worker_b([&bank, &reconciled] {
        for (int i = 0; i < 200000; ++i)
        {
            reconciled = bank.reconcile() && reconciled;
        }
    });

    worker_a.join();
    worker_b.join();
    std::printf("done, invariant held: %s\n", reconciled ? "yes" : "no");
    return 0;
}
