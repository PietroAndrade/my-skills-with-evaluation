# Base Patterns

Foundational patterns used throughout enterprise application layers.

Source: https://martinfowler.com/eaaCatalog/

---

## Gateway

**Problem**: Your application depends directly on the API of an external system (payment provider, file system, messaging service). That API is messy, changes frequently, or is hard to mock for tests.

**When to Use**:
- When wrapping any external system or third-party API
- When you want to insulate your domain from infrastructure concerns
- When you need to swap implementations (production vs. test stub)

**Structure**:
- One class encapsulates all access to one external system
- Domain code only talks to the Gateway interface
- The concrete Gateway handles protocol, authentication, and error translation

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>

// External system result type
struct PaymentResult { bool success; std::string transactionId; std::string error; };

// ── Gateway Interface ─────────────────────────────────────────────────────────
class PaymentGateway {
public:
    virtual ~PaymentGateway() = default;
    virtual PaymentResult charge(const std::string& accountId, double amount) = 0;
    virtual PaymentResult refund(const std::string& transactionId)            = 0;
};

// ── Production Gateway (wraps real payment API) ──────────────────────────────
class StripeGateway : public PaymentGateway {
public:
    PaymentResult charge(const std::string& accountId, double amount) override {
        // In reality: HTTP call to Stripe's API
        std::cout << "[Stripe] Charging " << accountId << " $" << amount << "\n";
        return { true, "stripe-txn-001", "" };
    }

    PaymentResult refund(const std::string& transactionId) override {
        std::cout << "[Stripe] Refunding " << transactionId << "\n";
        return { true, transactionId, "" };
    }
};

// ── Test Stub Gateway ─────────────────────────────────────────────────────────
class StubPaymentGateway : public PaymentGateway {
    bool shouldSucceed;
public:
    explicit StubPaymentGateway(bool succeed = true) : shouldSucceed(succeed) {}

    PaymentResult charge(const std::string& accountId, double amount) override {
        if (!shouldSucceed) return { false, "", "Card declined" };
        return { true, "stub-txn-999", "" };
    }

    PaymentResult refund(const std::string& transactionId) override {
        return { true, transactionId, "" };
    }
};

// ── Domain service using the gateway ─────────────────────────────────────────
class OrderService {
    PaymentGateway& gateway;
public:
    explicit OrderService(PaymentGateway& gw) : gateway(gw) {}

    void processPayment(const std::string& accountId, double total) {
        auto result = gateway.charge(accountId, total);
        if (!result.success)
            throw std::runtime_error("Payment failed: " + result.error);
        std::cout << "Payment confirmed: " << result.transactionId << "\n";
    }
};

int main() {
    // Production
    StripeGateway stripe;
    OrderService  prodService(stripe);
    prodService.processPayment("acct_123", 99.99);

    // Test
    StubPaymentGateway stub(false);  // simulate decline
    OrderService testService(stub);
    try {
        testService.processPayment("acct_456", 50.00);
    } catch (const std::exception& e) {
        std::cout << "Test caught: " << e.what() << "\n";
    }
}
```

**Pros**:
- Isolates domain from external system details
- Easy to stub for testing
- External API changes only affect the Gateway class

**Cons**:
- Extra indirection for each external system
- Gateway can become large if the external API is complex

**Related**: Service Stub (test implementation of a Gateway), Mapper (translates data formats), Remote Facade (inverse: expose your system remotely)

---

## Mapper

**Problem**: Two layers (e.g., domain and DB, or domain and external service) need to communicate without depending on each other. Both must remain unaware of the other's structure.

**When to Use**:
- When two subsystems need to be decoupled while exchanging data
- When domain objects and DB schemas diverge significantly
- When translation logic would pollute either side if embedded there

**Structure**:
- A separate class that knows about both sides and translates between them
- Neither side knows about the Mapper or the other side
- Often implemented as a Data Mapper or a DTO assembler

**C++ Example**:
```cpp
#include <iostream>
#include <string>

// ── Side A: Domain Layer ──────────────────────────────────────────────────────
struct Person {
    std::string firstName;
    std::string lastName;
    int         ageYears;
};

// ── Side B: External API / DB Row ─────────────────────────────────────────────
struct PersonApiResponse {
    std::string full_name;     // "Last, First"
    int         age_months;    // age in months
};

// ── Mapper — knows both sides, neither side knows it ─────────────────────────
class PersonMapper {
public:
    // Map from external API response → domain object
    Person toDomain(const PersonApiResponse& response) const {
        // Parse "Last, First" format
        auto comma = response.full_name.find(", ");
        std::string lastName  = response.full_name.substr(0, comma);
        std::string firstName = response.full_name.substr(comma + 2);
        int         ageYears  = response.age_months / 12;
        return { firstName, lastName, ageYears };
    }

    // Map from domain object → external API format
    PersonApiResponse toApi(const Person& person) const {
        return {
            person.lastName + ", " + person.firstName,
            person.ageYears * 12,
        };
    }
};

int main() {
    PersonMapper mapper;

    PersonApiResponse apiResponse{ "Smith, Alice", 372 };  // 31 years
    Person            domain   = mapper.toDomain(apiResponse);
    std::cout << "Domain: " << domain.firstName << " " << domain.lastName
              << ", age " << domain.ageYears << "\n";

    Person            updated  = { "Bob", "Jones", 25 };
    PersonApiResponse apiForm  = mapper.toApi(updated);
    std::cout << "API: " << apiForm.full_name << " (" << apiForm.age_months << " months)\n";
}
```

**Pros**:
- Both layers remain independent of each other
- Translation logic is centralized
- Easy to test the mapping independently

**Cons**:
- Extra class per mapping pair
- Must be updated if either schema changes

**Related**: Data Mapper (a specific Mapper for DB persistence), DTO Assembler (Mapper for DTOs), Gateway (often uses a Mapper internally)

---

## Value Object

**Problem**: Small domain concepts like Money, DateRange, or Coordinates are passed as primitives or structs, leading to scattered validation, inconsistent equality, and implicit coupling between fields.

**When to Use**:
- For small concepts that define their identity by their value, not a DB id
- When equality means "same values" not "same instance"
- When immutability ensures consistency (e.g., Money, Temperature, Color)

**Structure**:
- Immutable — all fields set at construction
- Equality based on all fields (not object identity)
- May have convenience methods (arithmetic, formatting)
- Never has a database id

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <stdexcept>

// ── Value Object: Money ───────────────────────────────────────────────────────
class Money {
    long        cents;    // store as integer to avoid float rounding
    std::string currency;
public:
    Money(long cents, std::string currency)
        : cents(cents), currency(std::move(currency)) {}

    // Factory for common case
    static Money usd(double amount) {
        return Money(static_cast<long>(amount * 100), "USD");
    }

    // ── Value equality ────────────────────────────────────────────────────────
    bool operator==(const Money& other) const {
        return cents == other.cents && currency == other.currency;
    }
    bool operator!=(const Money& other) const { return !(*this == other); }
    bool operator< (const Money& other) const {
        assertSameCurrency(other);
        return cents < other.cents;
    }

    // ── Arithmetic returns new Value Objects ──────────────────────────────────
    Money operator+(const Money& other) const {
        assertSameCurrency(other);
        return Money(cents + other.cents, currency);
    }
    Money operator-(const Money& other) const {
        assertSameCurrency(other);
        return Money(cents - other.cents, currency);
    }
    Money operator*(double factor) const {
        return Money(static_cast<long>(cents * factor), currency);
    }

    // ── Accessors ─────────────────────────────────────────────────────────────
    double      amount()   const { return cents / 100.0; }
    const std::string& getCurrency() const { return currency; }

    std::string toString() const {
        return currency + " " + std::to_string(amount());
    }

private:
    void assertSameCurrency(const Money& other) const {
        if (currency != other.currency)
            throw std::invalid_argument("Currency mismatch: " + currency + " vs " + other.currency);
    }
};

// ── Value Object: DateRange ───────────────────────────────────────────────────
struct Date { int year; int month; int day;
    bool operator==(const Date& o) const { return year==o.year && month==o.month && day==o.day; }
    bool operator< (const Date& o) const {
        if (year  != o.year)  return year  < o.year;
        if (month != o.month) return month < o.month;
        return day < o.day;
    }
};

class DateRange {
    Date start;
    Date end;
public:
    DateRange(Date start, Date end) : start(std::move(start)), end(std::move(end)) {
        if (end < start) throw std::invalid_argument("End must be >= start");
    }

    bool includes(const Date& date) const {
        return !(date < start) && !(end < date);
    }

    bool overlaps(const DateRange& other) const {
        return !(end < other.start) && !(other.end < start);
    }

    bool operator==(const DateRange& other) const {
        return start == other.start && end == other.end;
    }

    const Date& getStart() const { return start; }
    const Date& getEnd()   const { return end;   }
};

int main() {
    // Money Value Object
    Money price  = Money::usd(19.99);
    Money tax    = Money::usd(2.00);
    Money total  = price + tax;
    std::cout << "Total: $" << total.amount() << "\n";

    Money discount = total * 0.1;
    std::cout << "10% off: $" << discount.amount() << "\n";

    Money a = Money::usd(10.0);
    Money b = Money::usd(10.0);
    std::cout << "a == b: " << (a == b ? "true" : "false") << "\n"; // true

    // DateRange Value Object
    DateRange jan{{ 2024, 1, 1 }, { 2024, 1, 31 }};
    DateRange feb{{ 2024, 2, 1 }, { 2024, 2, 29 }};
    std::cout << "Jan/Feb overlap: " << (jan.overlaps(feb) ? "yes" : "no") << "\n";
    std::cout << "Jan 15 in Jan: "   << (jan.includes({ 2024, 1, 15 }) ? "yes" : "no") << "\n";
}
```

**Pros**:
- Immutability prevents accidental mutation
- Value equality is natural and correct
- Self-validating (throws on invalid construction)
- Encapsulates concept-specific operations

**Cons**:
- Requires discipline — easy to accidentally use mutable structs instead
- More verbose than passing primitives

**Related**: Domain Model (uses Value Objects for small concepts), Money pattern (canonical example)

---

## Registry

**Problem**: Objects deep in a call stack need access to well-known services (logging, configuration, repositories), without threading them through every call.

**When to Use**:
- For application-wide services that need to be accessible anywhere
- As a controlled alternative to global variables
- When Dependency Injection is too heavyweight for the context

**Structure**:
- A static or singleton class that maps names/types to service instances
- Only one Registry at a time per application scope
- Services are registered at startup and retrieved by type or key

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <any>
#include <stdexcept>
#include <typeindex>

// A simple type-safe Registry
class Registry {
    std::unordered_map<std::type_index, std::shared_ptr<void>> services;

    Registry() = default;
public:
    static Registry& instance() {
        static Registry reg;
        return reg;
    }

    // Register a service by its interface type
    template<typename T>
    void registerService(std::shared_ptr<T> service) {
        services[std::type_index(typeid(T))] = service;
    }

    // Retrieve a service by its interface type
    template<typename T>
    std::shared_ptr<T> get() {
        auto it = services.find(std::type_index(typeid(T)));
        if (it == services.end())
            throw std::runtime_error("Service not registered");
        return std::static_pointer_cast<T>(it->second);
    }
};

// Service interfaces
class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const std::string& msg) = 0;
};

class ConsoleLogger : public Logger {
public:
    void log(const std::string& msg) override {
        std::cout << "[LOG] " << msg << "\n";
    }
};

class Config {
public:
    virtual ~Config() = default;
    virtual std::string get(const std::string& key) = 0;
};

class AppConfig : public Config {
    std::unordered_map<std::string, std::string> values = {
        { "db.host", "localhost" },
        { "db.port", "5432"      },
    };
public:
    std::string get(const std::string& key) override {
        auto it = values.find(key);
        return it != values.end() ? it->second : "";
    }
};

// Code deep in the call stack that uses the Registry
void someDeepFunction() {
    auto logger = Registry::instance().get<Logger>();
    auto config = Registry::instance().get<Config>();
    logger->log("deep function called, db.host=" + config->get("db.host"));
}

int main() {
    // Application startup — register services
    Registry::instance().registerService<Logger>(std::make_shared<ConsoleLogger>());
    Registry::instance().registerService<Config>(std::make_shared<AppConfig>());

    // Anywhere in the app — retrieve services
    someDeepFunction();

    auto logger = Registry::instance().get<Logger>();
    logger->log("Main finished");
}
```

**Pros**:
- Accessible from anywhere without threading through call stacks
- Centralizes service lifecycle management
- Testable — swap implementations at startup

**Cons**:
- Is essentially a controlled global state — can be hard to reason about
- Hides dependencies (vs. explicit DI)
- Thread-safety requires care

**Related**: Service Locator (similar idea, often criticized more), Gateway (often stored in Registry), Singleton (Registry is usually a Singleton)

---

## Special Case (Null Object)

**Problem**: Callers must constantly check for null/missing values before using an object, leading to scattered null checks and potential NullPointerExceptions.

**When to Use**:
- When "not found" or "unknown" is a valid domain concept
- When the null case has defined, meaningful behavior (return zero, log nothing, etc.)
- When you want to eliminate null checks in business logic

**Structure**:
- A subclass or implementation of the domain object interface
- Has the same interface as the real object
- All methods return safe, "do nothing" or default values
- Business code never checks for null — just calls the object's interface

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>

// Domain interface
class Customer {
public:
    virtual ~Customer() = default;
    virtual std::string getName()     const = 0;
    virtual double      getDiscount() const = 0;
    virtual bool        isNull()      const = 0;
    virtual void        apply(double price) const = 0;
};

// Real Customer
class RealCustomer : public Customer {
    std::string name;
    double      discount;
public:
    RealCustomer(std::string name, double discount)
        : name(std::move(name)), discount(discount) {}
    std::string getName()     const override { return name; }
    double      getDiscount() const override { return discount; }
    bool        isNull()      const override { return false; }
    void        apply(double price) const override {
        std::cout << "Charged " << name << ": $" << price * (1.0 - discount) << "\n";
    }
};

// Null / Special Case Customer — safe defaults, no behavior
class NullCustomer : public Customer {
public:
    std::string getName()     const override { return "Guest";  }
    double      getDiscount() const override { return 0.0;      }
    bool        isNull()      const override { return true;     }
    void        apply(double price) const override {
        std::cout << "Guest charged full price: $" << price << "\n";
    }
};

// Customer lookup — returns NullCustomer instead of nullptr
std::unique_ptr<Customer> findCustomer(const std::string& id) {
    if (id == "alice") return std::make_unique<RealCustomer>("Alice", 0.10);
    return std::make_unique<NullCustomer>(); // never returns nullptr
}

// Business logic — NO null checks
void processOrder(const std::string& customerId, double price) {
    auto customer = findCustomer(customerId);
    // No if (!customer) check needed
    customer->apply(price);
}

int main() {
    processOrder("alice",   100.0); // real customer
    processOrder("unknown", 100.0); // null customer — safe behavior
}
```

**Pros**:
- Eliminates null checks from business logic
- "Missing" has a clear, consistent behavior
- The null case is explicit and testable

**Cons**:
- Adds extra class per domain concept
- Null behavior must be well-defined — wrong defaults can hide bugs

**Related**: Special Case is the general form (Null Object is the most common variant), Domain Model (when business rules need to handle missing entities cleanly)

---

## Service Stub

**Problem**: Unit tests depend on external services (payment gateway, email server, remote API) that are slow, expensive, or unavailable in test environments.

**When to Use**:
- When writing unit or integration tests for code that calls external services
- When the external service has side effects you don't want in tests (emails, charges)
- When you want precise control over the service's behavior per test

**Structure**:
- Implements the same interface as the real service
- Returns controlled, predictable data
- May record calls or assert on interactions (then it becomes a Mock)
- Injected via Dependency Injection or Registry at test time

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>

// ── Service interface ─────────────────────────────────────────────────────────
struct EmailMessage { std::string to; std::string subject; std::string body; };

class EmailService {
public:
    virtual ~EmailService() = default;
    virtual void send(const EmailMessage& msg) = 0;
};

class TaxCalcService {
public:
    virtual ~TaxCalcService() = default;
    virtual double calculateTax(double subtotal, const std::string& region) = 0;
};

// ── Production implementations ────────────────────────────────────────────────
class SmtpEmailService : public EmailService {
public:
    void send(const EmailMessage& msg) override {
        std::cout << "[SMTP] Sending to " << msg.to << ": " << msg.subject << "\n";
        // Real SMTP call here
    }
};

class ExternalTaxService : public TaxCalcService {
public:
    double calculateTax(double subtotal, const std::string& region) override {
        std::cout << "[ExternalTax] HTTP call for " << region << "\n";
        // Real HTTP call here
        return subtotal * 0.08;
    }
};

// ── Service Stubs for testing ─────────────────────────────────────────────────
class StubEmailService : public EmailService {
public:
    std::vector<EmailMessage> sentMessages; // captured for assertions

    void send(const EmailMessage& msg) override {
        std::cout << "[Stub] Email captured: " << msg.to << "\n";
        sentMessages.push_back(msg);
    }

    bool wasSentTo(const std::string& to) const {
        for (const auto& m : sentMessages)
            if (m.to == to) return true;
        return false;
    }
};

class StubTaxService : public TaxCalcService {
    double fixedRate;
public:
    explicit StubTaxService(double rate = 0.10) : fixedRate(rate) {}
    double calculateTax(double subtotal, const std::string&) override {
        return subtotal * fixedRate; // predictable, no HTTP
    }
};

// ── Application service under test ───────────────────────────────────────────
class OrderService {
    EmailService&   email;
    TaxCalcService& tax;
public:
    OrderService(EmailService& email, TaxCalcService& tax)
        : email(email), tax(tax) {}

    double placeOrder(const std::string& customerEmail, double subtotal, const std::string& region) {
        double taxAmount = tax.calculateTax(subtotal, region);
        double total     = subtotal + taxAmount;

        email.send({ customerEmail, "Order Confirmation",
                     "Your order total is $" + std::to_string(total) });
        return total;
    }
};

// ── Simulated test ────────────────────────────────────────────────────────────
void testOrderPlacement() {
    StubEmailService emailStub;
    StubTaxService   taxStub(0.10);

    OrderService svc(emailStub, taxStub);
    double total = svc.placeOrder("alice@example.com", 100.0, "CA");

    // Assertions (in real code: use gtest EXPECT_*)
    std::cout << "Total: " << total << " (expected 110)\n";
    std::cout << "Email sent: " << (emailStub.wasSentTo("alice@example.com") ? "YES" : "NO") << "\n";
}

int main() {
    testOrderPlacement();
}
```

**Pros**:
- Tests run fast and reliably without network/external dependencies
- Predictable, controlled behavior per test scenario
- Can capture/assert on calls to the stub

**Cons**:
- Stubs can diverge from real implementations over time
- Too much mocking can lead to tests that pass but don't reflect reality

**Related**: Gateway (the interface the stub implements), Registry (where stubs are registered at test time)

---

## Layer Supertype

**Problem**: All objects in a layer share common behavior (audit fields, save/load infrastructure, event publishing) that would be duplicated in every class.

**When to Use**:
- When all domain objects, all mappers, or all services share common behavior
- Centralizes infrastructure concerns (dirty tracking, timestamps, identity) in one base class
- Reduces repetition across all classes in a layer

**Structure**:
- A single abstract/base class for all objects in a layer
- Contains common fields and behaviors specific to that layer
- Subclasses inherit the infrastructure and focus on domain logic

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <chrono>
#include <memory>

using Clock    = std::chrono::system_clock;
using TimePoint = Clock::time_point;

// ── Layer Supertype for Domain Objects ───────────────────────────────────────
class DomainObject {
    int       id       = 0;
    int       version  = 0;         // optimistic locking
    TimePoint createdAt;
    TimePoint updatedAt;
    bool      dirty    = false;     // Unit of Work support
protected:
    void markDirty() { dirty = true; updatedAt = Clock::now(); }

public:
    DomainObject() : createdAt(Clock::now()), updatedAt(Clock::now()) {}
    virtual ~DomainObject() = default;

    int  getId()      const { return id; }
    int  getVersion() const { return version; }
    bool isDirty()    const { return dirty;   }

    void assignId(int assignedId) { id = assignedId; }
    void clearDirty()             { dirty = false;    }

    virtual std::string getType() const = 0;
};

// ── Concrete domain objects — inherit infrastructure, focus on domain ─────────
class Customer : public DomainObject {
    std::string name;
    std::string email;
public:
    Customer(std::string name, std::string email)
        : name(std::move(name)), email(std::move(email)) {}

    std::string getType() const override { return "Customer"; }

    const std::string& getName()  const { return name; }
    const std::string& getEmail() const { return email; }

    void changeEmail(std::string newEmail) {
        email = std::move(newEmail);
        markDirty(); // inherited from supertype
    }
};

class Product : public DomainObject {
    std::string name;
    double      price;
public:
    Product(std::string name, double price)
        : name(std::move(name)), price(price) {}

    std::string getType()  const override { return "Product"; }
    double      getPrice() const         { return price; }

    void setPrice(double p) {
        price = p;
        markDirty(); // inherited
    }
};

// ── Layer Supertype for Mappers ───────────────────────────────────────────────
class AbstractMapper {
protected:
    void logLoad(const DomainObject& obj) {
        std::cout << "[Mapper] Loaded " << obj.getType() << " id=" << obj.getId() << "\n";
    }
    void logSave(const DomainObject& obj) {
        std::cout << "[Mapper] Saved " << obj.getType() << " id=" << obj.getId()
                  << " dirty=" << obj.isDirty() << "\n";
    }
};

class CustomerMapper : public AbstractMapper {
public:
    std::unique_ptr<Customer> find(int id) {
        auto c = std::make_unique<Customer>("Alice", "alice@example.com");
        c->assignId(id);
        logLoad(*c);
        return c;
    }
    void save(const Customer& c) {
        logSave(c);
        // Real SQL here
    }
};

int main() {
    CustomerMapper mapper;
    auto customer = mapper.find(1);
    std::cout << "Email: " << customer->getEmail() << "\n";
    std::cout << "Dirty: " << customer->isDirty() << "\n";

    customer->changeEmail("alice@new.com");
    std::cout << "Dirty after change: " << customer->isDirty() << "\n";

    mapper.save(*customer);

    Product p("Widget", 9.99);
    std::cout << "Product dirty: " << p.isDirty() << "\n";
    p.setPrice(12.99);
    std::cout << "Product dirty after price change: " << p.isDirty() << "\n";
}
```

**Pros**:
- No duplication of infrastructure code across all domain objects
- Consistent behavior across all objects in a layer (dirty tracking, IDs, timestamps)
- New domain objects automatically get all infrastructure for free

**Cons**:
- Deep inheritance chains can become rigid
- Can pull in behavior that not all subclasses need
- C++ has no single-inheritance limitations, but multiple Layer Supertypes can conflict

**Related**: Domain Model (the objects that use it), Unit of Work (uses `isDirty()` from the supertype), Data Mapper (mapper supertype shares common load/save infrastructure)
