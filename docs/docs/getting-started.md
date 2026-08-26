# Getting Started Tutorial

This tutorial walks you through a complete Adaptive Quotient Filter program:
initialize a filter, insert a set of keys, query it for membership, catch a
false positive, and adapt the filter to correct it.


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
if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_NONE)) return 1;
```

Each argument controls part of the filter's layout:

| Argument | Value | Meaning |
|----------|-------|---------|
| `numExpectedElements` | `700` | Expected number of elements the filter will hold |
| `rbits`  | `8`   | Bits per slot remainder; larger = lower false positive rate, more space |
| `max_load_factor` | `0.95` | Filter refuses to adapt once 95% of slots are occupied |
| `hash_mode` | `QF_HASH_NONE` | Keys are already hashes — the filter won't hash them again |

`initializeForCapacity` computes the slot count for you: with 700 expected
elements at a 0.95 load factor it needs ~737 slots, rounded up to `1 << 10` =
1024. If you already know the number of quotient bits, `initializeWithQBits`
lets you set it directly. 


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

> For more details on how the AQF works and adapts, refer to [The quotient
> filter](qf.md). 


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

Each iteration queries a random key. The first branch handles **true negatives**
— the filter correctly says "not present". Next we scan the data set (linear
scan, O(n)) to check whether the query key was actually inserted. If it was,
that's a **true positive** and we skip it. The remaining case is a **false
positive**: the filter reports "present" for a key that was never inserted.

`adapt` corrects the false positive by looking up the real key that shares the
query's fingerprint (via the reverse map) and extending that key's stored
fingerprint until it no longer matches. The wrapper loops until the key is
fully corrected and returns the number of extension rounds performed. The
re-query confirms `key_present = 0` — the false positive is gone, and no
inserted key becomes harder to find. Finally `filter.close()` frees the
filter and the reverse map.


```cpp
for (int i = 0; i < 100000; i++) {
  uint64_t query = ((uint64_t)rand() << 32) | rand();
  QFilterQueryResult result;
  filter.queryFilter(query, &result);

  if (!result.key_present) {
    continue; // true negative — filter correctly says "not present"
  }

  // Scan the data set to confirm this is a false positive (O(n)).
  if (std::find(keys, keys + 700, query) != keys + 700) {
    continue; // true positive — filter correctly says "present"
  }

  // False positive — adapt to correct it.
  printf("False positive at query %d: key=%lx  rank=%d\n",
         i, query, result.minirun_rank);

  int n = filter.adapt(query, &result);

  printf("  Adapted, key no longer a false positive\n");
  filter.queryFilter(query, &result);
  printf("  Re-query: present=%d\n", result.key_present);
  break;
}
```

See the [API Reference](api.md) for the full `QFilterQueryResult` documentation.

## Compile and run

Make sure `libaqf.a` is built first:

```bash
make          # build the AQF library
```

Then compile the tutorial program:

```bash
g++ -std=c++17 -O3 -Iinclude -Ireverse_maps/localmap getting_started.cpp \
    libaqf.a -lpthread -lm -o getting_started
```

Run it:

```bash
./getting_started
```

With the fixed seed, you should see exactly:

```
Inserted 700 keys
False positive at query 709: key=699989c2d3708b4  rank=0
  Adapted, key no longer a false positive
  Re-query: present=0
```

The false positive appears at a deterministic query index, gets adapted, and
the re-query confirms it's corrected.

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
  if (filter.initializeForCapacity(700, 8, 0.95, QF_HASH_NONE)) return 1;

  srand(42);
  uint64_t keys[700];
  for (int i = 0; i < 700; i++) keys[i] = ((uint64_t)rand() << 32) | rand();
  filter.bulkInsert(keys, 700);
  printf("Inserted 700 keys\n");

  for (int i = 0; i < 100000; i++) {
    uint64_t query = ((uint64_t)rand() << 32) | rand();
    QFilterQueryResult result;
    filter.queryFilter(query, &result);

    if (!result.key_present) {
      continue; // true negative — filter correctly says "not present"
    }

    // Scan the data set to confirm this is a false positive (O(n)).
    if (std::find(keys, keys + 700, query) != keys + 700) {
      continue; // true positive — filter correctly says "present"
    }

    // False positive — adapt to correct it.
    printf("False positive at query %d: key=%lx  rank=%d\n",
           i, query, result.minirun_rank);

    int n = filter.adapt(query, &result);

    printf("  Adapted, key no longer a false positive\n");
    filter.queryFilter(query, &result);
    printf("  Re-query: present=%d\n", result.key_present);
    break;
  }

  filter.close();
  return 0;
}
```
