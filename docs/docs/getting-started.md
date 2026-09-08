# Getting Started Tutorial

This tutorial walks you through a complete Adaptive Quotient Filter program:
initialize a filter, insert a set of keys, query it for membership, catch a
false positive, and adapt the filter to correct it.

This tutorial is written as a standalone program. Create a file named
`getting_started.cpp` in the repository root and copy the complete program
from the final section into it. The program follows the LocalMap-backed
example in `examples/adapt_demo.cpp`; the SplinterDB-backed variant is shown
in `examples/adapt_demo_splinter.cpp`.


## Headers and initialization

```cpp
#include "aqf_wrapper.hpp"   // the AdaptiveQF template
#include "localmap_db.hpp"   // an in-memory reverse map
```

`aqf_wrapper.hpp` defines the `AdaptiveQF` template class — the main interface
for this library. The `localmap_db` is a *reverse map* implementation that
resides in main memory.

To initialize the filter:

```cpp
AdaptiveQF<LocalMap> filter;
if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_DEFAULT)) return 1;
```

Each argument controls part of the filter's layout:

| Argument | Value | Meaning |
|----------|-------|---------|
| `numExpectedElements` | `700` | Expected number of elements the filter will hold |
| `rbits`  | `8`   | Bits per slot remainder; larger = lower false positive rate, more space |
| `max_load_factor` | `0.95` | Adaptation stops once 95% of slots are occupied |
| `hash_mode` | `QF_HASH_DEFAULT` | The filter hashes ordinary application keys |

`initializeForCapacity` computes the slot count for you: with 700 expected
elements at a 0.95 load factor it needs ~737 slots, rounded up to `1 << 10` =
1024. If you already know the number of quotient bits, `initializeWithQBits`
lets you set it directly. 

This tutorial uses `QF_HASH_DEFAULT` because the inputs are ordinary
application keys. Use `QF_HASH_NONE` only when the caller has already computed
suitable hash values.


### What is a reverse map?

The second header provides a *reverse map*. The core AQF filter stores keys
lossily as *fingerprints*: each key is compressed to a small bit-string
fingerprint that the filter stores exactly. To adapt, the AQF needs to know the
key corresponding to a fingerprint, which is the job of the reverse map.

> The AQF maps keys to fingerprints, throws away the keys and stores just the
> fingerprints. The reverse map stores the (fingerprint -> key) mapping.

The reverse map is intended to be stored on disk, in the layer that stores the
set the filter is being built for. Because the reverse map stores keys
explicitly, it is essentially as large as the set the filter is being built
for. Since filters are specifically used when the set is too large to fit in
main memory, storing the reverse map in memory is not practical.

> For a high-level explanation of how the AQF works and adapts, refer to
> [How AQF Works](how-it-works.md). For implementation details, see
> [Quotient Filter Internals](qf.md).


`LocalMap` keeps everything in memory (it is based on a simple
`std::unordered_map`) and needs no extra dependencies, so we use it here. For a
disk-backed reverse map, see [SplinterDB](building.md) in the Building guide.
Also see [Reverse maps](reverse-maps.md) if you want to implement your own
reverse map.


## Insert some keys

```cpp
srand(42);
uint64_t keys[700];
for (int i = 0; i < 700; i++) keys[i] = ((uint64_t)rand() << 32) | rand();
filter.bulkInsert(keys, 700);
```

We insert 700 random 64-bit keys. `bulkInsert` does two things: it writes each
key's fingerprint into the filter, and it records `(fingerprint, rank) -> key`
in the reverse map so future adaptations can find the real key behind a
collision.

## Query and adapt

Each iteration queries a random key. If the filter returns NO, the query is a
true negative. If the filter returns YES, we check the backing set to determine
whether the item is actually present. If it is present, the result is a true
positive and we skip it. If it is absent, the filter returned a false positive,
which we can pass to `adapt`.

`adapt` corrects the false positive by looking up the real key that shares the
query's fingerprint (via the reverse map) and extending that key's stored
fingerprint until it no longer matches. The wrapper loops until the query is
fully corrected and returns `0` on success or a negative value on failure. The
re-query confirms `key_present = 0` for this query. The end-to-end tests also
check that inserted keys remain queryable after adaptation. Finally,
`filter.close()` frees the filter and the reverse map.


```cpp
for (int i = 0; i < 100000; i++) {
  uint64_t query = ((uint64_t)rand() << 32) | rand();
  QFilterQueryResult result;
  if (filter.queryFilter(query, &result) < 0) {
    fprintf(stderr, "Failed to query filter\n");
    break;
  }

  if (!result.key_present) {
    continue; // true negative — filter correctly says "not present"
  }

  // The filter returned "present"; check whether the item is in the backing set.
  if (std::find(keys, keys + 700, query) != keys + 700) {
    continue; // true positive
  }

  // False positive — adapt to correct it.
  printf("False positive at query %d: key=%lx  rank=%d\n",
         i, query, result.minirun_rank);

  if (filter.adapt(query, &result) < 0) {
    fprintf(stderr, "Failed to adapt false positive\n");
    break;
  }

  printf("  Adapted, key no longer a false positive\n");
  if (filter.queryFilter(query, &result) < 0) {
    fprintf(stderr, "Failed to re-query filter\n");
    break;
  }
  printf("  Re-query: present=%d\n", result.key_present);
  break;
}
```

See the [API Reference](api.md) for the full `QFilterQueryResult` documentation.

## Compile and run

From the repository root, make sure `libaqf.a` is built first:

```bash
make          # build the AQF library
```

Then compile the standalone tutorial program:

```bash
g++ -std=c++17 -O3 -Iinclude -Ireverse_maps/localmap \
    getting_started.cpp libaqf.a -lpthread -lm -o getting_started
```

Run it:

```bash
./getting_started
```

The exact false-positive query and key depend on the platform's `rand()`
implementation. The program should report an inserted-key summary, identify a
false positive, adapt it successfully, and report `present=0` after the
re-query.

The tutorial commands target the repository's GNU/Linux-oriented toolchain and
Makefiles.

## Full program

Here's everything together (identical to `examples/adapt_demo.cpp`):

```cpp
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include "aqf_wrapper.hpp"
#include "localmap_db.hpp"

int main() {
  AdaptiveQF<LocalMap> filter;
  if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_DEFAULT)) return 1;

  srand(42);
  uint64_t keys[700];
  for (int i = 0; i < 700; i++) keys[i] = ((uint64_t)rand() << 32) | rand();
  if (filter.bulkInsert(keys, 700) < 0) {
    fprintf(stderr, "Failed to insert keys\n");
    filter.close();
    return 1;
  }
  printf("Inserted 700 keys\n");

  for (int i = 0; i < 100000; i++) {
    uint64_t query = ((uint64_t)rand() << 32) | rand();
    QFilterQueryResult result;
    if (filter.queryFilter(query, &result) < 0) {
      fprintf(stderr, "Failed to query filter\n");
      filter.close();
      return 1;
    }

    if (!result.key_present) {
      continue; // true negative — filter correctly says "not present"
    }

    // The filter returned "present"; check whether the item is in the backing set.
    if (std::find(keys, keys + 700, query) != keys + 700) {
      continue; // true positive
    }

    // False positive — adapt to correct it.
    printf("False positive at query %d: key=%lx  rank=%d\n",
           i, query, result.minirun_rank);

    if (filter.adapt(query, &result) < 0) {
      fprintf(stderr, "Failed to adapt false positive\n");
      filter.close();
      return 1;
    }

    printf("  Adapted, key no longer a false positive\n");
    if (filter.queryFilter(query, &result) < 0) {
      fprintf(stderr, "Failed to re-query filter\n");
      filter.close();
      return 1;
    }
    printf("  Re-query: present=%d\n", result.key_present);
    break;
  }

  filter.close();
  return 0;
}
```

## Using a Disk-Based Reverse Map

The tutorial uses `LocalMap` because it is simple and requires no external
dependencies. `LocalMap` stores the reverse map in process memory, so it is
primarily useful for demonstrations, development, and small tests.

For a larger dataset, the original keys used during adaptation would generally
be stored in a disk-backed reverse map rather than in an in-memory
`std::unordered_map`. This allows the filter to remain compact while the
reverse map uses the storage layer associated with the application.

The repository provides a SplinterDB-backed implementation. The main changes
are:

- Include `splinter_reverse_map.hpp` instead of `localmap_db.hpp`.
- Instantiate `AdaptiveQF<SplinterDB>` instead of `AdaptiveQF<LocalMap>`.
- Configure a `ReverseMapConfig` with a storage path, cache size, and overwrite
  behavior.
- Build and link the optional SplinterDB dependencies.

For example:

```cpp
ReverseMapConfig reverse_map;
reverse_map.storage_path = "./splinter_demo_db";
reverse_map.cache_size_bytes = 64ULL * 1024 * 1024;
reverse_map.overwrite = true;

AdaptiveQF<SplinterDB> filter;
if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_DEFAULT,
                                 reverse_map)) {
  return 1;
}
```

From the repository root, build the optional dependency and the SplinterDB
support:

```bash
sudo apt install libaio-dev libxxhash-dev
git submodule update --init external/splinterdb
make -C external/splinterdb
make
make -C examples adapt_demo_splinter
./examples/adapt_demo_splinter
```

The package names above are for Debian-based GNU/Linux systems. The supplied
build configuration currently targets GNU/Linux.

The example uses `overwrite = true` and removes its database when it exits.
For a database that should survive process restarts, use an appropriate
persistent path, set `overwrite = false` when reopening it, and manage the
database lifecycle separately.

To use a different storage system, implement the reverse-map operations
required by `AdaptiveQF<ReverseMap>`. See [Reverse Maps](reverse-maps.md) for
the required interface and implementation guidance.
