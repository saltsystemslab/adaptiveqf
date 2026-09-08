# Quotient Filter Internals

> **Scope:** This is a simplified explanation of the quotient filter and the
> AQF extensions. It is not a comprehensive guide. The papers and the source
> code remain the authoritative sources for the exact algorithms and encoding.

If you have just read [How the AQF works](how-it-works.md), you have seen the
main ideas behind fingerprints, adaptivity, and extending fingerprints. This
page explains how the AQF extends the quotient filter to store extendible
fingerprints.

## Quotienting

A quotient filter is basically an array of slots. Initially, all of the slots
are empty:

```text
Slots:  0      1      2      3      4      5      6      7
       [   ]  [   ]  [   ]  [   ]  [   ]  [   ]  [   ]  [   ]
```

Now, let's see how a fingerprint is inserted. The AQF hashes the key and
produces the fingerprint `101110`. The quotient filter divides this fingerprint
into two logical parts:

```text
hash(key) = 101 | 110
            ---   ---
            quotient  remainder (stored)
```

The quotient `101` identifies slot 5, because `101` is the binary
representation of the number 5. The filter inserts the remainder `110` into
that home slot:

```text
Slots:  0      1      2      3      4      5      6      7
       [   ]  [   ]  [   ]  [   ]  [   ]  [110]  [   ]  [   ]
                                             ^
                                       home slot 5
```

The quotient filter does not need to store the quotient alongside the
remainder. The quotient tells it where the remainder belongs. This trick is
called **quotienting**, and it saves space.

### Choosing `q` and `r`

The number of quotient bits, `q`, determines how many home slots the filter has.
A filter with `q` quotient bits has `2^q` slots. As a rule of thumb, for a
target number of slots, `n`, choose:

```text
q = ceil(log2(n))
```

The number of remainder bits, `r`, is the **slot size**. Each slot has room for
one `r`-bit remainder. In the examples on this page, the filter has eight slots,
so `q = 3`, and each remainder has `r = 3` bits:

```text
fingerprint = quotient (q bits) | remainder (r bits)
              101                  110
```

The choice of `r` controls the initial false-positive rate. A larger remainder
is less likely to collide with the remainder of another key, but requires more
space per slot.

> **False-positive rate**
>
> For an `r`-bit remainder, the basic false-positive rate (assuming n=2^q) is:
>
> ```text
> FPR = 2^-r
> ```
>
> For the full relationship between slot size and false-positive rate in the AQF,
> see the [AQF paper](PAPER_LINK_TODO).

## Metadata bits and Handling Home-Slot Collisions

Two fingerprints may have the same quotient and therefore the same home slot.
A **run** is a consecutive group of remainders that belong to the same home
slot. To identify runs, the quotient filter uses two metadata bits:

- The `occupied` bit marks a home slot that has at least one stored remainder.
- The `runend` bit marks the final slot in a run.

The fingerprint from the previous example has quotient `101`, so its home slot
is 5. Its remainder `110` is stored in slot 5:

```text
101110 -> quotient 101, remainder 110
```

Now suppose another fingerprint, `101001`, needs to be inserted. It has the
same quotient, but a different remainder:

```text
101001 -> quotient 101, remainder 001
```

Slot 5 is already occupied, so the filter stores the new remainder in the next
available slot. The two remainders now form the run for quotient `101`:

```text
Slot:       0      1      2      3      4      5      6      7
occupied:   0      0      0      0      0      1      0      0
runend:     0      0      0      0      0      0      1      0
Remainder: [   ]  [   ]  [   ]  [   ]  [   ]  [110]  [001]  [   ]
                                               <-------->
                                                 Run for home-slot 5
```

The `occupied` bit at slot 5 records that the home slot for quotient `101` has
entries. The `runend` bit at slot 6 records that the run ends there. The second
remainder does not need to store its quotient again: the metadata associates
both remainders with the home slot at position 5.

When looking up a fingerprint with quotient `101`, the filter starts at slot 5
and scans the run through slot 6. It checks each stored remainder until it finds
a match or reaches the `runend` bit.

### Metadata During Lookup

The metadata lets the filter find a run without storing a quotient beside every
remainder. In simplified terms, it uses **rank** to count how many occupied
home slots appear before a quotient. It then uses **select** to locate the
corresponding run end. The filter can scan that run and compare the stored
remainders with the query.

The exact rank/select operations and metadata layout are described in the
[RSQF paper](RSQF_PAPER_LINK_TODO).

## Extending a Fingerprint

Now, let's look at how the AQF can extend a stored remainder. Suppose the
fingerprint corresponding to the remainder `110` needs to be extended.

The original fingerprint was:

```text
101110 -> quotient 101, remainder 110
```

Because the slot size is `r = 3`, the AQF extends fingerprints in chunks of
three bits. Suppose the next three bits from the hash are `010`. The extension
does not replace the original remainder. It stores the additional bits in the
next slot:

```text
Before: 101 | 110
After:  101 | 110 | 010
        ---   ---   ---
        q     r     extension chunk
```

The logical remainder is now `110|010`, but it occupies two slots: slot 5
stores the original remainder and slot 6 stores its extension bits. In this
example, slot 6 already contains the remainder `001` for another entry, so
that entry moves to slot 7. Finally, the AdaptiveQF adds another 
the `extension` metadata bit to identify slots containing extension slots:

```text
Before extension
Slot:       0      1      2      3      4      5      6      7
occupied:   0      0      0      0      0      1      0      0
runend:     0      0      0      0      0      0      1      0
extension:  0      0      0      0      0      0      0      0
Remainder: [   ]  [   ]  [   ]  [   ]  [   ]  [110]  [001]  [   ]
```

```text
After extension
Slot:       0      1      2      3      4      5      6      7
occupied:   0      0      0      0      0      1      0      0
runend:     0      0      0      0      0      0      0      1
extension:  0      0      0      0      0      0      1      0
Remainder: [   ]  [   ]  [   ]  [   ]  [   ]  [110]  [010]  [001]
```

The `extension` bit at slot 6 records that the slot contains extension bits for
the remainder in the preceding slot. The entry remains part of the same run
because its quotient is still `101`. The `runend` bit moves to slot 7 because
the other remainder was shifted from slot 6 to slot 7.

This diagram uses a simplified encoding. In the implementation, separate
metadata bit vectors are combined to distinguish normal remainders, extension
slots, run ends, and other internal states. The idea is the same: extension
bits occupy the next slot and are marked so lookups know how to interpret them.

A query that previously matched the three-bit remainder `110` must now match
the extended remainder `110|010`. A different key whose hash continues with a
different three-bit chunk will no longer match this entry.
