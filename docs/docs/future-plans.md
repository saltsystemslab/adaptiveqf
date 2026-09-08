# Future Plans

This page records work that remains to be tested, completed, or clarified. It
is not a promise of a particular release schedule.

## Testing

- Test concurrent queries and updates, then define the supported concurrency
  contract.
- Add thorough insert and delete tests. The underlying implementation has
  insert and delete paths, but they need coverage through the supported APIs.
- Test adaptation when inserts or deletes occur between the query and the
  adaptation request.

## Features

- Add file-backed AQF backup and restore, including serialization and
  deserialization.
- Finish counting support and define how counts relate to fingerprints and
  query results.
- Clarify and document adaptation guarantees and failure behavior.
- Make `AdaptiveQF::init()` fail when reverse-map initialization fails, and
  clean up any AQF state that was already allocated.
- Make reverse-map bulk-insert failures observable and define whether the
  filter and reverse map are rolled back when they disagree.

## Documentation And Release

- Document packaging and release workflows.
- Add citations and links to the relevant AQF and quotient-filter papers.
- Define and test the concurrency behavior of reverse-map implementations.
