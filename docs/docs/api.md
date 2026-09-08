# API Reference

## C API (`aqf.h`)

```c
#include "aqf.h"
```

### Lifecycle

```c
bool aqf_malloc(QF *qf, uint64_t nslots, uint64_t key_bits,
                uint64_t value_bits, enum qf_hashmode hash, uint32_t seed);
void aqf_free(QF *qf);
```

### Core operations

```c
int aqf_insert(QF *qf, uint64_t key, uint64_t count,
               qf_insert_result *result, uint8_t flags);
```

Inserts `key` (or increments its count). `result` returns the minirun ID and
rank used by the reverse map. Returns `0` on success and a negative value on
error.

```c
int aqf_bulk_insert(QF *qf, uint64_t *keys, uint64_t numKeys, uint8_t flags);
```

Inserts an array of keys using the configured hash mode. This low-level helper
does not perform reverse-map insertion; applications using the C API must keep
the filter and reverse map synchronized themselves.

```c
int aqf_query(const QF *qf, uint64_t key, uint64_t *ret_hash,
              uint8_t *ret_minirun_rank, uint8_t flags);
```

Returns the count if found, or `0` if not found. A positive result may still be
a false positive. `ret_hash` and `ret_minirun_rank` identify the matching
fingerprint and rank for callers that need to adapt the filter.

```c
int aqf_adapt(QF *qf, uint64_t orig_key, uint64_t fp_key,
              uint64_t minirun_rank, uint8_t flags);
```

Extends the fingerprint of `orig_key` so `fp_key` is no longer a false positive.
The C layer does not perform the reverse-map lookup. The application must use
the fingerprint and rank from the query result to retrieve `orig_key` from its
reverse map, then pass both keys and the rank to `aqf_adapt`.

Returns `1` when an extension is made, `0` when no extension is made, and a
negative value on failure, such as when the filter has no space.

### Metadata

```c
uint64_t aqf_get_noccupied_slots(const QF *qf);
uint64_t aqf_get_nslots(const QF *qf);
uint64_t aqf_get_total_size_in_bytes(const QF *qf);
void     aqf_dump_metadata(const QF *qf);
void     aqf_dump(const QF *qf);
```

### Flags

| Flag | Value | Meaning |
|------|-------|---------|
| `QF_NO_LOCK` | `0x01` | Skip locking (caller must synchronize) |
| `QF_TRY_ONCE_LOCK` | `0x02` | Return an error if the lock is unavailable immediately |
| `QF_WAIT_FOR_LOCK` | `0x04` | Wait until the lock is available |
| `QF_KEY_IS_HASH` | `0x08` | `key` is already a hash (skip hashing) |

### Hash modes

| Mode | Description |
|------|-------------|
| `QF_HASH_DEFAULT` | MurmurHash64A |
| `QF_HASH_INVERTIBLE` | Invertible hash |
| `QF_HASH_NONE` | Caller input is treated as a precomputed hash; skewed inputs can cause poor performance |

---

## C++ Wrapper (`aqf_wrapper.hpp`)

```cpp
#include "aqf_wrapper.hpp"
```

Templated on a reverse map implementation.

```cpp
template <typename ReverseMap>
class AdaptiveQF {
public:
  int init(QFilterConfig cfg);
  int initializeForCapacity(uint64_t numExpectedElements, size_t rbits,
                            double max_load_factor, int hash_mode,
                            ReverseMapConfig reverse_map = {});
  int initializeWithQBits(size_t qbits, size_t rbits,
                          double max_load_factor, int hash_mode,
                          ReverseMapConfig reverse_map = {});
  int bulkInsert(uint64_t *keys, uint64_t numKeys);
  int queryFilter(uint64_t key, QFilterQueryResult *result);
  int adapt(uint64_t key, QFilterQueryResult *result);
  int adaptOnce(uint64_t key, QFilterQueryResult *result);
  int close();

  // Accessors
  double loadFactor();
  uint64_t sizeInBytes();
  void dumpMetadata();
  void dump();
  QF *getQF();
  ReverseMap *getReverseMap();
};
```

`init` takes a fully-specified `QFilterConfig`, where `qbits` is the log2 of
the number of slots. `initializeForCapacity` derives `qbits` from the expected
number of elements and the max load factor; `initializeWithQBits` sets the
number of slots directly to `1 << qbits`. Both construct the filter so callers
do not need to manage the config struct manually.

`adapt` corrects the false positive for `key` and returns `0` on success or a
negative value on failure. `adaptOnce` performs a single extension and exposes
the lower-level operation result. Both take a `QFilterQueryResult` (e.g. from
`queryFilter`) describing the current false positive.

`bulkInsert` inserts each key into the filter and then populates the reverse map.
`queryFilter` returns `0` and writes the query result to `QFilterQueryResult`.
The caller must determine whether a positive result is actually a false
positive; the filter does not know the complete original key set. `adaptOnce`
returns a positive value when it extends a fingerprint, `0` when no extension
is made, and a negative value on failure. `close` releases the filter and
reverse-map resources.

Unlike the C API, the C++ wrapper performs the reverse-map lookup inside
`adaptOnce` before calling the underlying C adaptation function.

For a negative query, callers should use `key_present` and should not rely on
`count` being initialized by the current implementation.

### `QFilterConfig`

```cpp
struct QFilterConfig {
  size_t qbits;            // log2 of number of slots
  size_t rbits;            // remainder bits
  double max_load_factor;  // adapt returns -1 when exceeded
  int hash_mode;           // QF_HASH_DEFAULT, _INVERTIBLE, or _NONE
  ReverseMapConfig reverse_map;
};
```

### `ReverseMapConfig`

```cpp
struct ReverseMapConfig {
  std::string storage_path;       // file path for persistent stores
  uint64_t cache_size_bytes;      // memory budget for caching
  bool overwrite;                 // delete existing data on init
};
```

The current wrapper does not propagate every reverse-map failure. In
particular, `init` does not check the return value of `ReverseMap::init`, and
`ReverseMap::bulkInsert` returns `void`, so its individual insertion failures
cannot be reported through `AdaptiveQF::bulkInsert`.

---

## Reverse Maps

The `ReverseMap` template parameter must implement:

```cpp
struct ReverseMap {
  int  init(ReverseMapConfig config, int base_fingerprint_bits);
  int  insert(uint64_t fingerprint, uint64_t rank, uint64_t key);
  void bulkInsert(uint64_t *keys, uint64_t *fingerprints,
                  uint64_t *ranks, uint64_t numKeys);
  int  getFingerprint(uint64_t fingerprint, int rank, uint64_t *value);
  int  close();
};
```

| Implementation | File | Storage | Persistence |
|---|---|---|---|
| `LocalMap` | `reverse_maps/localmap/localmap_db.hpp` | `std::unordered_map` | None (volatile) |
| `SplinterDB` | `reverse_maps/splinterdb/splinter_reverse_map.hpp` | SplinterDB | Disk-backed |
