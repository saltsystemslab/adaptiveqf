# Overview

## What are filters 

Learning from false positives enables the AQF to provide **robust false positive
rate** guarantees: The AQF guarantees that the observed false positive rate will
never exceed the configured rate. 

In contrast, although traditional filters are also configurable in their false
positive rates, they cannot guarantee that the observed false positive rate will
be within the the configured rate. 

The reason traditional filters cannot provide such guarantees is due to repeated
false positive queries. Each time a prior false positive rate query is repeated,
traditional filters are guaranteed to repeat the mistake. On the other hand,
with the help of some feedback and auxilliary data, Adaptive filters are able to
update themselves to avoid repeating a prior false positive.




Filters are probabilistic data structures that are used to check if an element
is present in a set. 

Filters answer membership queries with one-sided errors: they never return false
negatives, but may return false positives. If the filter returns NO (element not
in set), the filter is always correct. However, if the filter returns YES
(element in set), there is a small chance of it being wrong.

> **TODO**: Describe why filters are useful.

> **TODO:** Describe the problem of repeated false positives. 

> **TODO:** Describe adaptivity — see [The Quotient Filter](qf.md).

> **TODO:** Further reading: point to papers, add citations.

> **TODO:** If you just want to get started, go to getting started.

