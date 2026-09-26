# Object-Relational Behavioral Patterns

Patterns that manage how in-memory objects synchronize with a relational database.

Source: https://martinfowler.com/eaaCatalog/

---

## Unit of Work

**Problem**: Each individual database call is expensive. Multiple small writes during a business transaction are fragile and slow.

**When to Use**:
- Applications with a Data Mapper layer
- When you need to batch all DB changes from a single business transaction into one atomic commit
- To avoid partial updates if something fails mid-transaction

**Structure**:
- Tracks which objects were loaded, which were modified (dirty), and which are new or deleted
- At the end of a business transaction, `commit()` issues all DB changes in the right order
- Callers register objects with the Unit of Work rather than writing to DB directly

**C++ Example**:
```cpp
#include <iostream>
#include <vector>
#include <unordered_set>
#include <memory>
#include <string>

// Simplified domain object interface
class DomainObject {
public:
    virtual ~DomainObject() = default;
    virtual int         getId()  const = 0;
    virtual std::string getType() const = 0;
};

class Person : public DomainObject {
    int         id;
    std::string name;
public:
    Person(int id, std::string name) : id(id), name(std::move(name)) {}
    int         getId()   const override { return id; }
    std::string getType() const override { return "Person"; }
    const std::string& getName() const   { return name; }
};

// Unit of Work — accumulates changes, commits once
class UnitOfWork {
    std::vector<DomainObject*> newObjects;
    std::vector<DomainObject*> dirtyObjects;
    std::vector<DomainObject*> removedObjects;

    void insertIntoDb(DomainObject* obj) {
        std::cout << "[DB] INSERT " << obj->getType() << " id=" << obj->getId() << "\n";
    }
    void updateInDb(DomainObject* obj) {
        std::cout << "[DB] UPDATE " << obj->getType() << " id=" << obj->getId() << "\n";
    }
    void deleteFromDb(DomainObject* obj) {
        std::cout << "[DB] DELETE " << obj->getType() << " id=" << obj->getId() << "\n";
    }

public:
    void registerNew(DomainObject* obj) {
        newObjects.push_back(obj);
    }

    void registerDirty(DomainObject* obj) {
        // Avoid duplicates
        for (auto* o : dirtyObjects) if (o == obj) return;
        dirtyObjects.push_back(obj);
    }

    void registerRemoved(DomainObject* obj) {
        removedObjects.push_back(obj);
    }

    // Commit: flush all tracked changes in a single DB transaction
    void commit() {
        std::cout << "[UoW] BEGIN TRANSACTION\n";
        for (auto* obj : newObjects)     insertIntoDb(obj);
        for (auto* obj : dirtyObjects)   updateInDb(obj);
        for (auto* obj : removedObjects) deleteFromDb(obj);
        std::cout << "[UoW] COMMIT\n";
        newObjects.clear();
        dirtyObjects.clear();
        removedObjects.clear();
    }

    void rollback() {
        std::cout << "[UoW] ROLLBACK — discarding " << newObjects.size()
                  << " new, " << dirtyObjects.size() << " dirty, "
                  << removedObjects.size() << " removed\n";
        newObjects.clear();
        dirtyObjects.clear();
        removedObjects.clear();
    }
};

int main() {
    UnitOfWork uow;

    Person alice(1, "Alice");
    Person bob(2, "Bob");
    Person carol(3, "Carol");

    uow.registerNew(&alice);    // new → will INSERT
    uow.registerDirty(&bob);    // modified → will UPDATE
    uow.registerRemoved(&carol); // deleted → will DELETE

    uow.commit();
}
```

**Pros**:
- Reduces round-trips to DB — all writes batched
- Provides atomicity for a business transaction
- Decouples business logic from DB write timing

**Cons**:
- More bookkeeping complexity
- Must carefully track all changes
- Memory cost if many objects are loaded

**Related**: Data Mapper (uses Unit of Work to stage writes), Identity Map (companion — tracks loaded objects), Repository (sits above Unit of Work)

---

## Identity Map

**Problem**: Loading the same database row twice creates duplicate in-memory objects, leading to inconsistency.

**When to Use**:
- Any Data Mapper architecture
- When multiple parts of a business transaction may load the same row
- To prevent N+1 loads of the same object

**Structure**:
- A map from (type, id) → object is kept per business transaction
- Before loading from DB, check the map first
- After loading, store in the map

**C++ Example**:
```cpp
#include <iostream>
#include <unordered_map>
#include <memory>
#include <string>
#include <optional>

class Person {
    int         id;
    std::string name;
public:
    Person(int id, std::string name) : id(id), name(std::move(name)) {}
    int                getId()   const { return id; }
    const std::string& getName() const { return name; }
};

// Identity Map — one per transaction/request
class IdentityMap {
    std::unordered_map<int, std::shared_ptr<Person>> persons;
public:
    void put(std::shared_ptr<Person> person) {
        persons[person->getId()] = person;
    }

    std::shared_ptr<Person> get(int id) const {
        auto it = persons.find(id);
        return it != persons.end() ? it->second : nullptr;
    }
};

// Mapper that uses the Identity Map
class PersonMapper {
    IdentityMap& identityMap;

    std::shared_ptr<Person> loadFromDb(int id) {
        std::cout << "[DB] SELECT * FROM persons WHERE id=" << id << "\n";
        return std::make_shared<Person>(id, "Alice");
    }

public:
    explicit PersonMapper(IdentityMap& map) : identityMap(map) {}

    std::shared_ptr<Person> find(int id) {
        // 1. Check identity map first
        auto cached = identityMap.get(id);
        if (cached) {
            std::cout << "[IdentityMap] Cache hit for id=" << id << "\n";
            return cached;
        }
        // 2. Load from DB and register
        auto person = loadFromDb(id);
        identityMap.put(person);
        return person;
    }
};

int main() {
    IdentityMap  map;
    PersonMapper mapper(map);

    auto p1 = mapper.find(1); // → DB hit
    auto p2 = mapper.find(1); // → cache hit (same object!)
    auto p3 = mapper.find(2); // → DB hit

    std::cout << "Same object? " << (p1.get() == p2.get() ? "YES" : "NO") << "\n";
}
```

**Pros**:
- Prevents duplicate loads of the same row
- Ensures identity equality — `p1 == p2` for same DB row
- Reduces DB round-trips

**Cons**:
- Memory overhead — all loaded objects stay alive per transaction
- Must be scoped to a single transaction/request (not a global cache)

**Related**: Unit of Work (companion — tracks changes), Data Mapper (uses Identity Map), Lazy Load (complements by deferring loads)

---

## Lazy Load

**Problem**: Loading a complex object graph eagerly is expensive when you only need part of it.

**When to Use**:
- Loading a parent object shouldn't force loading all associated children
- When associations are large or expensive to load
- Performance optimization for complex object graphs

**Structure**: Four variants:
1. **Lazy Initialization**: field is null until first access
2. **Virtual Proxy**: a proxy object stands in, loads on first method call
3. **Value Holder**: wrapper that loads on `getValue()`
4. **Ghost**: partially loaded object that fills in on first field access

**C++ Example (Lazy Initialization)**:
```cpp
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <optional>

struct Address { std::string street; std::string city; };

class Supplier {
    int         id;
    std::string name;

    // Lazily loaded associations
    mutable std::optional<std::vector<Address>> addresses; // mutable: loaded in const method

    std::vector<Address> loadAddressesFromDb() const {
        std::cout << "[DB] SELECT * FROM addresses WHERE supplierId=" << id << "\n";
        return {
            { "123 Main St", "Springfield" },
            { "456 Oak Ave",  "Shelbyville" },
        };
    }

public:
    Supplier(int id, std::string name) : id(id), name(std::move(name)) {}

    const std::string& getName() const { return name; }

    const std::vector<Address>& getAddresses() const {
        if (!addresses.has_value()) {
            addresses = loadAddressesFromDb();
        }
        return *addresses;
    }
};

int main() {
    Supplier s(1, "ACME Corp");
    std::cout << "Supplier: " << s.getName() << "\n";
    std::cout << "(No DB call yet)\n";

    // DB called only here
    const auto& addrs = s.getAddresses();
    for (const auto& a : addrs)
        std::cout << "  " << a.street << ", " << a.city << "\n";

    // Second access — no DB call
    std::cout << "Second access, count=" << s.getAddresses().size() << "\n";
}
```

**C++ Example (Virtual Proxy)**:
```cpp
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Product { int id; std::string name; double price; };

// Real loader
class RealProductList {
    int supplierId;
public:
    explicit RealProductList(int sid) : supplierId(sid) {}
    void load() {
        std::cout << "[DB] SELECT * FROM products WHERE supplierId="
                  << supplierId << "\n";
    }
};

// Proxy that defers loading
class ProductListProxy {
    int  supplierId;
    mutable std::unique_ptr<RealProductList> inner;

    void ensureLoaded() const {
        if (!inner) {
            inner = std::make_unique<RealProductList>(supplierId);
            inner->load();
        }
    }
public:
    explicit ProductListProxy(int sid) : supplierId(sid) {}

    void accessProducts() const {
        ensureLoaded();
        std::cout << "Products available for supplier " << supplierId << "\n";
    }
};

int main() {
    ProductListProxy proxy(42);
    std::cout << "Proxy created — no DB call\n";
    proxy.accessProducts(); // → DB hit
    proxy.accessProducts(); // → no DB hit
}
```

**Pros**:
- Avoids loading data that isn't needed
- Reduces initial load time and memory for complex graphs

**Cons**:
- Adds complexity (proxies, state flags)
- Can cause "N+1 query" problems if used carelessly in loops

**Related**: Identity Map (prevents duplicate loads), Unit of Work (tracks loaded objects)

---

## Repository

**Problem**: Business logic contains raw query logic mixed in, making it hard to test and maintain.

**When to Use**:
- Domain Model architectures with Data Mapper
- When you want to query domain objects as if they were an in-memory collection
- To isolate domain code from query syntax (SQL, NoSQL, etc.)

**Structure**:
- Looks like a collection (`add`, `remove`, `find`) for each aggregate root
- Internally uses Data Mapper / query objects to translate to DB queries
- Business logic only sees domain objects — never raw SQL

**C++ Example**:
```cpp
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <stdexcept>

// Domain object
class Customer {
    int         id;
    std::string name;
    std::string city;
    double      creditLimit;
public:
    Customer(int id, std::string name, std::string city, double credit)
        : id(id), name(std::move(name)), city(std::move(city)), creditLimit(credit) {}

    int                getId()          const { return id; }
    const std::string& getName()        const { return name; }
    const std::string& getCity()        const { return city; }
    double             getCreditLimit() const { return creditLimit; }
};

// Repository interface — looks like a collection
class CustomerRepository {
public:
    virtual ~CustomerRepository() = default;
    virtual void                                  add(std::shared_ptr<Customer> customer) = 0;
    virtual void                                  remove(int id)                          = 0;
    virtual std::shared_ptr<Customer>             findById(int id)                        = 0;
    virtual std::vector<std::shared_ptr<Customer>> findByCity(const std::string& city)    = 0;
    virtual std::vector<std::shared_ptr<Customer>> findHighValue(double minCredit)        = 0;
};

// In-memory implementation (production would use Data Mapper + SQL)
class InMemoryCustomerRepository : public CustomerRepository {
    std::vector<std::shared_ptr<Customer>> store;
    int nextId = 1;
public:
    void add(std::shared_ptr<Customer> customer) override {
        std::cout << "[Repo] Adding customer: " << customer->getName() << "\n";
        store.push_back(std::move(customer));
    }

    void remove(int id) override {
        store.erase(std::remove_if(store.begin(), store.end(),
            [id](const auto& c) { return c->getId() == id; }), store.end());
        std::cout << "[Repo] Removed customer id=" << id << "\n";
    }

    std::shared_ptr<Customer> findById(int id) override {
        for (const auto& c : store)
            if (c->getId() == id) return c;
        return nullptr;
    }

    std::vector<std::shared_ptr<Customer>> findByCity(const std::string& city) override {
        std::vector<std::shared_ptr<Customer>> result;
        for (const auto& c : store)
            if (c->getCity() == city) result.push_back(c);
        return result;
    }

    std::vector<std::shared_ptr<Customer>> findHighValue(double minCredit) override {
        std::vector<std::shared_ptr<Customer>> result;
        for (const auto& c : store)
            if (c->getCreditLimit() >= minCredit) result.push_back(c);
        return result;
    }
};

// Service using the repository — no SQL here
class CustomerService {
    CustomerRepository& repo;
public:
    explicit CustomerService(CustomerRepository& repo) : repo(repo) {}

    void printHighValueInCity(const std::string& city, double minCredit) {
        auto customers = repo.findByCity(city);
        for (const auto& c : customers)
            if (c->getCreditLimit() >= minCredit)
                std::cout << "  " << c->getName() << " (" << c->getCreditLimit() << ")\n";
    }
};

int main() {
    InMemoryCustomerRepository repo;
    repo.add(std::make_shared<Customer>(1, "Alice", "NYC", 10000.0));
    repo.add(std::make_shared<Customer>(2, "Bob",   "NYC", 500.0));
    repo.add(std::make_shared<Customer>(3, "Carol", "LA",  20000.0));

    CustomerService svc(repo);
    std::cout << "High-value NYC customers:\n";
    svc.printHighValueInCity("NYC", 5000.0);
}
```

**Pros**:
- Domain code reads like business language — no SQL leaked
- Easily mocked for unit testing (swap in-memory implementation)
- Centralizes query logic

**Cons**:
- Extra abstraction layer
- Complex queries may be awkward to express through collection interface

**Related**: Data Mapper (underlying persistence mechanism), Unit of Work (tracks changes), Query Object (for complex queries)

---

## Query Object

**Problem**: Building SQL strings in code is error-prone and hard to compose; adding new conditions leads to nested string concatenation.

**When to Use**:
- When business code needs to build complex, dynamic queries
- To isolate query construction from execution
- When queries need to be composed, reused, or tested independently

**Structure**:
- An object that represents a database query
- Fluent API to add criteria, ordering, limits
- Translates to SQL (or any query language) when executed

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// Query Object — builds a SELECT statement
class QueryObject {
    std::string  table;
    std::vector<std::string> criteria;
    std::string  orderBy;
    int          limitRows = -1;

public:
    explicit QueryObject(std::string table) : table(std::move(table)) {}

    // Fluent API
    QueryObject& where(const std::string& condition) {
        criteria.push_back(condition);
        return *this;
    }

    QueryObject& orderByColumn(const std::string& column) {
        orderBy = column;
        return *this;
    }

    QueryObject& limit(int n) {
        limitRows = n;
        return *this;
    }

    // Generate SQL
    std::string toSql() const {
        std::ostringstream sql;
        sql << "SELECT * FROM " << table;

        if (!criteria.empty()) {
            sql << " WHERE ";
            for (size_t i = 0; i < criteria.size(); ++i) {
                if (i > 0) sql << " AND ";
                sql << criteria[i];
            }
        }

        if (!orderBy.empty()) sql << " ORDER BY " << orderBy;
        if (limitRows > 0)    sql << " LIMIT " << limitRows;

        return sql.str();
    }

    // Execute (simulated)
    void execute() const {
        std::cout << "[DB] " << toSql() << "\n";
    }
};

// Business code builds queries without string concatenation
void findActiveHighCreditCustomers(const std::string& city, double minCredit) {
    QueryObject query("customers");
    query.where("city = '" + city + "'")
         .where("credit_limit >= " + std::to_string(minCredit))
         .where("status = 'ACTIVE'")
         .orderByColumn("credit_limit DESC")
         .limit(10);

    std::cout << "Generated SQL:\n  " << query.toSql() << "\n";
    query.execute();
}

int main() {
    findActiveHighCreditCustomers("NYC", 5000.0);

    // Compose queries dynamically
    QueryObject base("orders");
    base.where("status = 'PENDING'");

    // Add optional filter
    bool filterByUser = true;
    if (filterByUser) base.where("user_id = 42");

    base.execute();
}
```

**Pros**:
- Type-safe, composable query construction
- No raw string concatenation — easier to avoid SQL injection
- Queries can be built incrementally and tested

**Cons**:
- More classes required
- Not all SQL features are easily modeled as objects

**Related**: Repository (uses Query Objects internally), Specification Pattern (boolean logic over domain objects)
