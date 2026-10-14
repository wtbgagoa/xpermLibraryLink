# Representations and conventions {#representations}

## One-based mathematics, zero-based storage

@ref xperm::niehoff::Permutation is a `std::vector<int>` of images. Mathematical
point `i` has image `p[i - 1]`; point values range from 1 to the degree. Vector
indices and returned row indices are zero-based unless their individual API
contract says otherwise. Do not subtract one from a stored image until using
it as a C++ array index.

The inherited raw API flattens `m` degree-`n` generators into `m*n` integers,
row by row. A @ref StabilizerChain contains generator **row indices**, not point
numbers. The typed @ref xperm::niehoff::StabilizerChainState records the same
information as `generatorPositions` for each base level.

## Composition direction

The raw and vector `product` implementations compute “first `p`, then `q`”:

```text
p = [2,3,1], q = [2,1,3]
product(p,q) = [q[1],q[2],q[0]] = [1,3,2]
```

This is `q ∘ p` in ordinary function composition. If `g` maps slots to labels,
a slot transversal `t` changes the configuration by `product(t,g)`, while a
label renaming `ell` changes it by `product(g,ell)`. The combined action is
`product(product(t,g),ell)`.

The current original entry @ref canonical_perm_ext and the production
@ref xperm::niehoff::LegacyCanonicalPermInput both use slot-to-label images.
The historical @ref canonical_perm entry uses the inverse index-to-slot
convention. Consult its contract before calling it directly.

## The sign is encoded by two extra points

If there are `r` real tensor slots, production permutations have degree `r+2`.
The final two points form their own invariant pair:

| Meaning | Last two images |
| --- | --- |
| Positive sign | `[r+1, r+2]` |
| Negative sign | `[r+2, r+1]` |

They are not free tensor slots and cannot be mixed with real labels. For two
real slots, the antisymmetric exchange is `[2,1,4,3]`. Negative identity on the
real slots is `[1,2,4,3]`: its presence in the slot group already forces zero.

The typed result @ref xperm::niehoff::LegacyCanonicalPermResult uses `zero=true`
and an empty permutation for zero. The LibraryLink boundary represents zero as
an all-zero length-`degree` tensor, matching the established native layout.
A signed nonzero result remains a proper permutation of `1..degree`.

Some reusable stages use @ref xperm::niehoff::SignedPermutation or
@ref xperm::niehoff::SearchConfiguration::sign as a **separate** sign. Stage 5's
@ref xperm::niehoff::LabelCandidate::representative and `sign` must be composed
together. Production @ref niehoff_label_representative "IncrementalLabels::representative" encodes the sign
in the final two points instead. These are different interfaces, not two
independent signs to multiply on every production result.

## Label classes and dummy pairs

@ref xperm::niehoff::makeLegacyLabelGroups validates and translates the legacy
arrays. Their lengths count **labels**, not pairs. Each dummy class has an even
length; adjacent entries in its supplied list define a pair. Class boundaries
come from `dummySetLengths`, and `metricSymmetries` contains one character per
class. Repeated classes use their own length and label arrays.

| Type | Allowed exchange | Representation detail |
| --- | --- | --- |
| Fixed/free | No renaming | Unlisted real labels are fixed too |
| Repeated | Any permutation within that repeated class | Labels distinguish occurrences of the same component |
| Dummy, no metric (`0`) | Rename pairs while preserving their listed orientation | First and second entries define the two variances |
| Symmetric metric (`+1`) | Pair renaming and endpoint reversal | Reversal has positive character |
| Antisymmetric metric (`-1`) | Pair renaming and endpoint reversal | Reversing one pair changes sign |

The list `[4,1,6,3]` describes pairs `(4,1)` and `(6,3)`, not consecutive numeric
pairs. With no metric, label `4` can rename to `6` but cannot independently
rename to `1` or `3`. If the second variance is encountered first, its minimum
must be chosen among the second endpoints, not inferred from the minimum first
endpoint. With an antisymmetric metric, mapping a source pair to a differently
oriented target also requires the correct sign.

This is why production keeps explicit `partner`, `second`, and `type` arrays.
The paper's convenient adjacent-number/parity rules would not implement this
API's general input contract. Freezing a selected dummy target freezes both
of its endpoints, even if its partner has not yet appeared in a frozen slot.

## Three coordinates that must not be confused

The production @ref niehoff_configuration "PropagatedConfiguration" retains:

- `labels` (`g`): **current slot → current label**;
- `slots` (`s`): **current slot → original slot**.

Total slot subsets are indexed by current slot. Propagation entries are indexed
by original slot, so the symmetry at current slot `p` is `P[s[p-1]]` in the
one-based lookup arrays. Label values and dummy partners are indexed by labels,
so the value at `p` is `Values[g[p-1]]`. The inverse of `g` locates a label's
current slot when its partner's position is needed.

A slot transversal must update both `g` and `s`. A label renaming updates `g`
only. When equal `g` rows are deduplicated, the retained `s` must belong to the
same configuration pair; copying a `g` row without its `s` destroys the
coordinate invariant.

## Ordering and bases

The Niehoff production search chooses the numeric lexicographic minimum over
real slots `1..r`, independent of the caller's valid BSGS base. `legacyChain`
rebases/completes the slot chain to that order. The original backend retains
its own free-index and base-relative conventions. A supplied “strong” flag is
an assertion about a valid generating set, not a request for the library to
prove an arbitrary caller-supplied BSGS is complete.

The two sign points come after real slots, so sorted equal real arrangements
with opposite signs are adjacent. That permits the final pairwise cancellation
scan. Early B.3 checks are additionally needed because pruning may remove the
alternative that would have produced the opposite sign later.
