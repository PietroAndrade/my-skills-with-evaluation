# Creational Patterns

Patterns that handle object creation mechanisms, increasing flexibility and reuse of existing code.

---

## Factory Method

**Problem**: Creating objects without specifying exact classes causes tight coupling between creator and concrete types.

**When to Use**:
- You don't know exact types beforehand
- You want to provide extension points for library users
- You need to reuse existing objects instead of rebuilding them

**Structure**:
- `Creator` - Declares factory method
- `ConcreteCreator` - Overrides factory method
- `Product` - Interface for objects
- `ConcreteProduct` - Implements product interface

**C++ Example**:
```cpp
#include <iostream>
#include <memory>

// Product interface
class Transport {
public:
    virtual ~Transport() = default;
    virtual void deliver() const = 0;
};

// Concrete products
class Truck : public Transport {
public:
    void deliver() const override {
        std::cout << "Deliver by land in a box\n";
    }
};

class Ship : public Transport {
public:
    void deliver() const override {
        std::cout << "Deliver by sea in a container\n";
    }
};

// Creator
class Logistics {
public:
    virtual ~Logistics() = default;

    // Factory method
    virtual std::unique_ptr<Transport> createTransport() const = 0;

    void planDelivery() const {
        auto transport = createTransport();
        transport->deliver();
    }
};

// Concrete creators
class RoadLogistics : public Logistics {
public:
    std::unique_ptr<Transport> createTransport() const override {
        return std::make_unique<Truck>();
    }
};

class SeaLogistics : public Logistics {
public:
    std::unique_ptr<Transport> createTransport() const override {
        return std::make_unique<Ship>();
    }
};

// Usage
int main() {
    RoadLogistics road;
    road.planDelivery(); // Deliver by land

    SeaLogistics sea;
    sea.planDelivery();  // Deliver by sea
}
```

**Pros**:
- Avoids coupling between creator and products
- Single Responsibility - centralized creation code
- Open/Closed - add new products without breaking existing code

**Cons**:
- Can require many subclasses

**Related**: Often evolves to Abstract Factory, Prototype, or Builder

---

## Abstract Factory

**Problem**: Need to create families of related objects while ensuring compatibility without depending on concrete classes.

**When to Use**:
- Code needs to work with various families of related products
- You can't predict product variants beforehand
- A class has multiple factory methods obscuring its main responsibility

**Structure**:
- `AbstractFactory` - Interface for creating product families
- `ConcreteFactory` - Implements creation methods for variants
- `AbstractProduct` - Interfaces for each product type
- `ConcreteProduct` - Implementations for each variant

**C++ Example**:
```cpp
#include <iostream>
#include <memory>
#include <string>

// Abstract products
class Button {
public:
    virtual ~Button() = default;
    virtual void render() const = 0;
};

class Checkbox {
public:
    virtual ~Checkbox() = default;
    virtual void render() const = 0;
};

// Windows products
class WindowsButton : public Button {
public:
    void render() const override { std::cout << "Render Windows button\n"; }
};

class WindowsCheckbox : public Checkbox {
public:
    void render() const override { std::cout << "Render Windows checkbox\n"; }
};

// Mac products
class MacButton : public Button {
public:
    void render() const override { std::cout << "Render Mac button\n"; }
};

class MacCheckbox : public Checkbox {
public:
    void render() const override { std::cout << "Render Mac checkbox\n"; }
};

// Abstract factory
class GUIFactory {
public:
    virtual ~GUIFactory() = default;
    virtual std::unique_ptr<Button>   createButton()   const = 0;
    virtual std::unique_ptr<Checkbox> createCheckbox() const = 0;
};

// Concrete factories
class WindowsFactory : public GUIFactory {
public:
    std::unique_ptr<Button>   createButton()   const override { return std::make_unique<WindowsButton>(); }
    std::unique_ptr<Checkbox> createCheckbox() const override { return std::make_unique<WindowsCheckbox>(); }
};

class MacFactory : public GUIFactory {
public:
    std::unique_ptr<Button>   createButton()   const override { return std::make_unique<MacButton>(); }
    std::unique_ptr<Checkbox> createCheckbox() const override { return std::make_unique<MacCheckbox>(); }
};

// Client code — works with any factory
void renderUI(const GUIFactory& factory) {
    auto button   = factory.createButton();
    auto checkbox = factory.createCheckbox();
    button->render();
    checkbox->render();
}

// Usage
int main() {
    const std::string os = "Windows";
    std::unique_ptr<GUIFactory> factory;

    if (os == "Windows")
        factory = std::make_unique<WindowsFactory>();
    else
        factory = std::make_unique<MacFactory>();

    renderUI(*factory);
}
```

**Pros**:
- Ensures product compatibility
- Avoids tight coupling to concrete products
- Single Responsibility - centralized product creation
- Open/Closed - new variants without breaking existing code

**Cons**:
- Significant complexity with many new interfaces/classes

**Related**: Often evolves from Factory Method; can be implemented with Prototype or Builder

---

## Builder

**Problem**: Constructing complex objects with many optional parameters leads to giant constructors or subclass explosion.

**When to Use**:
- Telescoping constructors with many optional parameters
- Need to create different representations of a product
- Constructing complex composite objects like trees

**Structure**:
- `Builder` - Interface for construction steps
- `ConcreteBuilder` - Implements steps for specific representation
- `Director` - Defines construction order (optional)
- `Product` - Complex object being built

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <optional>

// Product
struct House {
    std::string walls;
    int doors    = 0;
    int windows  = 0;
    std::string roof;
    bool garage  = false;

    void describe() const {
        std::cout << "House with " << walls << " walls, "
                  << doors << " doors, " << windows << " windows, "
                  << roof << " roof, garage: " << std::boolalpha << garage << "\n";
    }
};

// Builder
class HouseBuilder {
    House house;
public:
    HouseBuilder& buildWalls(const std::string& material) {
        house.walls = material;
        return *this;
    }
    HouseBuilder& buildDoors(int count) {
        house.doors = count;
        return *this;
    }
    HouseBuilder& buildWindows(int count) {
        house.windows = count;
        return *this;
    }
    HouseBuilder& buildRoof(const std::string& type) {
        house.roof = type;
        return *this;
    }
    HouseBuilder& buildGarage() {
        house.garage = true;
        return *this;
    }

    House build() {
        House result = std::move(house);
        house = {};  // reset
        return result;
    }
};

// Usage
int main() {
    HouseBuilder builder;
    House house = builder
        .buildWalls("brick")
        .buildDoors(2)
        .buildWindows(6)
        .buildRoof("tile")
        .buildGarage()
        .build();

    house.describe();
}
```

**Pros**:
- Construct objects step-by-step
- Reuse construction code for different representations
- Single Responsibility - isolates complex construction

**Cons**:
- Increases code complexity with new classes

**Related**: Can combine with Composite for tree construction; often implemented alongside other creational patterns

---

## Singleton

**Problem**: Need to ensure a class has only one instance with global access point.

**When to Use**:
- Single shared resource (database, file) needs controlled access
- Need stricter control than global variables
- ⚠️ **Use sparingly** - often indicates design issues

**Structure**:
- Single class with private constructor and static instance field
- Static method to get instance (lazy initialization)

**C++ Example**:
```cpp
#include <iostream>
#include <mutex>
#include <string>

class Database {
    std::string connection;

    // Private constructor prevents direct instantiation
    Database() : connection("Connected to database") {}

public:
    // Delete copy/move to enforce single instance
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // Thread-safe lazy initialization via local static (C++11 guarantees)
    static Database& getInstance() {
        static Database instance;
        return instance;
    }

    void query(const std::string& sql) const {
        std::cout << "Executing: " << sql << "\n";
    }
};

// Usage
int main() {
    Database& db1 = Database::getInstance();
    Database& db2 = Database::getInstance();

    std::cout << std::boolalpha
              << (&db1 == &db2) << "\n"; // true — same instance

    db1.query("SELECT * FROM users");
}
```

**Pros**:
- Guarantees single instance
- Global access point
- Thread-safe lazy initialization (C++11 magic statics)

**Cons**:
- Violates Single Responsibility Principle
- Can mask poor design
- Difficult to test (no way to reset state)
- **Better alternative**: Use dependency injection

**Related**: Facade can become Singleton; many patterns can be implemented as Singletons

**⚠️ Warning**: Overuse of Singleton is an anti-pattern. Consider dependency injection instead.
