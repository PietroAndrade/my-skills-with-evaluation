# RouteTable synchronization choice

## Primitive: `std::shared_mutex` (C++17) with RAII lock guards

`RouteTable` is a read-mostly structure: many threads call `lookup` concurrently while
writes happen only a handful of times at startup and on reload. `std::shared_mutex`
matches that ratio directly:

- Readers take `std::shared_lock`, so any number of `lookup`/`contains`/`size` calls
  proceed in parallel without serializing on each other.
- Writers take `std::unique_lock`, which excludes all readers and other writers for the
  brief, rare duration of an `upsert`/`remove`/`clear`.

### Why not the alternatives

- **`std::mutex`**: correct but forces every reader to serialize, turning the hot path
  into a single-threaded bottleneck for no benefit given writes are rare.
- **`std::atomic` / lock-free map**: `std::unordered_map` is not atomically replaceable,
  and a hand-rolled lock-free map is far more complexity and risk than this access
  pattern justifies.
- **Copy-on-write via `shared_ptr<const map>`**: a valid read-mostly alternative, but it
  changes the API (readers must hold a snapshot) and readers still pay an atomic refcount
  bump per lookup. `shared_mutex` keeps the interface simple with uncontended reads.
- **`std::recursive_mutex`**: unnecessary; no public method calls another under the lock.

### Notes

- The mutex is `mutable` so `const` read methods can lock it.
- Locking is exclusively RAII (`std::shared_lock` / `std::unique_lock`); there are no
  manual `lock()`/`unlock()` calls, so the lock is released on every path including
  exceptions.
- `lookup` returns the destination through an out-parameter plus a `bool`, copying the
  value while the shared lock is held. Returning a reference or pointer into the map would
  hand out a dangling reference once the lock is dropped.
- Copy and move are deleted because `std::shared_mutex` is neither copyable nor movable
  and the table is intended to be shared by reference.

## Files

- `router/include/router/route_table.h`
- `router/src/route_table.cpp`
