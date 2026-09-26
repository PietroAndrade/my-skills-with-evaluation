# Behavioral Patterns

Patterns concerned with algorithms and the assignment of responsibilities between objects.

---

## Observer

**Problem**: Multiple objects need to be notified about state changes in another object without tight coupling.

**When to Use**:
- Changes in one object require changing others, and the set of objects is dynamic or unknown
- GUI classes need to react to user actions
- Objects need to observe others temporarily or in specific cases

**Structure**:
- `Publisher` - Maintains subscriber list, notifies on state changes
- `Subscriber` - Interface with update method
- `ConcreteSubscriber` - Implements update to react to changes

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

// Observer interface
class Observer {
public:
    virtual ~Observer() = default;
    virtual void update(const std::string& eventType, const std::string& data) = 0;
};

// Publisher
class EventManager {
    std::unordered_map<std::string, std::vector<Observer*>> observers;
public:
    void subscribe(const std::string& eventType, Observer* observer) {
        observers[eventType].push_back(observer);
    }

    void unsubscribe(const std::string& eventType, Observer* observer) {
        auto& subs = observers[eventType];
        subs.erase(std::remove(subs.begin(), subs.end(), observer), subs.end());
    }

    void notify(const std::string& eventType, const std::string& data) {
        if (observers.count(eventType)) {
            for (auto* obs : observers[eventType])
                obs->update(eventType, data);
        }
    }
};

class Editor {
    EventManager events;
    std::string file;
public:
    EventManager& getEvents() { return events; }

    void openFile(const std::string& path) {
        file = path;
        events.notify("open", path);
    }

    void saveFile() {
        if (!file.empty())
            events.notify("save", file);
    }
};

// Concrete observers
class EmailNotificationListener : public Observer {
    std::string email;
public:
    explicit EmailNotificationListener(std::string email) : email(std::move(email)) {}

    void update(const std::string& /*eventType*/, const std::string& data) override {
        std::cout << "Email to " << email << ": File " << data << " was saved\n";
    }
};

class LoggingListener : public Observer {
public:
    void update(const std::string& /*eventType*/, const std::string& data) override {
        std::cout << "Log: Someone performed operation on file " << data << "\n";
    }
};

// Usage
int main() {
    Editor editor;
    EmailNotificationListener emailListener("admin@example.com");
    LoggingListener loggingListener;

    editor.getEvents().subscribe("save", &emailListener);
    editor.getEvents().subscribe("save", &loggingListener);

    editor.openFile("test.txt");
    editor.saveFile();
}
```

**Pros**:
- Open/Closed - add new subscribers without modifying publisher
- Establish relationships at runtime

**Cons**:
- Subscribers notified in random order
- Can cause dangling pointers if observer is destroyed before unsubscribing

**Related**: Mediator (eliminates direct connections), Command/Chain of Responsibility (different sender-receiver relationships)

---

## Strategy

**Problem**: Multiple algorithm variants within a single class cause code bloat and make changes risky.

**When to Use**:
- Need to use different variants of an algorithm and switch at runtime
- Similar classes differ only in behavior execution
- Want to isolate business logic from implementation details
- Class has massive conditionals selecting between algorithms

**Structure**:
- `Context` - Maintains reference to strategy
- `Strategy` - Interface for algorithms
- `ConcreteStrategy` - Implements specific algorithm

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// Strategy interface
class PaymentStrategy {
public:
    virtual ~PaymentStrategy() = default;
    virtual void pay(double amount) const = 0;
};

// Concrete strategies
class CreditCardPayment : public PaymentStrategy {
    std::string cardNumber;
public:
    explicit CreditCardPayment(std::string cardNumber)
        : cardNumber(std::move(cardNumber)) {}

    void pay(double amount) const override {
        std::cout << "Paid $" << amount << " using credit card " << cardNumber << "\n";
    }
};

class PayPalPayment : public PaymentStrategy {
    std::string email;
public:
    explicit PayPalPayment(std::string email) : email(std::move(email)) {}

    void pay(double amount) const override {
        std::cout << "Paid $" << amount << " using PayPal account " << email << "\n";
    }
};

class CryptoPayment : public PaymentStrategy {
    std::string walletAddress;
public:
    explicit CryptoPayment(std::string walletAddress)
        : walletAddress(std::move(walletAddress)) {}

    void pay(double amount) const override {
        std::cout << "Paid $" << amount << " using crypto wallet " << walletAddress << "\n";
    }
};

// Context
class ShoppingCart {
    std::vector<std::string> items;
    std::unique_ptr<PaymentStrategy> paymentStrategy;
public:
    void addItem(const std::string& item) { items.push_back(item); }

    void setPaymentStrategy(std::unique_ptr<PaymentStrategy> strategy) {
        paymentStrategy = std::move(strategy);
    }

    void checkout(double amount) const {
        if (!paymentStrategy) {
            std::cout << "Please select a payment method\n";
            return;
        }
        paymentStrategy->pay(amount);
    }
};

// Usage - swap strategies at runtime
int main() {
    ShoppingCart cart;
    cart.addItem("Book");
    cart.addItem("Laptop");

    cart.setPaymentStrategy(std::make_unique<CreditCardPayment>("1234-5678-9012-3456"));
    cart.checkout(500.0);

    cart.setPaymentStrategy(std::make_unique<PayPalPayment>("user@example.com"));
    cart.checkout(500.0);
}
```

**Pros**:
- Swap algorithms at runtime
- Isolate implementation from usage
- Use composition instead of inheritance
- Open/Closed - add new strategies without changing context

**Cons**:
- Overkill for few rarely-changing algorithms
- Clients must understand strategy differences
- Modern C++ lambdas/`std::function` may reduce need

**Related**: Command (turns operations to objects), State (strategies may be aware of each other), Template Method (class-level vs object-level)

---

## Command

**Problem**: Tight coupling between GUI elements and business logic makes code difficult to reuse and extend.

**When to Use**:
- Parameterize objects with operations
- Queue, schedule, or execute operations remotely
- Implement reversible operations (undo/redo)
- Want to log or persist operations

**Structure**:
- `Command` - Interface with execute method
- `ConcreteCommand` - Implements execute, stores receiver and parameters
- `Invoker` - Triggers commands
- `Receiver` - Contains business logic

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// Receiver
class TextEditor {
    std::string text;
public:
    const std::string& getText() const { return text; }
    void insertText(const std::string& str) { text += str; }
    void deleteText(size_t length) {
        if (length <= text.size())
            text.erase(text.size() - length);
    }
};

// Command interface
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

// Concrete commands
class InsertTextCommand : public Command {
    TextEditor& editor;
    std::string text;
public:
    InsertTextCommand(TextEditor& editor, std::string text)
        : editor(editor), text(std::move(text)) {}

    void execute() override { editor.insertText(text); }
    void undo() override    { editor.deleteText(text.size()); }
};

class DeleteTextCommand : public Command {
    TextEditor& editor;
    size_t length;
    std::string deletedText;
public:
    DeleteTextCommand(TextEditor& editor, size_t length)
        : editor(editor), length(length) {}

    void execute() override {
        const auto& t = editor.getText();
        if (length <= t.size())
            deletedText = t.substr(t.size() - length);
        editor.deleteText(length);
    }
    void undo() override { editor.insertText(deletedText); }
};

// Invoker
class CommandHistory {
    std::vector<std::unique_ptr<Command>> history;
    int current = -1;
public:
    void execute(std::unique_ptr<Command> command) {
        history.erase(history.begin() + current + 1, history.end());
        command->execute();
        history.push_back(std::move(command));
        ++current;
    }

    void undo() {
        if (current >= 0) {
            history[current]->undo();
            --current;
        }
    }

    void redo() {
        if (current < static_cast<int>(history.size()) - 1) {
            ++current;
            history[current]->execute();
        }
    }
};

// Usage
int main() {
    TextEditor editor;
    CommandHistory history;

    history.execute(std::make_unique<InsertTextCommand>(editor, "Hello "));
    history.execute(std::make_unique<InsertTextCommand>(editor, "World"));
    std::cout << editor.getText() << "\n"; // "Hello World"

    history.undo();
    std::cout << editor.getText() << "\n"; // "Hello "

    history.redo();
    std::cout << editor.getText() << "\n"; // "Hello World"
}
```

**Pros**:
- Decouples invokers from performers (Single Responsibility)
- Open/Closed - new commands without modifying existing code
- Implements undo/redo
- Supports deferred execution
- Compose simple commands into complex ones

**Cons**:
- Adds new layer between senders and receivers

**Related**: Chain of Responsibility/Mediator/Observer (sender-receiver patterns), Memento (undo state), Strategy (operation to object)

---

## State

**Problem**: Objects behave differently based on state, leading to massive conditionals that are hard to maintain.

**When to Use**:
- Object behavior depends on state with numerous states and frequent changes
- Class has bulky conditionals altering behavior based on field values
- Similar states have duplicate code in state machine

**Structure**:
- `Context` - Maintains current state, delegates requests
- `State` - Interface declaring state-specific methods
- `ConcreteState` - Implements behavior for specific state

**C++ Example**:
```cpp
#include <iostream>
#include <memory>

class VendingMachine;

// State interface
class State {
public:
    virtual ~State() = default;
    virtual void insertCoin(VendingMachine& m) = 0;
    virtual void ejectCoin(VendingMachine& m) = 0;
    virtual void selectProduct(VendingMachine& m) = 0;
    virtual void dispense(VendingMachine& m) = 0;
};

// Forward declarations of concrete states
class NoCoinState;
class HasCoinState;
class DispensingState;
class SoldOutState;

// Context
class VendingMachine {
    std::unique_ptr<State> state;
    bool hasProduct = true;
public:
    VendingMachine();

    void setState(std::unique_ptr<State> s) { state = std::move(s); }
    void insertCoin()    { state->insertCoin(*this); }
    void ejectCoin()     { state->ejectCoin(*this); }
    void selectProduct() { state->selectProduct(*this); }
    void dispense()      { state->dispense(*this); }

    bool hasProductAvailable() const { return hasProduct; }
    void releaseProduct() {
        if (hasProduct) {
            std::cout << "Product dispensed\n";
            hasProduct = false;
        }
    }
};

class NoCoinState : public State {
public:
    void insertCoin(VendingMachine& m) override;
    void ejectCoin(VendingMachine&) override    { std::cout << "No coin to eject\n"; }
    void selectProduct(VendingMachine&) override { std::cout << "Insert coin first\n"; }
    void dispense(VendingMachine&) override      { std::cout << "Insert coin first\n"; }
};

class HasCoinState : public State {
public:
    void insertCoin(VendingMachine&) override   { std::cout << "Coin already inserted\n"; }
    void ejectCoin(VendingMachine& m) override;
    void selectProduct(VendingMachine& m) override;
    void dispense(VendingMachine&) override      { std::cout << "Select product first\n"; }
};

class DispensingState : public State {
public:
    void insertCoin(VendingMachine&) override   { std::cout << "Please wait, dispensing product\n"; }
    void ejectCoin(VendingMachine&) override    { std::cout << "Cannot eject, dispensing product\n"; }
    void selectProduct(VendingMachine&) override { std::cout << "Already dispensing\n"; }
    void dispense(VendingMachine& m) override;
};

class SoldOutState : public State {
public:
    void insertCoin(VendingMachine&) override   { std::cout << "Sold out\n"; }
    void ejectCoin(VendingMachine&) override    { std::cout << "No coin to eject\n"; }
    void selectProduct(VendingMachine&) override { std::cout << "Sold out\n"; }
    void dispense(VendingMachine&) override      { std::cout << "Sold out\n"; }
};

// Definitions (after all classes are declared)
VendingMachine::VendingMachine() : state(std::make_unique<NoCoinState>()) {}

void NoCoinState::insertCoin(VendingMachine& m) {
    std::cout << "Coin inserted\n";
    m.setState(std::make_unique<HasCoinState>());
}
void HasCoinState::ejectCoin(VendingMachine& m) {
    std::cout << "Coin ejected\n";
    m.setState(std::make_unique<NoCoinState>());
}
void HasCoinState::selectProduct(VendingMachine& m) {
    std::cout << "Product selected\n";
    m.setState(std::make_unique<DispensingState>());
}
void DispensingState::dispense(VendingMachine& m) {
    m.releaseProduct();
    m.setState(m.hasProductAvailable()
        ? std::unique_ptr<State>(std::make_unique<NoCoinState>())
        : std::unique_ptr<State>(std::make_unique<SoldOutState>()));
}

// Usage
int main() {
    VendingMachine machine;
    machine.insertCoin();
    machine.selectProduct();
    machine.dispense();
}
```

**Pros**:
- Single Responsibility - organize state-related code
- Open/Closed - add new states without modifying existing
- Eliminates bulky conditionals

**Cons**:
- Overkill for simple state machines with few states

**Related**: Bridge/Strategy (structure), but State allows states to be aware of each other and change context state

---

## Template Method

**Problem**: Multiple classes implement similar algorithms with slight variations, causing code duplication.

**When to Use**:
- Let clients extend specific algorithm steps, not the whole structure
- Several classes contain nearly identical algorithms with minor differences
- Want to eliminate code duplication while preserving structure

**Structure**:
- `AbstractClass` - Defines template method and abstract steps
- `ConcreteClass` - Implements abstract steps

**C++ Example**:
```cpp
#include <iostream>
#include <string>

// Abstract class
class DataMiner {
public:
    virtual ~DataMiner() = default;

    // Template method — defines the algorithm skeleton
    void mine(const std::string& path) {
        std::string file     = openFile(path);
        std::string rawData  = extractData(file);
        std::string data     = parseData(rawData);
        std::string analysis = analyzeData(data);
        sendReport(analysis);
        closeFile(file);
    }

protected:
    virtual std::string openFile(const std::string& path) {
        std::cout << "Opening file: " << path << "\n";
        return path;
    }

    virtual void closeFile(const std::string& file) {
        std::cout << "Closing file: " << file << "\n";
    }

    // Abstract steps — must be implemented by subclasses
    virtual std::string extractData(const std::string& file) = 0;
    virtual std::string parseData(const std::string& data) = 0;

    // Hook — can be overridden
    virtual std::string analyzeData(const std::string& /*data*/) {
        std::cout << "Performing basic analysis\n";
        return "Basic analysis results";
    }

    virtual void sendReport(const std::string& analysis) {
        std::cout << "Report: " << analysis << "\n";
    }
};

class PDFDataMiner : public DataMiner {
protected:
    std::string extractData(const std::string& /*file*/) override {
        std::cout << "Extracting data from PDF\n";
        return "PDF raw data";
    }
    std::string parseData(const std::string& data) override {
        std::cout << "Parsing PDF data\n";
        return "{ type: PDF, content: " + data + " }";
    }
};

class CSVDataMiner : public DataMiner {
protected:
    std::string extractData(const std::string& /*file*/) override {
        std::cout << "Extracting data from CSV\n";
        return "CSV raw data";
    }
    std::string parseData(const std::string& data) override {
        std::cout << "Parsing CSV data\n";
        return "{ type: CSV, content: " + data + " }";
    }
    std::string analyzeData(const std::string& data) override {
        std::cout << "Performing advanced CSV analysis\n";
        return "CSV analysis: " + data;
    }
};

// Usage
int main() {
    PDFDataMiner pdfMiner;
    pdfMiner.mine("data.pdf");

    std::cout << "---\n";

    CSVDataMiner csvMiner;
    csvMiner.mine("data.csv");
}
```

**Pros**:
- Clients override only specific algorithm parts
- Pull duplicate code to superclass

**Cons**:
- Clients limited by provided skeleton
- Can violate Liskov Substitution Principle
- Harder to maintain with many steps

**Related**: Factory Method (specialization of Template Method), Strategy (uses composition and runtime switching vs inheritance)

**Note**: Template Method works at class level (static), Strategy at object level (dynamic runtime switching).

---

## Memento

**Problem**: Need to save and restore an object's previous state without breaking encapsulation or exposing internal details.

**When to Use**:
- Need to produce snapshots of object state to restore previous states
- Implement undo/redo functionality
- Handle transaction rollback on errors
- Direct access to object fields would violate encapsulation

**Structure**:
- `Originator` - Creates memento containing snapshot of its state
- `Memento` - Value object acting as snapshot of originator's state
- `Caretaker` - Knows when and why to save/restore originator state

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// Memento — stores snapshot (only Originator can create/read it)
class EditorMemento {
    std::string content;
    size_t cursorPosition;

    EditorMemento(std::string content, size_t cursor)
        : content(std::move(content)), cursorPosition(cursor) {}

    friend class TextEditor;
public:
    const std::string& getContent() const { return content; }
    size_t getCursorPosition() const       { return cursorPosition; }
};

// Originator — creates and restores from mementos
class TextEditor {
    std::string content;
    size_t cursorPosition = 0;
public:
    void type(const std::string& text) {
        content.insert(cursorPosition, text);
        cursorPosition += text.size();
    }

    void setCursor(size_t position) {
        cursorPosition = std::min(position, content.size());
    }

    const std::string& getContent() const { return content; }

    EditorMemento save() const {
        return EditorMemento(content, cursorPosition);
    }

    void restore(const EditorMemento& memento) {
        content        = memento.getContent();
        cursorPosition = memento.getCursorPosition();
    }
};

// Caretaker — manages mementos
class History {
    std::vector<EditorMemento> mementos;
    int current = -1;
public:
    void push(EditorMemento memento) {
        mementos.erase(mementos.begin() + current + 1, mementos.end());
        mementos.push_back(std::move(memento));
        ++current;
    }

    const EditorMemento* undo() {
        if (current > 0) return &mementos[--current];
        return nullptr;
    }

    const EditorMemento* redo() {
        if (current < static_cast<int>(mementos.size()) - 1)
            return &mementos[++current];
        return nullptr;
    }
};

// Usage
int main() {
    TextEditor editor;
    History history;

    history.push(editor.save());

    editor.type("Hello ");
    history.push(editor.save());

    editor.type("World");
    history.push(editor.save());

    std::cout << editor.getContent() << "\n"; // "Hello World"

    if (const auto* s = history.undo()) {
        editor.restore(*s);
        std::cout << editor.getContent() << "\n"; // "Hello "
    }

    if (const auto* s = history.undo()) {
        editor.restore(*s);
        std::cout << editor.getContent() << "\n"; // ""
    }

    if (const auto* s = history.redo()) {
        editor.restore(*s);
        std::cout << editor.getContent() << "\n"; // "Hello "
    }
}
```

**Pros**:
- Produce snapshots without violating encapsulation
- Simplifies originator by delegating history management to caretakers
- Maintains clean separation between state management and business logic

**Cons**:
- High RAM consumption if snapshots created frequently
- Caretakers must track originator lifecycle for cleanup
- Value semantics in C++ avoid integrity issues present in dynamic languages

**Related**: Command + Memento (commands execute operations, mementos store pre-execution state for undo), Iterator (can capture iteration state), Prototype (simpler alternative for basic cloning)

**Note**: Memento is often used with Command pattern - Command handles the operation execution while Memento handles state restoration for undo.
