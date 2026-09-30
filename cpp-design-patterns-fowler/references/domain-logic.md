# Domain Logic Patterns

Patterns that organize the core business logic of an enterprise application.

Source: https://martinfowler.com/eaaCatalog/

---

## Transaction Script

**Problem**: Business logic organized as a set of procedures where each procedure handles a single request from the presentation layer.

**When to Use**:
- Simple business logic with few interactions between transactions
- Teams unfamiliar with OO domain modeling
- Small applications or modules that won't grow significantly

**Structure**:
- Each business transaction maps to a single procedure/function
- Procedures call directly into the database layer
- No rich domain objects — data is just passed around as structs/DTOs

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

// Simple DB row structs (in real code: actual DB binding)
struct OrderRow    { int id; int customerId; double amount; bool isPaid; };
struct CustomerRow { int id; std::string name; double balance; };

// Simulated DB gateway (would be real SQL in production)
class OrderGateway {
public:
    std::vector<OrderRow> findUnpaidByCustomer(int customerId) {
        // SELECT * FROM orders WHERE customerId=? AND isPaid=0
        return {{ 1, customerId, 150.0, false }, { 2, customerId, 75.0, false }};
    }
    void markPaid(int orderId) {
        std::cout << "[DB] Order " << orderId << " marked as paid\n";
    }
};

class CustomerGateway {
public:
    CustomerRow find(int customerId) {
        return { customerId, "Alice", 500.0 };
    }
    void updateBalance(int customerId, double newBalance) {
        std::cout << "[DB] Customer " << customerId << " balance updated to " << newBalance << "\n";
    }
};

// Transaction Script — one function per use case
class OrderTransactions {
    OrderGateway    orderGw;
    CustomerGateway customerGw;
public:
    // Use case: pay all outstanding orders for a customer
    void payOutstandingOrders(int customerId) {
        CustomerRow customer = customerGw.find(customerId);
        auto unpaid = orderGw.findUnpaidByCustomer(customerId);

        double total = 0.0;
        for (const auto& order : unpaid)
            total += order.amount;

        if (customer.balance < total)
            throw std::runtime_error("Insufficient balance");

        for (const auto& order : unpaid)
            orderGw.markPaid(order.id);

        customerGw.updateBalance(customerId, customer.balance - total);
        std::cout << "Paid " << total << " for customer " << customer.name << "\n";
    }
};

int main() {
    OrderTransactions txn;
    txn.payOutstandingOrders(42);
}
```

**Pros**:
- Simple and straightforward — easy to understand
- Works well with procedural developers
- Minimal overhead for simple use cases

**Cons**:
- Duplication proliferates as logic grows
- Hard to reuse logic across transactions
- Doesn't scale to complex business rules

**Related**: Domain Model (richer OO alternative), Service Layer (organizes transactions behind a service boundary)

---

## Domain Model

**Problem**: Business rules and data are scattered or duplicated. As complexity grows, procedural scripts become tangled.

**When to Use**:
- Complex business logic with many interacting concepts
- Rules that vary by type or state
- Long-term maintainability matters more than initial simplicity

**Structure**:
- Rich objects that encapsulate both data and behavior
- Objects interact through well-defined interfaces
- Persistence handled separately (by a Data Mapper)

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <stdexcept>

class Order {
    int    id;
    double amount;
    bool   paid = false;
public:
    Order(int id, double amount) : id(id), amount(amount) {}

    int    getId()    const { return id; }
    double getAmount() const { return amount; }
    bool   isPaid()   const { return paid; }

    void pay() {
        if (paid) throw std::logic_error("Order already paid");
        paid = true;
        std::cout << "Order " << id << " paid\n";
    }
};

class Customer {
    int    id;
    std::string name;
    double balance;
    std::vector<Order> orders;
public:
    Customer(int id, std::string name, double balance)
        : id(id), name(std::move(name)), balance(balance) {}

    void addOrder(Order order) { orders.push_back(std::move(order)); }

    // Domain behavior lives here — not in a script
    void payOutstandingOrders() {
        double total = 0.0;
        for (const auto& o : orders)
            if (!o.isPaid()) total += o.getAmount();

        if (balance < total)
            throw std::runtime_error("Insufficient balance for " + name);

        for (auto& o : orders)
            if (!o.isPaid()) o.pay();

        balance -= total;
        std::cout << name << " balance after payment: " << balance << "\n";
    }

    const std::string& getName()   const { return name; }
    double             getBalance() const { return balance; }
};

int main() {
    Customer alice(1, "Alice", 500.0);
    alice.addOrder({ 1, 150.0 });
    alice.addOrder({ 2, 75.0 });
    alice.payOutstandingOrders();
}
```

**Pros**:
- Handles complex business rules cleanly
- High cohesion — logic lives close to data
- Easier to test individual domain concepts

**Cons**:
- Higher initial complexity
- Requires ORM or Data Mapper for persistence
- Steeper learning curve for procedural developers

**Related**: Transaction Script (simpler alternative), Data Mapper (persistence), Service Layer (coordinator)

---

## Table Module

**Problem**: You need to organize logic around database tables, but with more structure than Transaction Script.

**When to Use**:
- Database-centric applications where every object maps to a table
- Record-set based frameworks (e.g., ADO.NET DataSet-style)
- When a single class handles all logic for one table

**Structure**:
- One class per database table
- Operates on a record set (collection of rows) rather than single instances
- Sits between Transaction Script and Domain Model in complexity

**C++ Example**:
```cpp
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

// Simulates a record set (result of a DB query)
struct ContractRecord {
    int    id;
    int    productId;
    double revenue;
    int    year;
};

// Table Module — handles all business logic for the contracts table
class Contract {
    std::vector<ContractRecord> records;
public:
    explicit Contract(std::vector<ContractRecord> records)
        : records(std::move(records)) {}

    // Business method: total revenue for a given year
    double totalRevenueFor(int year) const {
        double total = 0.0;
        for (const auto& r : records)
            if (r.year == year) total += r.revenue;
        return total;
    }

    // Business method: contracts for a product
    std::vector<ContractRecord> byProduct(int productId) const {
        std::vector<ContractRecord> result;
        for (const auto& r : records)
            if (r.productId == productId) result.push_back(r);
        return result;
    }

    // Business method: recognize revenue (simplified)
    double recognizedRevenueAsOf(int contractId, int asOfYear) const {
        for (const auto& r : records)
            if (r.id == contractId && r.year <= asOfYear) return r.revenue;
        return 0.0;
    }
};

int main() {
    std::vector<ContractRecord> data = {
        { 1, 10, 1000.0, 2023 },
        { 2, 10, 2000.0, 2024 },
        { 3, 20, 500.0,  2023 },
    };

    Contract contracts(data);
    std::cout << "Revenue 2023: " << contracts.totalRevenueFor(2023) << "\n";
    std::cout << "Revenue 2024: " << contracts.totalRevenueFor(2024) << "\n";
    std::cout << "Recognized contract 1 as of 2023: "
              << contracts.recognizedRevenueAsOf(1, 2023) << "\n";
}
```

**Pros**:
- More structured than Transaction Script
- Works naturally with tabular data / record sets
- Single class per table is easy to navigate

**Cons**:
- Less flexible than Domain Model for complex interactions
- Hard to reuse logic that spans multiple tables

**Related**: Transaction Script (simpler), Domain Model (richer), Table Data Gateway (data access counterpart)

---

## Service Layer

**Problem**: Business logic is scattered across controllers, domain objects, or duplicated between different clients (web, API, batch).

**When to Use**:
- Multiple clients (web UI, REST API, CLI) need the same business operations
- You want to define a clear application boundary
- Coordinating multiple domain objects or external services in a single operation

**Structure**:
- Thin layer that defines the application's operations
- Delegates to domain objects or Transaction Scripts for actual logic
- Handles transactions, security, and cross-cutting concerns

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>

// Domain objects (simplified)
struct Order { int id; double amount; bool paid = false; };
struct Customer { int id; std::string name; double balance; };

// Repository interfaces (persistence abstraction)
class OrderRepository {
public:
    virtual ~OrderRepository() = default;
    virtual Order findById(int id) = 0;
    virtual void  save(const Order& order) = 0;
};

class CustomerRepository {
public:
    virtual ~CustomerRepository() = default;
    virtual Customer findById(int id) = 0;
    virtual void     save(const Customer& customer) = 0;
};

// Stub repositories for demo
class InMemoryOrderRepository : public OrderRepository {
public:
    Order findById(int id) override { return { id, 200.0, false }; }
    void  save(const Order& o) override {
        std::cout << "[DB] Order " << o.id << " saved (paid=" << o.paid << ")\n";
    }
};

class InMemoryCustomerRepository : public CustomerRepository {
public:
    Customer findById(int id) override { return { id, "Alice", 500.0 }; }
    void     save(const Customer& c) override {
        std::cout << "[DB] Customer " << c.id << " balance=" << c.balance << "\n";
    }
};

// Service Layer — application operations
class OrderService {
    std::shared_ptr<OrderRepository>    orderRepo;
    std::shared_ptr<CustomerRepository> customerRepo;
public:
    OrderService(std::shared_ptr<OrderRepository>    orderRepo,
                 std::shared_ptr<CustomerRepository> customerRepo)
        : orderRepo(std::move(orderRepo))
        , customerRepo(std::move(customerRepo)) {}

    // Application operation: pay an order
    void payOrder(int orderId, int customerId) {
        Order    order    = orderRepo->findById(orderId);
        Customer customer = customerRepo->findById(customerId);

        if (order.paid)
            throw std::logic_error("Order already paid");
        if (customer.balance < order.amount)
            throw std::runtime_error("Insufficient balance");

        order.paid         = true;
        customer.balance  -= order.amount;

        orderRepo->save(order);
        customerRepo->save(customer);

        std::cout << "Order " << orderId << " paid by " << customer.name << "\n";
    }
};

int main() {
    auto orderSvc = std::make_shared<OrderService>(
        std::make_shared<InMemoryOrderRepository>(),
        std::make_shared<InMemoryCustomerRepository>()
    );

    orderSvc->payOrder(1, 42);
}
```

**Pros**:
- Clear application boundary — easy to know what the app can do
- Reusable across different clients (REST, CLI, tests)
- Good place for transaction demarcation and security

**Cons**:
- Can become anemic if it just delegates to domain objects without value
- Risk of "fat service" anti-pattern if domain logic leaks here

**Related**: Domain Model (what the service coordinates), Repository (data access), Transaction Script (alternative for simple cases)
