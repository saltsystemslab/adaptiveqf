# How the AdaptiveQF Works

The AdaptiveQF, like the well-known [Bloom filter](https://en.wikipedia.org/wiki/Bloom_filter),
is an approximate membership tester. It answers whether an item may be present
in a set using less space than required to store the complete set.

Like other approximate membership filters, the AdaptiveQF may return false
positives but does not normally return false negatives. If the filter returns
**NO**, the item is not in the set. If the filter returns **YES**, the item may
be in the set, but the filter may be wrong.

The key difference between traditional filters and the AdaptiveQF is that the
AdaptiveQF can update itself to avoid repeating a false positive mistake for a
query. Once the application notifies the AdaptiveQF that it returned a false
positive, the AdaptiveQF can update itself so that if that particular query is
repeated later, the filter returns the correct result next time. This allows
the AdaptiveQF to eliminate the impact of repeated or adversarial false-positive
queries.

The rest of this document explains how the AdaptiveQF (or AQF) works at a high
level. Internally, the AQF is based on the **[Quotient
filter](https://en.wikipedia.org/wiki/Quotient_filter)**. Filters are often used
in front of a database: if the filter returns **NO**, the application can avoid
checking the database; if it returns **YES** but the item is not found when the
database is checked on disk, the filter returned a false positive. We will use
a database as the application context, but the discussion also applies to other
contexts.

## The AQF is a Fingerprint Filter

The AQF belongs to a class of filters known as **fingerprint filters**. The term
'fingerprint' refers to a small hash value that the filter stores for each key.

Fingerprint filters hash each item in the set and store a short fingerprint
derived from the hash. To query the filter, the system hashes the query and
checks whether its fingerprint is present. 

Because fingerprints are shorter than the original hashes (and the original
keys), the filter is smaller than the actual set. On the other hand, this also
means the fingerprints in the filter can collide with fingerprints of items not
in the set, causing a false positive.

### Fingerprint filters 101

Let's look at an example using the vowels as the set we want to store.

#### Filter construction

Suppose the set contains the vowels:

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
{101, 010, 110, 001, 111}
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
are different; only their prefixes match:

```text
hash(e) = 010101...
hash(x) = 010000...
```

The key idea behind adapting in the AdaptiveQF is to take advantage of this.
Two keys might have the same initial prefix. But if you keep extending the
prefix, eventually they will differ. So if we update the fingerprint of `e` to
be a longer prefix, the query for key `x` will no longer be a false positive.

The hashes do not match. Only their three-bit prefixes match, and those are the
only bits currently stored by the filter:

```text
stored fingerprint(e) = 010
query fingerprint(x)  = 010

longer fingerprint(e) = 0101
longer fingerprint(x) = 0100
```

Before adaptation, the filter stores three-bit fingerprints. A query hashes the
item, takes the corresponding prefix of the hash, and asks whether the filter
contains that fingerprint:

```text
Before adapting
----------------
Stored fingerprints:  {101, 010, 110, 001, 111}
Query 'x': Does the filter contain a prefix of hash(x)=01000...?
Filter: YES (False positive)

Adapting to 'x' false positive
------------------------------
Prefix used for lookup = 010
Extend fingerprint 010 using the reverse map
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


## How Fingerprints Are Extended

Since the filter only stores the fingerprint `010` and does not know that it
maps to the key `e`, how does it know to extend `010` to the longer prefix
`0101`? The answer is a **reverse map**: an auxiliary structure that stores the
relationship between a fingerprint and the original key that produced it.

Since the reverse map stores the original keys, it is not small like the filter,
and is typically stored on disk. The reverse map is only accessed whenever the
filter itself is updated, or when a false positive needs to be adapted. This means
queries are still completely in main memory like in a traditional filter.

Let's take a look at the prior example and see what the reverse map would
contain in that case. Recall that we stored fingerprints for the set
`{a,e,i,o,u}` in the filter. 

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

The details of how fingerprints, ranks, and extensions are encoded are covered
in [Quotient Filter Internals](qf.md). Storage choices and custom reverse-map
implementations are covered in [Reverse Maps](reverse-maps.md).
