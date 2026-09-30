---
name: cpp-modernize
description: Transforms C-style patterns in legacy C++ code to modern C++11+. Use when reviewing or refactoring old code that uses #define, NULL, malloc/free, manual lock/unlock, int-as-bool, thread-unsafe singletons, or bzero. Trigger on: "modernize this code", "refactor legacy C++", "replace #define", "NULL to nullptr", "replace malloc", "make this thread-safe", "fix this singleton", "replace bzero", "int return bool".
---

## Goal

Transform specific C-style anti-patterns to their C++11+ equivalents. Each rule below is a mechanical substitution — apply all that are present. Read `docs/cpp-conventions.md` for naming conventions that apply after the transform.

---

## Rules

### `#define` → `constexpr` / `enum class`

`#define` has no type, no scope, and can silently shadow names in every translation unit that includes the header.

**Numeric or size constants:**
```cpp
// before
#define BUFSIZE 65536
#define DNS_TYPE_A 0x0001

// after
static constexpr std::size_t BUFFER_SIZE = 65536;
static constexpr uint16_t    DNS_TYPE_A  = 0x0001;
```

**String constants:**
```cpp
// before
#define PERIOD "."

// after
static constexpr std::string_view PERIOD = ".";
```

**Groups of related values → `enum class`:**
```cpp
// before
#define STATUS_OK    0
#define STATUS_ERROR 1
#define STATUS_RETRY 2

// after
enum class Status { Ok, Error, Retry };
```

**Macro-as-accessor** (e.g. `#define CONFIG Conf::getInstance()`) — eliminate entirely. Callers call `Conf::getInstance()` directly. These macros are the worst kind: they look like variables, hide a function call, and break grep.

Place `constexpr` constants in the class body (private or public as appropriate) or in an anonymous namespace in the `.cpp` if they are implementation details.

---

### Magic numbers → named `constexpr`

Any literal number appearing inline in logic is a magic number. Name it, type it precisely, and scope it to the class or struct where it is used.

**Type selection:**
- Byte masks, bit fields, protocol flags → `uint8_t`, `uint16_t`, `uint32_t` from `<cstdint>` — use the exact width the protocol requires
- Sizes and offsets → `std::size_t`
- Signed counts or error codes → `int`

**Placement:** `static constexpr` inside the class body. Private if it is an implementation detail; public if callers need to reference it (e.g. a fixed packet size).

```cpp
// before — in a method body
uint16_t flags = value & 0x000F;
if (type == 1) { /* IPv4 */ }
if (type == 28) { /* IPv6 */ }
packet[2] = (ttl >> 8) & 0xFF;

// after — constants scoped to the class
struct ResolutionResponse {
    static constexpr uint16_t RCODE_MASK    = 0x000F;
    static constexpr uint16_t QUESTION_IPV4 = 1;
    static constexpr uint16_t QUESTION_IPV6 = 28;
    static constexpr uint8_t  BYTE_MASK     = 0xFF;
    static constexpr uint8_t  BYTE_SHIFT_8  = 8;
    static constexpr uint16_t TIME_TO_LIVE  = 60;
    // ...
};
```

**Bit manipulation constants** — always name both the mask and the shift separately:
```cpp
static constexpr uint8_t BYTE_MASK     = 0xFF;
static constexpr uint8_t BYTE_SHIFT_8  = 8;
static constexpr uint8_t BYTE_SHIFT_16 = 16;
static constexpr uint8_t BYTE_SHIFT_24 = 24;

// usage
packet[0] = static_cast<uint8_t>((value >> BYTE_SHIFT_24) & BYTE_MASK);
packet[1] = static_cast<uint8_t>((value >> BYTE_SHIFT_16) & BYTE_MASK);
```

**Protocol-specific sentinels** — name what the value means, not its representation:
```cpp
static constexpr uint8_t  QUESTION_END_BYTE      = 0;      // 0-length label terminates QNAME
static constexpr uint8_t  COMPRESSION_POINTER    = 0xC0;   // top 2 bits set = pointer, not label
static constexpr size_t   MAX_LABEL_LENGTH       = 63;
static constexpr size_t   QUESTION_FIXED_SIZE    = 4;      // qtype(2) + qclass(2)
```

Never group unrelated constants in a single `constexpr` block or a free-standing `enum` — keep them scoped to the class that uses them.

---

### `NULL` → `nullptr`

`NULL` is a macro that expands to the integer `0`. This causes silent ambiguity in overload resolution and template deduction.

```cpp
// before
Class* Class::m_pThis = NULL;
if (m_pThis == NULL) { ... }

// after
Class* Class::m_pThis = nullptr;
if (m_pThis == nullptr) { ... }
```

Replace every occurrence. There are no exceptions — `nullptr` is always correct where `NULL` was used for pointer comparison.

---

### `malloc` / `realloc` / `free` / `calloc` → RAII containers

C allocators bypass constructors and destructors. They require manual `free`, which is skipped on exceptions.

**Buffer of bytes:**
```cpp
// before
char* reply = (char*) calloc(size, sizeof(char));
// ... use reply ...
free(reply);

// after
std::vector<char> reply(size, 0);
// ... use reply.data() where char* is needed ...
// freed automatically
```

**Growable string/buffer:**
```cpp
// before
char* name = nullptr;
name = (char*) realloc(name, new_size);
// ...
free(name);

// after
std::string name;
name.resize(new_size);
// or std::vector<char> if binary data
```

**Singleton heap allocation:**
```cpp
// before
static MyClass* m_pThis = nullptr;
m_pThis = new MyClass();

// after — Meyers singleton (see singleton rule below)
```

---

### `bzero` → `std::memset`

`bzero` is POSIX-deprecated. Use `std::memset` from `<cstring>`:

```cpp
// before
bzero(&addr, sizeof(struct sockaddr_in));

// after
std::memset(&addr, 0, sizeof(addr));
```

---

### Manual `lock()` / `unlock()` → `std::lock_guard`

Manual unlock is skipped if an exception is thrown between `lock()` and `unlock()` — resource leak or deadlock.

```cpp
// before
m_mutex.lock();
m_cache.erase(key);
m_mutex.unlock();

// after
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_cache.erase(key);
}
```

Use `std::unique_lock` only when you need to unlock early or use `wait()` on a condition variable. For all other cases, `std::lock_guard` is sufficient and communicates the intent.

---

### `int` return used as bool → `bool` / `enum class` / exception

Methods returning `0`/`-1` force callers to remember the convention. Two failure modes (`-1`, `-2`) encoded as magic integers are unreadable.

**Binary success/failure:**
```cpp
// before
int setNonBlocking(int fd);   // returns 0 on success, -1 on failure

// after — option A: throw on failure (preferred for errors the caller can't handle)
void setNonBlocking(int fd);  // throws std::system_error on failure

// after — option B: return bool (when caller needs to branch)
bool setNonBlocking(int fd);
```

**Multiple failure reasons:**
```cpp
// before
int getAddr(...);  // 0 = ok, -1 = not found, -2 = system error

// after
enum class LookupResult { Ok, NotFound, SystemError };
LookupResult getAddr(...);
```

---

### Thread-unsafe singleton → Meyers singleton

The classic `if (m_pThis == nullptr) { m_pThis = new T(); }` pattern is a data race — two threads can both see `nullptr` and both construct an instance.

```cpp
// before
class Conf {
public:
    static Conf* getInstance() {
        if (m_pThis == nullptr) {
            m_pThis = new Conf();
        }
        return m_pThis;
    }
private:
    static Conf* m_pThis;
};

// after — Meyers singleton: static local is initialized exactly once,
// thread-safe by C++11 guarantee (§6.7)
class Conf {
public:
    static Conf& getInstance() {
        static Conf instance;
        return instance;
    }

    Conf(const Conf&) = delete;
    Conf& operator=(const Conf&) = delete;
};
```

Return a reference, not a pointer — callers have no reason to check for null. Delete copy constructor and copy-assignment — a singleton must not be copyable.

---

### `int stopFlag` → `std::atomic<bool>`

A plain `int` accessed from multiple threads without synchronization is undefined behavior.

```cpp
// before
int stopFlag = 0;
// thread A:
server.stopFlag = 1;
// thread B:
if (server.stopFlag) { ... }

// after
std::atomic<bool> m_stop{false};
// thread A:
m_stop.store(true);
// thread B:
if (m_stop.load()) { ... }
```

---

## What this skill does NOT cover

- God methods / large functions — decompose using `cpp-concrete-class` and `cpp-interface`
- Missing interfaces on concrete classes — use `cpp-interface`
- SQL string concatenation — use parameterized queries (`sqlite3_bind_text`); this is a security issue, not a modernization issue
- C-style casts (`(Type*)`) — replace with `reinterpret_cast` / `static_cast` as appropriate, but this requires understanding the intent; don't blindly replace
