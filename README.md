# Adaptive Quotient Filter (AQF)

The AdaptiveQF is an approximate membership filter that adapts to false
positives.  It is based on the quotient filter. It compactly stores fingerprints
for keys and can extend them to prevent repeated false positives.

The project provides:

- A C API in `include/aqf.h`.
- A C++ `AdaptiveQF<ReverseMap>` wrapper in `include/aqf_wrapper.hpp` that
  supports the provided `LocalMap` and `SplinterDB` implementations, as well
  as support for custom reverse maps.

Note that the filter only reports whether a fingerprint matches; it needs
feedback about whether its result was a true or false positive. Like other
approximate membership filters, it never returns false negatives. 

When a false positive is identified, AQF uses a *reverse map*, configured during
filter initialization, to recover the original key so that it can adapt and
extend the corresponding fingerprint.

## Quick Start

Build the core library and LocalMap-backed example:

```bash
make
make -C examples
./examples/adapt_demo
```

The example inserts keys, performs membership queries, identifies a false
positive using the original key set, and adapts the filter to remove that
false positive.

## Tests

Build and run the LocalMap end-to-end test:

```bash
make -C tests test_e2e
./tests/test_e2e
```

The test exercises insertion, querying, false-positive adaptation, and
false-negative checks. SplinterDB-backed examples and tests require the
optional external dependency described in the [building guide](docs/docs/building.md).

## Documentation

- [Overview](docs/docs/overview.md)
- [Getting Started](docs/docs/getting-started.md)
- [Building and Testing](docs/docs/building.md)
- [How the Quotient Filter Works](docs/docs/qf.md)
- [API Reference](docs/docs/api.md)
- [Reverse Maps](docs/docs/reverse-maps.md)
- [Project Roadmap](docs/docs/TODO.md)

## Status and Roadmap

### Work in Progress

- Counting behavior is not finalized in the modern AQF wrapper.
- The C++ wrapper's concurrency contract is not finalized.
- File-backed AQF serialization and deserialization are not currently exposed
  through `aqf.h`.
- Packaging and release workflows are not currently documented.

## License

See [LICENSE](LICENSE).
