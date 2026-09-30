---
name: cpp-wrapping-c-pure-posix
description: Guides C++/POSIX integration: wrapping file descriptors in RAII classes, errno handling, C struct initialization (sockaddr_in/in6, epoll_event), event loop patterns with epoll, interface abstraction over raw POSIX APIs, and C API resources with custom deleters. Trigger on: "wrap a socket", "add epoll support", "POSIX integration", "C API wrapper", "file descriptor", "non-blocking socket", "event loop", "errno handling", "integrate with C library", "wrap C resource", "custom deleter", "getaddrinfo", "freeaddrinfo".
---

## Goal

Produce C++ wrappers that isolate POSIX details behind interfaces. The POSIX noise stays in `.cpp` files — callers see only C++ types and exceptions. Read `docs/cpp-conventions.md` for naming and include guard conventions.

---

## Workflow

1. Ask what POSIX resource needs wrapping (socket fd, epoll fd, eventfd, timer, etc.)
2. Ask what operations the interface must expose (open, close, read, write, etc.)
3. Ask if there is an existing interface to implement or if a new one is needed
4. Ask the lib name
5. Generate: interface header, concrete class header, `.cpp` implementation

---

## Rules

### POSIX headers

Put all POSIX headers in `.cpp` files, not in interface headers. Interface users see only C++ types.

```cpp
// concrete_class.cpp — OK
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <cstring>
```

Exception: if a POSIX type (e.g., `sockaddr_storage`) must appear in a header used by callers, include the minimum POSIX header needed there and document why.

### File descriptor storage and validity

Store raw fd as `int`, initialize to `-1` (invalid). Define a named constant for clarity.

```cpp
static constexpr int INVALID_FD = -1;
static constexpr int VALID_FD   = 0;

int m_fd{INVALID_FD};
```

Check validity before every operation:

```cpp
if (m_fd < VALID_FD) {
    throw std::system_error(EBADF, std::generic_category(), "fd not open");
}
```

### RAII for file descriptors

Constructor initializes fd to `INVALID_FD`. Destructor calls `close()` only when valid. Never rely on compiler-generated destructor for owned fds.

```cpp
SocketHelper::SocketHelper()
    : m_fd(INVALID_FD)
{
}

SocketHelper::~SocketHelper()
{
    if (m_fd >= VALID_FD) {
        ::close(m_fd);
        m_fd = INVALID_FD;
    }
}
```

If the class is not meant to be copied, delete copy constructor and copy-assignment. Add move constructor only when explicit ownership transfer is needed — otherwise leave them deleted.

### C API resources with custom deleters

Many C APIs return an opaque pointer or struct that must be freed with a specific function (e.g., `getaddrinfo`/`freeaddrinfo`, `popen`/`pclose`, sqlite handles). The goal is the same as RAII for fds: never let the raw pointer outlive the scope.

**C++11 — `decltype(&freefn)` as deleter type:**

```cpp
addrinfo* rawResult = nullptr;
const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &rawResult);
std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result(rawResult, &freeaddrinfo);
```

`decltype(&freeaddrinfo)` resolves to the function pointer type `void(*)(addrinfo*)`. This is the correct C++11 idiom: the deleter type is a template parameter, so it must be known at compile time. `&freeaddrinfo` is passed as the runtime deleter.

**C++14+ — lambda deleter (preferred when available):**

```cpp
auto result = std::unique_ptr<addrinfo, void(*)(addrinfo*)>(rawResult, freeaddrinfo);
```

Or with a lambda (cleaner when the cleanup needs extra logic):

```cpp
auto deleter = [](addrinfo* p) { if (p) freeaddrinfo(p); };
std::unique_ptr<addrinfo, decltype(deleter)> result(rawResult, deleter);
```

**Why this matters:** `getaddrinfo` sets `rawResult` even when `rc != 0` in some implementations. Wrapping immediately — before checking `rc` — means `freeaddrinfo` is called on cleanup regardless. Never check `rc` before wrapping.

```cpp
addrinfo* rawResult = nullptr;
const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &rawResult);
std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result(rawResult, &freeaddrinfo);
// NOW check rc — result is already guarded
if (rc != 0) {
    return makeFromGaiError(host, rc);
}
```

Apply this pattern to any C API that hands back a resource requiring a paired free call: curl handles, sqlite statements, FILE* opened with popen, etc.

---

### errno handling

Save errno **immediately** after the failing call — subsequent calls clobber it.

```cpp
const int rc = ::bind(m_fd, addr, addrLen);
if (rc < 0) {
    const int savedErrno = errno;
    logger::error("bind failed: {}", std::strerror(savedErrno));
    throw std::system_error(savedErrno, std::generic_category(), "bind");
}
```

Pattern:
1. `const int savedErrno = errno;`
2. Log with `std::strerror(savedErrno)` for human-readable text
3. Throw `std::system_error(savedErrno, std::generic_category(), "<operation>")` — never throw `std::runtime_error` for POSIX failures

### C struct initialization

Zero-initialize with `std::memset` before setting fields. Use `std::memcpy` to copy between C structs. Always use the `std::` namespace versions.

```cpp
sockaddr_in addr{};
std::memset(&addr, 0, sizeof(addr));
addr.sin_family = AF_INET;
addr.sin_port   = htons(port);
addr.sin_addr.s_addr = INADDR_ANY;
```

For `sockaddr_storage` (type-erased storage for IPv4 and IPv6):

```cpp
sockaddr_storage storage{};
std::memset(&storage, 0, sizeof(storage));

sockaddr_in a{};
std::memset(&a, 0, sizeof(a));
a.sin_family = AF_INET;
a.sin_port   = htons(port);
a.sin_addr.s_addr = INADDR_ANY;
std::memcpy(&storage, &a, sizeof(a));
```

Cast to `sockaddr*` only at the call site using `reinterpret_cast`:

```cpp
::bind(m_fd, reinterpret_cast<const sockaddr*>(&storage), sizeof(storage));
```

### Non-blocking sockets

Set `O_NONBLOCK` via `fcntl` after creation:

```cpp
const int flags = ::fcntl(fd, F_GETFL, 0);
if (flags < 0) { /* handle errno */ }
if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { /* handle errno */ }
```

### epoll event loop

Own the epoll fd and a wake-up eventfd as class members. Use `EPOLL_CLOEXEC` and `EFD_NONBLOCK | EFD_CLOEXEC` on creation.

```cpp
m_epollFd  = ::epoll_create1(EPOLL_CLOEXEC);
m_wakeUpFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
```

Add fds to epoll:

```cpp
epoll_event ev{};
ev.events  = EPOLLIN;
ev.data.fd = fd;
::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev);
```

Event loop skeleton:

```cpp
void EventsScheduler::run()
{
    constexpr int MAX_EVENTS      = 32;
    constexpr int EPOLL_TIMEOUT_MS = 1000;

    epoll_event events[MAX_EVENTS];

    while (!m_stop.load()) {
        const int count = ::epoll_wait(m_epollFd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        if (count < 0) {
            if (errno == EINTR) { continue; }
            break;
        }

        for (int i = 0; i < count; ++i) {
            if (events[i].data.fd == m_wakeUpFd) {
                uint64_t buf;
                ::read(m_wakeUpFd, &buf, sizeof(buf));
                continue;
            }

            if (events[i].events & (EPOLLERR | EPOLLHUP)) { continue; }
            if (!(events[i].events & EPOLLIN))              { continue; }

            // dispatch to callback
        }
    }
}
```

Stop by writing to the eventfd — this unblocks `epoll_wait` without waiting for the timeout:

```cpp
void EventsScheduler::stop()
{
    m_stop.store(true);
    const uint64_t signal = 1;
    ::write(m_wakeUpFd, &signal, sizeof(signal));
}
```

Use `std::atomic<bool> m_stop{false}` — no mutex needed for the flag itself.

### Callbacks

Use `std::function` for event callbacks, not raw function pointers:

```cpp
using Callback = std::function<void(ISocketClient& client)>;
```

Store as `Callback m_callback`. Guard invocation with a null check: `if (m_callback) { m_callback(*client); }`.

### Interface abstraction

Wrap all POSIX details behind an interface. Callers never see fds, `sockaddr`, or errno. The interface exposes C++ types only:

```cpp
class ISocketClient
{
public:
    virtual ~ISocketClient() = default;

    virtual void openSyncConnection(uint16_t port) = 0;
    virtual void openAsyncConnection(uint16_t port) = 0;
    virtual SocketDto receiveData() = 0;
    virtual void sendData(UdpPayload payload, PeerMetadata peer) = 0;
    virtual int fileDescriptor() const = 0;
};
```

`fileDescriptor()` is the **only** leakage of raw POSIX types upward, and only when the scheduler layer needs to register the fd with epoll. Keep it on the interface, not on internal helpers.

### No `extern "C"` for standard POSIX

Standard POSIX headers (`<sys/socket.h>`, `<unistd.h>`, etc.) already handle linkage. Never add `extern "C"` blocks around them.

---

## File layout

```
<lib>/
├── include/<lib>/
│   ├── interface_<name>.h     ← pure virtual, no POSIX headers
│   └── <name>.h               ← concrete class declaration only
└── src/
    └── <name>.cpp             ← all POSIX includes and implementations
```

---

## Example: RAII socket wrapper

### `socket_client/include/socket_client/interface_socket_helper.h`

```cpp
#if !defined(SOCKET_CLIENT_INTERFACE_SOCKET_HELPER_H)
#define SOCKET_CLIENT_INTERFACE_SOCKET_HELPER_H

#include "socket_client/socket_dto.h"
#include "socket_client/peer_metadata.h"

namespace socket_client
{

class ISocketHelper
{
public:
    virtual ~ISocketHelper() = default;

    virtual int createBlockingSocket(const PeerMetadata& meta) = 0;
    virtual int createNonBlockingSocket(const PeerMetadata& meta) = 0;
    virtual SocketDto receiveFromSocket() = 0;
    virtual void sendToSocket(const SocketDto& data) = 0;
};

} // namespace socket_client

#endif /* SOCKET_CLIENT_INTERFACE_SOCKET_HELPER_H */
```

### `socket_client/include/socket_client/socket_helper.h`

```cpp
#if !defined(SOCKET_CLIENT_SOCKET_HELPER_H)
#define SOCKET_CLIENT_SOCKET_HELPER_H

#include "socket_client/interface_socket_helper.h"

namespace socket_client
{

class SocketHelper final : public ISocketHelper
{
public:
    explicit SocketHelper();
    ~SocketHelper();

    int createBlockingSocket(const PeerMetadata& meta) override;
    int createNonBlockingSocket(const PeerMetadata& meta) override;
    SocketDto receiveFromSocket() override;
    void sendToSocket(const SocketDto& data) override;

private:
    static constexpr int INVALID_FD = -1;
    static constexpr int VALID_FD   = 0;

    int m_fd;
};

} // namespace socket_client

#endif /* SOCKET_CLIENT_SOCKET_HELPER_H */
```

### `socket_client/src/socket_helper.cpp`

```cpp
#include "socket_client/socket_helper.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <system_error>

namespace socket_client
{

SocketHelper::SocketHelper()
    : m_fd(INVALID_FD)
{
}


SocketHelper::~SocketHelper()
{
    if (m_fd >= VALID_FD) {
        ::close(m_fd);
        m_fd = INVALID_FD;
    }
}


int SocketHelper::createBlockingSocket(const PeerMetadata& meta)
{
    m_fd = ::socket(meta.family, meta.socketType, meta.protocol);
    if (m_fd < VALID_FD) {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "socket");
    }
    return m_fd;
}


int SocketHelper::createNonBlockingSocket(const PeerMetadata& meta)
{
    createBlockingSocket(meta);

    const int flags = ::fcntl(m_fd, F_GETFL, 0);
    if (flags < 0) {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "fcntl F_GETFL");
    }

    if (::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "fcntl F_SETFL");
    }

    return m_fd;
}


SocketDto SocketHelper::receiveFromSocket()
{
    if (m_fd < VALID_FD) {
        throw std::system_error(EBADF, std::generic_category(), "receiveFromSocket: fd not open");
    }

    sockaddr_storage peerAddr{};
    socklen_t peerLen = sizeof(peerAddr);
    uint8_t buffer[65535];

    const auto bytesReceived = ::recvfrom(
        m_fd, buffer, sizeof(buffer), 0,
        reinterpret_cast<sockaddr*>(&peerAddr), &peerLen
    );

    if (bytesReceived < 0) {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "recvfrom");
    }

    return SocketDto{buffer, static_cast<std::size_t>(bytesReceived), peerAddr};
}


void SocketHelper::sendToSocket(const SocketDto& data)
{
    if (m_fd < VALID_FD) {
        throw std::system_error(EBADF, std::generic_category(), "sendToSocket: fd not open");
    }

    const auto bytesSent = ::sendto(
        m_fd,
        data.payload.data(), data.payload.size(),
        0,
        reinterpret_cast<const sockaddr*>(&data.peer.addr),
        sizeof(data.peer.addr)
    );

    if (bytesSent < 0) {
        const int savedErrno = errno;
        throw std::system_error(savedErrno, std::generic_category(), "sendto");
    }
}

} // namespace socket_client
```
