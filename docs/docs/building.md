<!-- BEGIN author: opencode -->
# Building

## Prerequisites

- gcc, g++ (C11 / C++17)
- (Optional) SplinterDB — see [Reverse maps](#reverse-maps) below

## Build

```bash
cd adaptiveqf

# Build the AQF library
make

# Build and run example
make -C examples && ./examples/adapt_demo
```

## Reverse maps

The AQF core library (`libaqf.a`) contains only the quotient filter.  To enable
adaptation, you must link `libaqf.a` with a reverse map implementation that
stores the original keys. 

We provide a reference persistent reverse map in the form of **SplinterDB**, and
a lightweight in-memory **LocalMap** for development and testing.

### LocalMap (header-only)

LocalMap is a single-header C++ class — no separate compilation needed.
Simply include `reverse_maps/localmap/localmap_db.hpp` in your source and
instantiate `AdaptiveQF<LocalMap>`. No extra link flags required.

### SplinterDB

[SplinterDB](https://github.com/vmware/splinterdb) requires `libaio-dev` and
`libxxhash-dev`. It must be built first, then linked via `libsplinterutil.a`:

```bash
git submodule update --init external/splinterdb
cd external/splinterdb && make && cd ../..
make -C examples adapt_demo_splinter
```

In your Makefile, add `-Lexternal/splinterdb/build/release/lib -lsplinterdb`
and link `libsplinterutil.a`.

## Run the demos

```bash
# In-memory reverse map (no external deps)
./examples/adapt_demo

# SplinterDB-backed reverse map
./examples/adapt_demo_splinter
```

## Run the tests

```bash
# LocalMap (no SplinterDB needed)
make -C tests test_e2e && ./tests/test_e2e

# SplinterDB
make -C tests test_e2e_splinter && ./tests/test_e2e_splinter
```

Both should report **0 false negatives** and identical false-positive counts.

<!-- END author: opencode -->
