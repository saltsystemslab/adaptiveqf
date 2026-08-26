# TODO / Roadmap

## Counting

- Counting is not implemented. A value (a count) is meant to be stored
  alongside each fingerprint, completely unrelated to query multiplicity.
- `QFilterQueryResult::count` currently always carries `1` when a key is
  present; once counting lands it should hold the per-fingerprint value.
- Add multi-count insert support and tests for it.

## Concurrency / multithreading

- Multithreaded mode is not tested. The concurrency contract needs to be
  considered carefully before adding parallel use.
- Any concurrency design must pin down what `adapt` is allowed to assume: it
  takes a `QFilterQueryResult` and is currently only valid if no inserts or
  deletes happen between the query and the adapt. That assumption may change.

## `adapt` semantics

- Define and document the guarantees of `adapt`. Should it re-query and then
  adapt, or act purely on a caller-supplied result? What happens if the filter
  changes between the query and the adapt?