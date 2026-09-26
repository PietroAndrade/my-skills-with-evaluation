# Structural Patterns

Patterns that explain how to assemble objects and classes into larger structures while keeping these structures flexible and efficient.

---

## Adapter

**Problem**: Incompatible interfaces prevent objects from working together (e.g., integrating a JSON-based library into an XML-based application).

**When to Use**:
- Need to use an existing class with an incompatible interface
- Want to reuse subclasses lacking common functionality
- Working with third-party libraries that don't match your interface

**Structure**:
- `Target` - Interface expected by client
- `Adapter` - Implements target, delegates to adaptee
- `Adaptee` - Existing class with incompatible interface
- `Client` - Works with target interface

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <algorithm>

// Target — interface the client expects
class MediaPlayer {
public:
    virtual ~MediaPlayer() = default;
    virtual void play(const std::string& filename) = 0;
};

// Adaptee — existing class with a different interface
class AdvancedMediaPlayer {
public:
    void playVlc(const std::string& filename) {
        std::cout << "Playing VLC file: " << filename << "\n";
    }
    void playMp4(const std::string& filename) {
        std::cout << "Playing MP4 file: " << filename << "\n";
    }
};

// Adapter — bridges Target and Adaptee
class MediaAdapter : public MediaPlayer {
    AdvancedMediaPlayer advancedPlayer;
public:
    void play(const std::string& filename) override {
        if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".vlc")
            advancedPlayer.playVlc(filename);
        else if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".mp4")
            advancedPlayer.playMp4(filename);
    }
};

// Enhanced client using adapter for unsupported formats
class EnhancedAudioPlayer : public MediaPlayer {
    MediaAdapter adapter;
public:
    void play(const std::string& filename) override {
        auto ext = filename.substr(filename.rfind('.'));
        if (ext == ".mp3")
            std::cout << "Playing MP3 file: " << filename << "\n";
        else if (ext == ".vlc" || ext == ".mp4")
            adapter.play(filename);
        else
            std::cout << "Invalid format: " << filename << "\n";
    }
};

// Usage
int main() {
    EnhancedAudioPlayer player;
    player.play("song.mp3");
    player.play("video.mp4");
    player.play("movie.vlc");
}
```

**Pros**:
- Single Responsibility - separates interface conversion from business logic
- Open/Closed - add new adapters without breaking existing code

**Cons**:
- Increases overall complexity
- Sometimes simpler to modify the service class directly

**Related**: Bridge (designed upfront vs. retrofitted), Decorator (changes interface), Facade (new simple interface)

---

## Decorator

**Problem**: Need to add behaviors to objects dynamically without creating an explosion of subclasses.

**When to Use**:
- Assign extra behaviors at runtime without breaking existing code
- Inheritance is impractical (e.g., `final` classes)
- Need to combine multiple behaviors flexibly

**Structure**:
- `Component` - Interface for objects
- `ConcreteComponent` - Basic implementation
- `Decorator` - Base class wrapping a component
- `ConcreteDecorator` - Adds specific behavior

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>

// Component interface
class DataSource {
public:
    virtual ~DataSource() = default;
    virtual void writeData(const std::string& data) = 0;
    virtual std::string readData() const = 0;
};

// Concrete component
class FileDataSource : public DataSource {
    std::string filename;
    std::string data;
public:
    explicit FileDataSource(std::string filename) : filename(std::move(filename)) {}

    void writeData(const std::string& d) override {
        std::cout << "Writing to file: " << filename << "\n";
        data = d;
    }
    std::string readData() const override {
        std::cout << "Reading from file: " << filename << "\n";
        return data;
    }
};

// Base decorator
class DataSourceDecorator : public DataSource {
protected:
    std::unique_ptr<DataSource> wrappee;
public:
    explicit DataSourceDecorator(std::unique_ptr<DataSource> source)
        : wrappee(std::move(source)) {}

    void writeData(const std::string& data) override { wrappee->writeData(data); }
    std::string readData() const override             { return wrappee->readData(); }
};

// Concrete decorators
class EncryptionDecorator : public DataSourceDecorator {
    static std::string encode(const std::string& s) {
        std::string enc = s;
        for (auto& c : enc) c = static_cast<char>(c + 1); // simple Caesar shift
        return enc;
    }
    static std::string decode(const std::string& s) {
        std::string dec = s;
        for (auto& c : dec) c = static_cast<char>(c - 1);
        return dec;
    }
public:
    using DataSourceDecorator::DataSourceDecorator;

    void writeData(const std::string& data) override {
        std::cout << "Encrypting data\n";
        wrappee->writeData(encode(data));
    }
    std::string readData() const override {
        std::cout << "Decrypting data\n";
        return decode(wrappee->readData());
    }
};

class CompressionDecorator : public DataSourceDecorator {
public:
    using DataSourceDecorator::DataSourceDecorator;

    void writeData(const std::string& data) override {
        std::cout << "Compressing data\n";
        wrappee->writeData("compressed(" + data + ")");
    }
    std::string readData() const override {
        std::cout << "Decompressing data\n";
        std::string d = wrappee->readData();
        // Strip "compressed(" prefix and ")" suffix
        if (d.size() > 11 && d.substr(0, 11) == "compressed(")
            d = d.substr(11, d.size() - 12);
        return d;
    }
};

// Usage — layer decorators
int main() {
    auto source = std::make_unique<FileDataSource>("data.txt");
    auto encrypted   = std::make_unique<EncryptionDecorator>(std::move(source));
    auto compressed  = std::make_unique<CompressionDecorator>(std::move(encrypted));

    compressed->writeData("Hello World");
    std::cout << compressed->readData() << "\n";
}
```

**Pros**:
- Extend behavior without new subclasses
- Add/remove responsibilities at runtime
- Combine multiple behaviors
- Single Responsibility - divide monolithic class

**Cons**:
- Hard to remove specific wrappers from stack
- Behavior depends on decorator order
- Initial configuration can be complex

**Related**: Adapter (changes interface), Proxy (different purpose), Composite (aggregates results)

---

## Facade

**Problem**: Working with complex subsystems requires extensive initialization, dependency tracking, and correct method execution order.

**When to Use**:
- Need a simple interface to a complex subsystem
- Subsystem is complex with many configuration details
- Want to layer a subsystem with entry points at each level

**Structure**:
- `Facade` - Provides simple interface to subsystem
- `Subsystem Classes` - Implement functionality, unaware of facade

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>

// Complex subsystem classes
class CPU {
public:
    void freeze()              { std::cout << "CPU: Freezing\n"; }
    void jump(int position)    { std::cout << "CPU: Jumping to " << position << "\n"; }
    void execute()             { std::cout << "CPU: Executing\n"; }
};

class Memory {
public:
    void load(int position, const std::string& data) {
        std::cout << "Memory: Loading \"" << data << "\" at " << position << "\n";
    }
};

class HardDrive {
public:
    std::string read(int sector, int size) {
        std::cout << "HardDrive: Reading " << size << " bytes from sector " << sector << "\n";
        return "boot data";
    }
};

// Facade — hides all the complexity behind a simple interface
class ComputerFacade {
    CPU       cpu;
    Memory    memory;
    HardDrive hardDrive;
public:
    void start() {
        std::cout << "Starting computer...\n";
        cpu.freeze();
        std::string bootData = hardDrive.read(0, 1024);
        memory.load(0, bootData);
        cpu.jump(0);
        cpu.execute();
        std::cout << "Computer started!\n";
    }
};

// Usage — simple interface hides complexity
int main() {
    ComputerFacade computer;
    computer.start();
}
```

**Pros**:
- Isolates code from subsystem complexity
- Provides clean, simple interface

**Cons**:
- Can become a god object coupled to all classes

**Related**: Adapter (wraps single object), Abstract Factory (hides creation), Mediator (centralizes communication)

**Note**: Facade defines new interface; Adapter makes existing interfaces compatible.
