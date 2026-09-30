# Data Source Architectural Patterns

Patterns that organize how domain objects communicate with the database.

Source: https://martinfowler.com/eaaCatalog/

---

## Table Data Gateway

**Problem**: SQL queries are scattered across business logic code, making the data access layer hard to maintain or swap.

**When to Use**:
- Table Module or Transaction Script architectures
- When you want to centralize all SQL for a table in one place
- Simple CRUD applications

**Structure**:
- One gateway class per database table
- Methods correspond to queries and commands (`find`, `insert`, `update`, `delete`)
- Returns raw data (record sets / structs), not domain objects

**C++ Example**:
```cpp
#include <iostream>
#include <vector>
#include <string>
#include <optional>

// Row structure (like a DB record)
struct PersonRecord {
    int         id;
    std::string firstName;
    std::string lastName;
    int         age;
};

// Table Data Gateway — ALL SQL for the persons table lives here
class PersonGateway {
public:
    // Simulated query: SELECT * FROM persons WHERE id = ?
    std::optional<PersonRecord> findById(int id) {
        std::cout << "[SQL] SELECT * FROM persons WHERE id=" << id << "\n";
        if (id == 1) return PersonRecord{ 1, "Alice", "Smith", 30 };
        return std::nullopt;
    }

    // Simulated query: SELECT * FROM persons WHERE age > ?
    std::vector<PersonRecord> findOlderThan(int age) {
        std::cout << "[SQL] SELECT * FROM persons WHERE age > " << age << "\n";
        return {
            { 1, "Alice", "Smith", 30 },
            { 2, "Bob",   "Jones", 45 },
        };
    }

    // Simulated command: INSERT INTO persons ...
    int insert(const std::string& firstName, const std::string& lastName, int age) {
        std::cout << "[SQL] INSERT INTO persons VALUES ('" << firstName
                  << "', '" << lastName << "', " << age << ")\n";
        return 42; // new ID
    }

    // Simulated command: UPDATE persons SET age=? WHERE id=?
    void updateAge(int id, int newAge) {
        std::cout << "[SQL] UPDATE persons SET age=" << newAge
                  << " WHERE id=" << id << "\n";
    }

    // Simulated command: DELETE FROM persons WHERE id=?
    void deleteById(int id) {
        std::cout << "[SQL] DELETE FROM persons WHERE id=" << id << "\n";
    }
};

// Transaction Script using the gateway
void printAdults(PersonGateway& gw) {
    auto adults = gw.findOlderThan(17);
    for (const auto& p : adults)
        std::cout << p.firstName << " " << p.lastName << " (age " << p.age << ")\n";
}

int main() {
    PersonGateway gw;
    gw.insert("Carol", "Davis", 25);
    printAdults(gw);
    gw.updateAge(1, 31);
    gw.deleteById(2);
}
```

**Pros**:
- All SQL in one class — easy to audit or swap DB
- Simple to understand and implement
- Works naturally with Transaction Script

**Cons**:
- Returns raw data, not objects — callers must interpret
- Does not couple well with Domain Model
- One class per table can become large

**Related**: Row Data Gateway (per-row instead of per-table), Data Mapper (returns domain objects), Active Record (combined gateway + domain object)

---

## Row Data Gateway

**Problem**: You need each database row to be represented by its own in-memory object, with DB access bundled in.

**When to Use**:
- When you want one object per row, handling its own persistence
- Works well with Transaction Script where each row is processed independently
- Simpler alternative to Active Record when domain logic is minimal

**Structure**:
- Each instance represents exactly one database row
- Object has fields matching columns + `find`, `insert`, `update`, `delete` methods
- No domain logic — only data and SQL

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <optional>

// Row Data Gateway — one instance = one DB row
class PersonRow {
    int         id;
    std::string firstName;
    std::string lastName;
    int         numberOfDependents;
public:
    // Constructor from existing row
    PersonRow(int id, std::string fn, std::string ln, int deps)
        : id(id), firstName(std::move(fn)), lastName(std::move(ln))
        , numberOfDependents(deps) {}

    // Factory: find from DB (static finder)
    static std::optional<PersonRow> find(int id) {
        std::cout << "[SQL] SELECT * FROM persons WHERE id=" << id << "\n";
        // Simulate result
        return PersonRow{ id, "Alice", "Smith", 2 };
    }

    static std::optional<PersonRow> findByLastName(const std::string& ln) {
        std::cout << "[SQL] SELECT * FROM persons WHERE lastName='" << ln << "'\n";
        return PersonRow{ 1, "Alice", ln, 2 };
    }

    // Instance operations
    void insert() {
        std::cout << "[SQL] INSERT INTO persons VALUES ('"
                  << firstName << "', '" << lastName << "', "
                  << numberOfDependents << ")\n";
    }

    void update() {
        std::cout << "[SQL] UPDATE persons SET firstName='" << firstName
                  << "', lastName='" << lastName
                  << "', numberOfDependents=" << numberOfDependents
                  << " WHERE id=" << id << "\n";
    }

    void remove() {
        std::cout << "[SQL] DELETE FROM persons WHERE id=" << id << "\n";
    }

    // Accessors
    int         getId()                 const { return id; }
    const std::string& getFirstName()   const { return firstName; }
    const std::string& getLastName()    const { return lastName; }
    int         getNumberOfDependents() const { return numberOfDependents; }
    void        setNumberOfDependents(int n)  { numberOfDependents = n; }
};

// Transaction Script using Row Data Gateway
double exemptionFor(const PersonRow& person) {
    // Business rule: extra exemption if 2+ dependents
    return person.getNumberOfDependents() > 1 ? 1500.0 : 1000.0;
}

int main() {
    auto person = PersonRow::find(1);
    if (person) {
        std::cout << "Exemption: " << exemptionFor(*person) << "\n";
        person->setNumberOfDependents(3);
        person->update();
    }
}
```

**Pros**:
- Clean separation of data access from business logic
- Easy to understand one-object-per-row mental model
- Finder methods are easy to locate

**Cons**:
- No domain behavior — logic must live in scripts
- Instance-per-row memory cost at scale
- Doesn't compose well with Domain Model

**Related**: Table Data Gateway (per-table), Active Record (adds domain behavior), Data Mapper (full separation)

---

## Active Record

**Problem**: You want domain objects that also handle their own persistence — keeping things simple for CRUD-heavy apps.

**When to Use**:
- Domain logic is not too complex
- Objects map closely to tables (one class ≈ one table)
- Speed of development matters more than clean separation

**Structure**:
- Domain object fields correspond to table columns
- Object has domain methods (business behavior) AND `save`, `delete`, `find` methods
- The class itself is both the domain model and the data mapper

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <optional>
#include <stdexcept>

class Employee {
    int         id       = 0;
    std::string name;
    std::string type;    // "SALARIED", "COMMISSIONED", "HOURLY"
    double      salary   = 0.0;
    bool        isNew    = true;
public:
    Employee() = default;
    Employee(std::string name, std::string type, double salary)
        : name(std::move(name)), type(std::move(type)), salary(salary) {}

    // ── Persistence ──────────────────────────────────────────────────────────
    void save() {
        if (isNew) {
            std::cout << "[SQL] INSERT INTO employees (name,type,salary) VALUES ('"
                      << name << "','" << type << "'," << salary << ")\n";
            id    = 99; // simulated auto-increment
            isNew = false;
        } else {
            std::cout << "[SQL] UPDATE employees SET salary=" << salary
                      << " WHERE id=" << id << "\n";
        }
    }

    void remove() {
        if (!isNew)
            std::cout << "[SQL] DELETE FROM employees WHERE id=" << id << "\n";
    }

    static std::optional<Employee> find(int id) {
        std::cout << "[SQL] SELECT * FROM employees WHERE id=" << id << "\n";
        Employee e("Bob", "SALARIED", 70000.0);
        e.id    = id;
        e.isNew = false;
        return e;
    }

    // ── Domain Behavior ──────────────────────────────────────────────────────
    double annualBonus() const {
        if (type == "SALARIED")     return salary * 0.10;
        if (type == "COMMISSIONED") return salary * 0.20;
        return 0.0;
    }

    void giveRaise(double pct) {
        if (pct <= 0 || pct > 0.50)
            throw std::invalid_argument("Raise must be 0–50%");
        salary *= (1.0 + pct);
    }

    // Accessors
    int                getId()     const { return id; }
    const std::string& getName()   const { return name; }
    double             getSalary() const { return salary; }
};

int main() {
    // Create and save new employee
    Employee e("Carol", "COMMISSIONED", 60000.0);
    e.save();

    // Load from DB, apply business rule, persist
    auto emp = Employee::find(1);
    if (emp) {
        std::cout << "Bonus: " << emp->annualBonus() << "\n";
        emp->giveRaise(0.05);
        emp->save();
    }
}
```

**Pros**:
- Very convenient for simple CRUD
- One class — easy to find everything about an entity
- Low ceremony / fast to write

**Cons**:
- Domain model and persistence are coupled — hard to test without DB
- Doesn't scale to complex business rules
- Hard to reuse domain logic when persistence changes

**Related**: Data Mapper (full separation), Row Data Gateway (no domain behavior), Domain Model (rich behavior without persistence coupling)

---

## Data Mapper

**Problem**: You want domain objects that are completely ignorant of the database — but you still need persistence.

**When to Use**:
- Domain Model with complex business rules that should be unit-testable without DB
- Long-lived applications where schema and domain may evolve independently
- When domain objects must not depend on any infrastructure

**Structure**:
- Domain object: plain C++ class, no DB knowledge
- Mapper: separate class that knows both the domain object and the DB schema
- Mapper maps columns → fields on load, and fields → columns on save

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

// ── Domain Object — knows nothing about DB ───────────────────────────────────
class Person {
    int         id;
    std::string firstName;
    std::string lastName;
    int         age;
public:
    Person(int id, std::string fn, std::string ln, int age)
        : id(id), firstName(std::move(fn)), lastName(std::move(ln)), age(age) {}

    // Pure domain behavior
    bool isAdult() const { return age >= 18; }

    int                getId()        const { return id; }
    const std::string& getFirstName() const { return firstName; }
    const std::string& getLastName()  const { return lastName; }
    int                getAge()       const { return age; }
    void               setAge(int a)        { age = a; }
};

// ── Simulated DB row ─────────────────────────────────────────────────────────
struct PersonRow {
    int         id;
    std::string first_name;
    std::string last_name;
    int         age;
};

// ── Data Mapper — mediates between Person and DB ─────────────────────────────
class PersonMapper {
    // Simulated in-memory DB
    std::unordered_map<int, PersonRow> db = {
        { 1, { 1, "Alice", "Smith", 30 } },
        { 2, { 2, "Bob",   "Jones", 16 } },
    };
    int nextId = 3;

public:
    std::optional<std::unique_ptr<Person>> find(int id) {
        auto it = db.find(id);
        if (it == db.end()) return std::nullopt;
        const auto& row = it->second;
        std::cout << "[Mapper] Loaded Person id=" << row.id << "\n";
        return std::make_unique<Person>(row.id, row.first_name, row.last_name, row.age);
    }

    void insert(Person& person) {
        int id = nextId++;
        db[id] = { id, person.getFirstName(), person.getLastName(), person.getAge() };
        std::cout << "[Mapper] Inserted Person -> id=" << id << "\n";
    }

    void update(const Person& person) {
        auto it = db.find(person.getId());
        if (it == db.end()) return;
        it->second = { person.getId(), person.getFirstName(),
                       person.getLastName(), person.getAge() };
        std::cout << "[Mapper] Updated Person id=" << person.getId() << "\n";
    }

    void remove(int id) {
        db.erase(id);
        std::cout << "[Mapper] Deleted Person id=" << id << "\n";
    }
};

int main() {
    PersonMapper mapper;

    auto result = mapper.find(1);
    if (result) {
        auto& alice = *result;
        std::cout << alice->getFirstName() << " is adult: " << alice->isAdult() << "\n";
        alice->setAge(31);
        mapper.update(*alice);
    }

    auto bob = mapper.find(2);
    if (bob)
        std::cout << (*bob)->getFirstName() << " is adult: " << (*bob)->isAdult() << "\n";
}
```

**Pros**:
- Complete isolation of domain from persistence — testable without DB
- Domain and schema can evolve independently
- Works with complex Domain Models

**Cons**:
- Most complex to implement
- More classes and indirection
- Overkill for simple CRUD

**Related**: Active Record (simpler but coupled), Repository (builds on mapper for collection-like access), Identity Map (prevents duplicate loads), Unit of Work (batches mapper writes)
