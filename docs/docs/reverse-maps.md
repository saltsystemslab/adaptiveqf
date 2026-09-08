# Reverse Maps

This page focuses on the provided reverse-map implementations, the wrapper's
structural interface, and implementing a custom reverse map. A reverse map
stores the original key associated with each minirun ID and rank.

Multiple entries can have the same base fingerprint. The quotient and base
remainder identify a minirun, and the rank identifies one entry within that
minirun. The reverse map maintains the following association:

```text
(minirun ID, rank) -> original key
```

The minirun ID is the quotient and base remainder that the filter stores for an
entry. The rank is not a global identifier; it is the entry's position among
entries with the same minirun ID.

## Adaptation Flow

When a query returns a positive result, the application must first determine
whether it is actually a false positive. If it is, the C++ wrapper derives the
minirun ID from the query hash and uses it with the query rank to recover the
original key:

1. `queryFilter()` returns a positive result with the matching hash and rank.
2. The application confirms that the query key is not in the original set.
3. `adaptOnce()` derives the minirun ID and calls
   `ReverseMap::getFingerprint()` with it and the rank.
4. The reverse map returns the original key, which the wrapper passes to the C
   AQF adaptation function along with the false-positive query key.

The C API does not perform this reverse-map lookup. Applications using the C
API must perform the lookup themselves and pass the recovered original key to
`aqf_adapt()`.

## Provided Implementations

| Implementation | Location | Storage | Persistence | Intended use |
|---|---|---|---|---|
| `LocalMap` | `reverse_maps/localmap/localmap_db.hpp` | In-memory STL containers | No | Demonstrations and tests |
| `SplinterDB` | `reverse_maps/splinterdb/splinter_reverse_map.hpp` | SplinterDB | Yes, when opened without overwrite | Larger or persistent stores |

### LocalMap

`LocalMap` is a header-only implementation backed by
in-memory STL containers. It requires no external storage dependency and is the
reverse map used by the basic example. It is volatile and intended for
demonstrations and tests rather than persistent storage.

### SplinterDB

`SplinterDB` stores encoded fingerprint/rank keys in a SplinterDB database. Its
configuration uses:

- `storage_path` as the database file path.
- `cache_size_bytes` as the SplinterDB cache size.
- `overwrite` to choose whether existing data is removed before initialization.

With `overwrite = true`, initialization removes and recreates the existing
database. With `overwrite = false`, initialization attempts to open the
existing database. The repository's demo uses `overwrite = true` and removes
its database when it exits, so the demo does not demonstrate reopening data
across process restarts. See the [building guide](building.md) for dependencies
and build commands.

## ReverseMapConfig

The wrapper passes a `ReverseMapConfig` to the reverse map during
initialization:

```cpp
struct ReverseMapConfig {
  std::string storage_path;
  uint64_t cache_size_bytes;
  bool overwrite;
};
```

The fields are implementation-specific. `LocalMap` ignores all of them;
persistent implementations may use them to select a database, configure a
cache, and decide whether to remove existing data. A custom implementation
should document its interpretation of each field.

## Implementing Your Own Reverse Map

`AdaptiveQF<ReverseMap>` uses a structural C++ template requirement. There is
no formal base class to inherit from. A compatible type must provide these
methods:

```cpp
struct ReverseMap {
  int init(ReverseMapConfig config, int base_fingerprint_bits);
  int insert(uint64_t fingerprint, uint64_t rank, uint64_t key);
  void bulkInsert(uint64_t *keys, uint64_t *fingerprints,
                  uint64_t *ranks, uint64_t numKeys);
  int getFingerprint(uint64_t fingerprint, int rank, uint64_t *value);
  int close();
};
```

The method names and argument types must match the operations used by the
wrapper. The implementation must preserve the fingerprint/rank identity from
insertion through lookup. `getFingerprint` returns the original key through
`value`; the name reflects the legacy API and does not mean that it returns a
new fingerprint.

### Pseudocode Design

The following is deliberately pseudocode, not a complete implementation. It
shows the required data flow without prescribing a storage engine:

```cpp
class MyReverseMap {
  int init(ReverseMapConfig config, int base_fingerprint_bits) {
    // Open or create storage using config.
    // Save base_fingerprint_bits if the key encoding needs it.
    return storage.open(config.storage_path, config.overwrite);
  }

  int insert(uint64_t fingerprint, uint64_t rank, uint64_t key) {
    auto storage_key = encode(fingerprint, rank);
    return storage.put(storage_key, key) ? 0 : -1;
  }

  void bulkInsert(uint64_t *keys, uint64_t *fingerprints,
                  uint64_t *ranks, uint64_t numKeys) {
    for (uint64_t i = 0; i < numKeys; ++i) {
      insert(fingerprints[i], ranks[i], keys[i]);
    }
  }

  int getFingerprint(uint64_t fingerprint, int rank, uint64_t *value) {
    auto storage_key = encode(fingerprint, rank);
    if (!storage.get(storage_key, value)) return -1;
    return 0;
  }

  int close() {
    return storage.close() ? 0 : -1;
  }
};
```

In a real implementation, decide how to handle duplicate keys, insertion
failure, transactions, reopening, corrupted storage, and concurrent access.
The current `bulkInsert` signature returns `void`, so failures from individual
insertions cannot be reported through that method without changing the
interface.

## Implementation Checklist

1. Choose an encoding for `(fingerprint, rank)` that is stable across process
   restarts.
2. Store the original key as the value for that encoded identity.
3. Use the same encoding in `insert`, `bulkInsert`, and `getFingerprint`.
4. Define the behavior of `storage_path`, `cache_size_bytes`, and `overwrite`.
5. Return `0` for successful operations and a negative value for lookup or
   storage failures where the interface allows a return value.
6. Instantiate the wrapper as `AdaptiveQF<YourReverseMap>` and link the AQF
   library with any storage-library dependencies.
7. Test that every inserted fingerprint/rank pair retrieves the expected key
   after reopening the storage.

The encoding must preserve the full base fingerprint and rank without
collisions across process restarts. It must also use the same widths and byte
order for insertion and lookup.

See the [API Reference](api.md) for the wrapper surface, the
[Getting Started tutorial](getting-started.md) for usage, and the
[Building guide](building.md) for the supplied SplinterDB build path.
