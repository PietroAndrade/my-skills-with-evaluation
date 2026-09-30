# Distribution Patterns

Patterns for structuring communication between processes or remote services.

Source: https://martinfowler.com/eaaCatalog/

---

## Remote Facade

**Problem**: Fine-grained object calls work well in-process, but over a network they create excessive round-trips and unacceptable latency.

**When to Use**:
- When exposing an internal fine-grained domain model over a remote interface (gRPC, REST, IPC)
- When remote calls are expensive and you want to minimize their number
- To avoid exposing internal domain structure to remote clients

**Structure**:
- A coarse-grained facade wraps the fine-grained domain objects
- Each facade method performs a batch of internal operations and returns all needed data in one call
- Clients call the facade, not individual domain methods
- The facade translates between DTOs (Data Transfer Objects) and domain objects

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// ── Fine-grained domain objects (internal) ───────────────────────────────────
class Customer {
    int         id;
    std::string name;
    std::string email;
public:
    Customer(int id, std::string name, std::string email)
        : id(id), name(std::move(name)), email(std::move(email)) {}

    int                getId()    const { return id; }
    const std::string& getName()  const { return name; }
    const std::string& getEmail() const { return email; }
    void               setEmail(std::string e) { email = std::move(e); }
};

class Address {
    int         customerId;
    std::string street;
    std::string city;
public:
    Address(int cid, std::string street, std::string city)
        : customerId(cid), street(std::move(street)), city(std::move(city)) {}

    int                getCustomerId() const { return customerId; }
    const std::string& getStreet()     const { return street; }
    const std::string& getCity()       const { return city; }
    void               setCity(std::string c) { city = std::move(c); }
};

class Order {
    int    customerId;
    double total;
public:
    Order(int cid, double total) : customerId(cid), total(total) {}
    int    getCustomerId() const { return customerId; }
    double getTotal()      const { return total; }
};

// ── Coarse-grained DTO (what remote clients receive in one call) ──────────────
struct CustomerSummaryDTO {
    int         id;
    std::string name;
    std::string email;
    std::string city;
    double      totalOrders;
};

struct UpdateCustomerDTO {
    int         id;
    std::string newEmail;
    std::string newCity;
};

// ── Remote Facade — coarse-grained remote interface ──────────────────────────
class CustomerFacade {
    // Simulated repositories
    Customer  internalCustomer{ 1, "Alice", "alice@example.com" };
    Address   internalAddress { 1, "123 Main St", "Springfield" };
    std::vector<Order> internalOrders{ { 1, 150.0 }, { 1, 75.0 } };

public:
    // One remote call → gathers data from multiple domain objects
    CustomerSummaryDTO getCustomerSummary(int customerId) {
        // Would call customer repo, address repo, order repo internally
        double totalOrders = 0.0;
        for (const auto& o : internalOrders)
            if (o.getCustomerId() == customerId) totalOrders += o.getTotal();

        std::cout << "[Facade] getCustomerSummary — 3 internal calls batched into 1\n";
        return {
            internalCustomer.getId(),
            internalCustomer.getName(),
            internalCustomer.getEmail(),
            internalAddress.getCity(),
            totalOrders,
        };
    }

    // One remote call → updates multiple domain objects
    void updateCustomer(const UpdateCustomerDTO& dto) {
        std::cout << "[Facade] updateCustomer — 2 internal updates batched into 1\n";
        internalCustomer.setEmail(dto.newEmail);
        internalAddress.setCity(dto.newCity);
        // Would call Unit of Work / commit here
    }
};

int main() {
    CustomerFacade facade;

    auto summary = facade.getCustomerSummary(1);
    std::cout << "Customer: " << summary.name
              << " | " << summary.email
              << " | " << summary.city
              << " | Orders: $" << summary.totalOrders << "\n";

    facade.updateCustomer({ 1, "alice@new.com", "Shelbyville" });
    summary = facade.getCustomerSummary(1);
    std::cout << "Updated: " << summary.email << " | " << summary.city << "\n";
}
```

**Pros**:
- Minimizes network round-trips — one big call instead of many small ones
- Hides internal domain structure from remote clients
- Easy to version the remote interface independently

**Cons**:
- Facade methods can become bloated if they try to cover too many scenarios
- Coarse granularity makes it harder to compose operations
- Introduces a translation layer that must be maintained

**Related**: Data Transfer Object (the payloads the facade sends/receives), Gateway (wraps external systems on the caller side)

---

## Data Transfer Object (DTO)

**Problem**: Remote calls are expensive. Sending individual fields over the wire one by one, or exposing domain objects directly, is impractical and couples consumers to your internal model.

**When to Use**:
- When transferring data between processes or over the network
- When you want to decouple the consumer's view of data from the internal domain model
- When a single message needs to carry multiple fields to minimize round-trips

**Structure**:
- A plain data object with no behavior — only fields (getters/setters or public members)
- Serializable (JSON, protobuf, XML, binary)
- May differ significantly from the domain model in shape
- Can be assembled from multiple domain objects and sent as one payload

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// ── DTOs — plain data carriers, no logic ────────────────────────────────────
struct AddressDTO {
    std::string street;
    std::string city;
    std::string country;

    std::string serialize() const {
        return street + "|" + city + "|" + country;
    }
    static AddressDTO deserialize(const std::string& s) {
        // simplified
        return { "123 Main St", "NYC", "US" };
    }
};

struct OrderItemDTO {
    int         productId;
    std::string productName;
    int         quantity;
    double      unitPrice;
};

struct OrderDTO {
    int                      orderId;
    std::string              customerName;
    AddressDTO               shippingAddress;
    std::vector<OrderItemDTO> items;
    double                   totalAmount;

    // Simple serialization for illustration
    std::string serialize() const {
        std::ostringstream oss;
        oss << "Order{id=" << orderId
            << ", customer=" << customerName
            << ", total=" << totalAmount
            << ", items=" << items.size()
            << ", address=" << shippingAddress.city << "}";
        return oss.str();
    }
};

// ── Domain objects (internal) ─────────────────────────────────────────────────
struct InternalOrder {
    int    id;
    int    customerId;
    double subtotal;
    double tax;
};

struct InternalCustomer {
    int         id;
    std::string firstName;
    std::string lastName;

    std::string fullName() const { return firstName + " " + lastName; }
};

// ── Assembler — constructs DTO from multiple domain objects ──────────────────
class OrderDTOAssembler {
public:
    static OrderDTO assemble(const InternalOrder&    order,
                             const InternalCustomer& customer,
                             const AddressDTO&        address,
                             const std::vector<OrderItemDTO>& items)
    {
        return {
            order.id,
            customer.fullName(),
            address,
            items,
            order.subtotal + order.tax,
        };
    }
};

// ── Remote Facade using the DTO ───────────────────────────────────────────────
class OrderFacade {
public:
    OrderDTO getOrder(int orderId) {
        InternalOrder    order    { orderId, 1, 100.0, 10.0  };
        InternalCustomer customer { 1, "Alice", "Smith"       };
        AddressDTO       address  { "123 Main St", "NYC", "US" };
        std::vector<OrderItemDTO> items = {
            { 10, "Widget",  2, 9.99  },
            { 20, "Gadget",  1, 24.99 },
        };

        std::cout << "[Facade] Assembling DTO from order, customer, address, items\n";
        return OrderDTOAssembler::assemble(order, customer, address, items);
    }
};

int main() {
    OrderFacade facade;
    OrderDTO dto = facade.getOrder(42);

    std::cout << dto.serialize() << "\n";
    std::cout << "Items:\n";
    for (const auto& item : dto.items)
        std::cout << "  " << item.productName << " x" << item.quantity
                  << " @ $" << item.unitPrice << "\n";
    std::cout << "Total: $" << dto.totalAmount << "\n";
}
```

**Pros**:
- Minimizes network calls — one DTO carries all needed data
- Decouples consumer from internal domain structure
- Serializable — maps cleanly to JSON/protobuf/XML

**Cons**:
- Requires maintaining assemblers/mappers to convert domain objects ↔ DTOs
- Can proliferate — many DTOs for different use cases
- Risk of "anemic domain model" if DTOs are used internally too

**Related**: Remote Facade (sends/receives DTOs), Mapper (assembles DTOs from domain objects), Value Object (similar structure but has domain meaning and equality semantics)
