<!-- BEGIN author: opencode -->
# Building

## Prerequisites

- GNU Make
- gcc and g++ with C11 / C++17 support
- The default build targets GNU/Linux on x86-64 and enables SSE4.2 flags.
  Use `NH=1` to disable the SSE4.2-specific flags.
- (Optional) SplinterDB and its system dependencies — see
  [Reverse maps](#reverse-maps) below

## Build

Run these commands from the repository root:

```bash
# Build the AQF library
make

# Build and run example
make -C examples && ./examples/adapt_demo
```

Useful root Makefile modes include:

```bash
make D=1    # debug-style build
make NH=1   # disable SSE4.2-specific flags
make P=1    # profiling build
make clean
```

## Reverse maps

The AQF core library (`libaqf.a`) contains the AQF and quotient-filter C
implementations. Reverse-map implementations are supplied separately. To use
the C++ adaptation wrapper, link the core library with a reverse map that
stores the original keys.

We provide a reference persistent reverse map in the form of **SplinterDB**, and
a lightweight in-memory **LocalMap** for development and testing.

### LocalMap (header-only)

LocalMap is a single-header C++ class — no separate compilation needed.
Simply include `reverse_maps/localmap/localmap_db.hpp` in your source and
instantiate `AdaptiveQF<LocalMap>`. No extra link flags required.

### SplinterDB

[SplinterDB](https://github.com/vmware/splinterdb) requires Linux system
dependencies such as `libaio` and `xxhash`. On Debian-based systems, the
development packages are typically named `libaio-dev` and `libxxhash-dev`.
SplinterDB must be built before the optional AQF support library:

```bash
sudo apt install libaio-dev libxxhash-dev
git submodule update --init external/splinterdb
make -C external/splinterdb
# Re-run the root build so libsplinterutil.a is created.
make
make -C examples adapt_demo_splinter
```

The repository's example and test Makefiles provide the include and link flags
needed for the supplied SplinterDB-backed programs. External consumers need
the equivalent SplinterDB include path, library path, `-lsplinterdb`, `-laio`,
`-lxxhash`, and the repository's `libsplinterutil.a`.

## Run the demos

```bash
# In-memory reverse map (no external deps)
./examples/adapt_demo

# SplinterDB-backed reverse map
./examples/adapt_demo_splinter
```

The SplinterDB demo uses `overwrite = true` and removes its database when it
exits. It demonstrates the disk-backed integration but does not demonstrate
reopening a database across process restarts.

## Run the tests

```bash
# LocalMap (no SplinterDB needed)
make -C tests test_e2e && ./tests/test_e2e

# SplinterDB
make -C tests test_e2e_splinter && ./tests/test_e2e_splinter
```

Running `make -C tests` builds both test binaries and therefore requires
SplinterDB. The LocalMap test also supports `--legacy`, `--both`, `--qbits N`,
`--num-queries N`, and `--load-factor F`; the SplinterDB test supports the
numeric options. The `--load-factor` option controls the initial fill amount;
the adaptation threshold is currently configured separately in the test code.

Both tests check that inserted keys remain queryable after adaptation. They
report false-positive counts as workload statistics; identical counts across
reverse-map implementations are not a documented invariant.

<!-- END author: opencode -->
