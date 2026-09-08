# How the AdaptiveQF works

The AdaptiveQF is an approximate membership tester. It answers whether an item
may be present in a set using less space than required to store the complete
set. 

Other examples of [approximate membership
testers](https://en.wikipedia.org/wiki/Approximate_membership_query_filter) are
Bloom filters, Cuckoo filters and XOR filters.

Like other approximate membership testers, the AdaptiveQF may return false
positives but does not return false negatives. If the filter returns
**NO**, the item is not in the set. If the filter returns **YES**, the item may
be in the set, but the filter may be wrong.

The AdaptiveQF differentiates itself from other membership testers by being able
to update itself to avoid repeating a false positive mistake. Once the
application notifies the AdaptiveQF that it returned a false positive, the
AdaptiveQF can update itself so that the filter returns the correct result if
that particular query is repeated later.  

The rest of this document explains how the AdaptiveQF (or AQF) works at a high
level. Internally, the AQF is based on the **[Quotient
filter](https://en.wikipedia.org/wiki/Quotient_filter)**. 


## The AQF is a fingerprint filter

The AQF belongs to a class of filters known as **fingerprint filters**. The term
'fingerprint' refers to a small hash value that the filter stores for each key.

Fingerprint filters hash each item in the set and store a short fingerprint
derived from the hash. To query the filter, the system hashes the query and
checks whether its fingerprint is present. Storing short fingerprints allows the
filter to use less space than storing the actual keys. The tradeoff for saving
space are false positives from fingerprint collisions with keys not in the set.

### Fingerprint filters example

Let's look at an example where the universe keys are lower case english alphabets, and the set we want to store are english vowels.

```text
{ a, e, i, o, u }
```

For illustration, imagine that the keys produce these hashes:

```text
hash(a) = 101101...
hash(e) = 010101...
hash(i) = 110011...
hash(o) = 001110...
hash(u) = 111000...
```

If the filter stores the first three bits as fingerprints, it stores:

```text
a -> 101
e -> 010
i -> 110
o -> 001
u -> 111
```

Note that the filter only stores the fingerprints and not the keys themselves. The filter
is essentially the set of fingerprints:

```text
Set: {a, e, i, o, u}              -> 8 bits per key
Filter: {101, 010, 110, 001, 111} -> 3 bits per key
```

#### Queries and False Positives

To query the filter, the system hashes the query item and derives the same short
fingerprint. It then checks whether that fingerprint is represented in the
filter.

For the vowel set, querying `e` produces a true positive:

```text
Application
------
Query: e
Ask Filter: Is 'e' in the database?

Filter 
------
hash(e) = 010101...
Fingerprint of Query(e): 010
Lookup: Is 010 in the Filter?
Stored Fingerprints: {101, 010, 110, 001, 111}
Result: YES 

Application
------
Filter says YES, search database.
Key 'e' found in database! (Filter true positive)
```

If the fingerprint is absent, the filter returns NO. Under the filter's normal
one-sided-error behavior, the item is definitely not in the set. This is a
**true negative**.

```text
Application
------
Query: d
Ask Filter: Is 'd' in the database?

Filter 
------
Fingerprint of Query(d): 100
hash(d) = 100101...
Lookup: Is 100 in the Filter?
Fingerprints: {101, 010, 110, 001, 111}
Result: NO (True negative)

Application
------
Filter says NO, skip database lookup.
```


Now consider an absent item whose hash begins with the same bits:

```text
Application
------
Query: x
Ask Filter: Is 'x' in the database?

Filter 
------
hash(x) = 010000...
Fingerprint of Query(x): 010
Fingerprints: {101, 010, 110, 001, 111}
Lookup: Is 010 in the Filter?
Result: YES 

Application
------
Filter says YES, search database.
Key 'x' not found in the database (filter false positive)
```

The reason the false positive occurs is that the filter sees the same three-bit
fingerprint for `e` and `x`:

```text
stored fingerprint(e) = 010
query fingerprint(x)  = 010
```

A traditional fingerprint filter does not change when it reports a false
positive. If the same query is repeated, the filter will return the same false positive response.  

```text
first query for x  -> YES (False positive)
second query for x -> YES (False positive)
third query for x -> YES (False positive)
...
```


## How the AQF Adapts

The false positive occurs because of a fingerprint collision: both `e` and `x`
have the same first three bits in their hashes. However, their complete hashes
are different.

```text
hash(e) = 010101...
hash(x) = 010000...
```

If we took a longer prefix, we could differentiate these keys. This is the key
behind adapting in the AdaptiveQF.  If we update the fingerprint of `e` to be a
longer prefix, the query for key `x` will no longer be a false positive. Upon
notifying the AQF, that it returned a false positive, it will attempt to extend
the fingerprint of key `e`.

```text
stored fingerprint(e) = 010
query fingerprint(x)  = 010

longer fingerprint(e) = 0101
longer fingerprint(x) = 0100
```

> Only the key causing the fingerprint collision is updated. Other fingerprints
> are still 3 bits.

### Reverse map

Since the filter only stores the fingerprint `010` and does not know that it
maps to the key `e`, how does it know to extend `010` to the longer prefix
`0101`? The answer is a **reverse map**: an auxiliary structure that stores the
relationship between a fingerprint and the original key that produced it.

Since the reverse map stores the original keys, it is not small like the filter,
and is typically stored on disk. The reverse map is only accessed whenever the
filter itself is updated, or when a false positive needs to be adapted. This means

```text
Filter (Main memory):
------------------
Fingerprints: {101, 010, 110, 001, 111}

Reverse map (Disk):
------------------
101 -> a
010 -> e
110 -> i
001 -> o
111 -> u
```

When the filter is notified that the query for key `x` is a false positive, it
knows that the colliding fingerprint was `010`. The AQF uses this fingerprint
to retrieve `e` from the reverse map. The fingerprint for `e` is the one that
needs to be extended.

The AQF then extends `e`'s stored fingerprint from `010` to `0101`. The query
for `x` uses the prefix `0100`, so it no longer matches the stored fingerprint.
The query for `e` still uses `0101` and continues to return YES.


### Queries in the AQF

The query path is updated accordingly. To check if an item is in the set, the
filter checks if any of the existing fingerprints are prefixes of the query
hash.

The full process is illustrated below:


```text
Before adapting
----------------
Stored fingerprints:  {101, 010, 110, 001, 111}
Query 'x': Does the filter contain a prefix of hash(x)=01000...?
Filter: YES (False positive)

Adapting to 'x' false positive
------------------------------
Prefix used for lookup = 010
Using reverse map to lookup key corresponding to '010': 'e'
Extend fingerprint of 'e' to 0101 
Updated fingerprints:  {101, 0101, 110, 001, 111}

After Adapting
--------------
Stored fingerprints:  {101, 0101, 110, 001, 111}
Query for key 'x': Does the filter contain a prefix of hash(x)=0100...?
Filter: NO (True negative)

```

Note that the query for `e` still succeeds after adapting:


```text
Stored fingerprints:  {101, 0101, 110, 001, 111}
Query for key 'e': Does the filter contain a prefix of hash(e)=0101...?
Filter: YES
```


## Next steps

The details of how fingerprints, ranks, and extensions are encoded are covered
in [Quotient Filter Internals](qf.md). Storage choices and custom reverse-map
implementations are covered in [Reverse Maps](reverse-maps.md).

> TODO: Add links to papers.
