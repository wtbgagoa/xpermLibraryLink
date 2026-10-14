# Niehoff's algorithm: paper to production code {#niehoff_algorithm}

This page uses the numbering in the supplied published paper, *Faster tensor
canonicalization*, CPC 228 (2018), 123–145. Page numbers differ in the preprint;
algorithm numbers and the conceptual structure are the useful anchors.
See @ref references. The code uses the same central mechanism with additional
pruning distinctions needed by this repository's arbitrary label layouts.

## Correspondence table

| Paper concept or step | Code to read | What the implementation actually does |
| --- | --- | --- |
| Configuration and slot group | @ref xperm::niehoff::LegacyCanonicalPermInput, @ref niehoff_core "canonicalizeCore" | Validate signed input and construct label classes; explicit extended subsets augment the slot group. |
| Ordered slot stabilizer chain | @ref niehoff_chain "legacyChain", @ref xperm::niehoff::currentBasicOrbit | Use xPerm's SGS/Schreier machinery, rebased to natural real-slot order. |
| Symmetric-Subsets, section 4.3 | @ref xperm::niehoff::inferTotalSymmetrySubsets | Infer maximal signed transposition components from actual group membership, including hidden swaps. |
| Algorithm 4.1 initialization | @ref niehoff_search "incrementalSearch", @ref niehoff_configuration "PropagatedConfiguration" | Start with `(g_init, identity)` and zero propagation entries. |
| Values and Label-Codes, section 4.1 | @ref niehoff_incremental_labels "IncrementalLabels" | `value`, `type`, `fixed`, `partner`, and `second` implement the current label orbits. |
| 4.1 line 5: slot orbit | @ref xperm::niehoff::currentBasicOrbit | Compute the orbit under the subgroup fixing earlier real slots. |
| B.1 / 4.1 line 6 | @ref niehoff_search "incrementalSearch", @ref niehoff_least_instance "LeastInstance" | Record `(parent,p,q)` at the global least value; even propagated classes may supply `q` outside the slot orbit. |
| B.2 / 4.1 line 10 | @ref niehoff_update_propagation "PropagatedSymmetries::update" | Process least-value source members in slot order; assign odd source and even partner entries with old-class precedence. |
| B.3 / 4.1 lines 11–13 | @ref niehoff_zero_check "PropagatedSymmetries::zero" | Detect the relevant sign contradictions before any equivalent branch is discarded. |
| 4.2 / 4.1 line 14 | @ref niehoff_search "incrementalSearch" (`visitedSubsets`) | Keep certified distinct choices; compose propagated label swap, ordinary label representative, and slot transversal. |
| B.4 / 4.1 line 16 | @ref niehoff_freeze_label "IncrementalLabels::freeze", @ref niehoff_update_values "IncrementalLabels::updateValues" | Freeze the selected label and its partner, then recompute current class minima. |
| 4.1 lines 17–20 | @ref niehoff_search "incrementalSearch" | Sort/deduplicate by `g`, preserve its matching `s`, and detect opposite signs. |
| Return or zero | @ref xperm::niehoff::LegacyCanonicalPermResult | Return the natural-order representative or the explicit zero result. |

The implementations of B.1 and 4.2 are blocks inside the main loop, not public
functions with the paper's names. The source comments mark those blocks.
The reusable functions `runLabelGroupSearch` and `runPropagatedSymmetrySearch`
are not the implementations of B.1–B.4 invoked here.

## What Values means during a search

For each active label, `value[label]` is the least target in its current ordinary
label-group orbit. A frozen label's value is itself. A repeated class has one
minimum among its unfrozen members. A metric dummy class has one minimum among
unfrozen endpoints. A no-metric dummy class has one minimum for each listed
variance. The partner of a chosen dummy target becomes frozen immediately.

This replaces repeatedly constructing and rebasing an explicit label-group
BSGS. It does not construct the new symmetries induced by slot contractions;
those belong to `PropagatedSymmetries`. Ordinary frozen-label rules therefore
must not be confused with the additional label moves allowed by an even
propagated class.

## B.1: why a candidate contains both p and q

`p` identifies the orbit point reachable by a slot representative from the
current selected slot. Usually `q=p`. If the original slot associated with `p`
has an even propagated entry, another remaining slot with the same entry may
carry a lower value. B.1 records that slot as `q`.

During expansion the code exchanges the labels at `p` and `q`, with the
propagated character, applies the ordinary label representative for the source
label at `q`, and then uses the slot transversal taking the selected slot to
`p`. Array evaluation performs these actions with the conventions in
@ref representations. Both `g` and `s` receive the slot action.

The code caches the lowest-valued slot per even class and parent, avoiding a
fresh full scan for every orbit point. Slot and ordinary-label representatives
are also cached within each level; the caches are discarded when the
stabilizers change.

## B.2: how propagation is stored

An odd signed identifier belongs to a source set of mutually exchangeable
labels in a total slot subset. Its corresponding even identifier is transported
to their dummy partners. For example, positive odd `1` has partner `2`, and
negative odd `-3` has partner `-4`.

The arrays live in original-slot coordinates. Source members all share the
least Value, which identifies one ordinary label orbit; this prevents mixing
different dummy pools, repeated classes, or no-metric variances. New source
sets are processed left-to-right, rather than sorting signed identifiers
numerically. An established propagated class retains precedence on overlaps.
Consumed singleton sources are remembered so they are not rediscovered as
fresh independent groups later.

Sources are installed together before their partners. This preserves an odd
entry where both ends of a metric contraction lie inside the same source set.
The implementation does not explicitly erase an even identifier that has only
one remaining occurrence; such an identifier supplies no exchange with another
occurrence. This differs from the paper's presentation of singleton removal
without changing the allowed moves.

## B.3: zero checks are part of correctness

Before pruning a parent, the code checks:

1. An antisymmetric source contains occurrences from the same repeated class.
2. Both endpoints of a metric pair have the same source symmetry, whose sign
   conflicts with the metric character.
3. At least **two** entries of the same even propagated class lie in the same
   nonzero total slot subset with the opposite sign.

The multiplicity requirement in the third case matters: one endpoint in an
unrelated subset is not a contradiction. The later opposite-sign-row check
cannot replace these checks, because the missing branch may have been pruned.
Negative identity is also checked while preparing the slot group.

## 4.2: the pruning certificate used here

Within one parent/least-value set, the implementation uses the key

```text
(total slot subset at p, propagated class at p, partner is internal)
```

and prunes only when the propagated class is nonzero. Unmarked choices remain.
The internal flag asks whether the source label's partner lies in that same
total slot subset. This keeps two distinctions that a literal subset-only rule
would discard: an internal pair versus an endpoint contracted elsewhere, and
an old even class versus a newly discovered odd class.

For a common odd class, two internal choices are interchangeable as whole
pairs; external choices retain their omitted freedom in the matching even
partner class. For a common even class, the propagated label transposition is
already recorded. B.3 checks character consistency in both cases.

The maintained dummy-pair provenance is one of:

```text
(0,0), (oddK,oddK), (oddK,evenK), (evenK,oddK)
```

with matching signs. Debug builds assert this invariant. The refined key is a
coding choice needed to preserve the current API's natural lexicographic
contract; it is not attributed verbatim to the paper. The two motivating
counterexamples are discussed in @ref design_choices and covered by tests.

## Worked contraction: symmetry reaches a fixed slot

Take a symmetric tensor with slots `1,2,3`, contracted with a generic tensor
with slots `4,5,6`. Use dummy pairs `(1,2),(3,4),(5,6)` and a symmetric metric.
Include positive sign points `7,8`. The initial configuration is

```text
g = [1,3,5,2,6,4,7,8]
```

Only the first three slots have slot symmetry. Initially all six dummy labels
have Value 1. B.2 records odd `1` on the first three original slots and even `2`
on the last three. At the first slot, three equivalent source choices become
one configuration plus that propagation record.

| Selected slot | Ordinary slot orbit | Relevant action |
| --- | --- | --- |
| 1 | `{1,2,3}` | Choose label 1; freeze pair `(1,2)`; retain one source choice. |
| 2 | `{2,3}` | Choose label 3; freeze pair `(3,4)`. |
| 3 | `{3}` | Choose label 5; freeze pair `(5,6)`. |
| 4 | `{4}` | Label 2 is already minimal. |
| 5 | `{5}` | The ordinary orbit contains label 6 only. B.1 uses even class 2 to choose `p=5,q=6`, bringing label 4 from slot 6. |
| 6 | `{6}` | Finish with label 6. |

The result is `[1,3,5,2,4,6,7,8]`. Notice that label 4 is already frozen under
ordinary pair renaming when slot 5 is processed. The propagated symmetry is
what makes the move possible while preserving the earlier prefix. If the
source tensor is antisymmetric, the same partner exchange contributes a minus
sign and the last two images become `[8,7]`.

@ref validation includes a compiled example that checks these results and
prints the surviving-configuration counts. It is a small instance of the
contraction family for which redundant branches otherwise grow factorially.

## Complexity and limits

The incremental label and propagation tables are linear in the signed degree;
updating class minima scans the label tables once per level. Each configuration
still stores two permutations. The group-inference helper additionally stores
unordered-pair orbits, requiring quadratic storage, and chain construction and
membership sifting have their own costs. These costs are not hidden by reporting
only the number of surviving configurations.

The central contraction family retains one configuration per level. This does
not establish a polynomial bound for every group. Whole-pair exchanges without
single-slot transpositions, alternating groups, and many inequivalent
contraction structures can still leave a large search. See @ref design_choices
and @ref validation for what is and is not established by the tests.
