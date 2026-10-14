# The inherited Butler-Portugal algorithm {#butler_portugal}

This page explains the original xPerm search in `src/xperm.cpp`, how its state
corresponds to Niehoff's description of Butler-Portugal, and which parts the new
implementation reuses. Start with @ref algorithm_overview and @ref
representations if the distinction between slots and labels is unfamiliar.

The old search is still available through `LL_canonical_perm`, which calls
canonical_perm_ext(). It is a separate implementation from the Niehoff search.
The presence of the common `xperm.cpp` object file in the library does not imply
that the old canonicalizer is executing: both algorithms need its permutation,
Schreier, and strong-generating-set operations. The paclet's algorithm selection
is described in the project README.

## Mathematical problem and multiplication convention

A tensor configuration is encoded by a permutation `g` mapping **slot to label**.
A slot symmetry `s` changes where labels occur; a label symmetry `d` renames dummy
indices or exchanges indistinguishable repeated indices. The old source calls
these groups `S` and `D`. Niehoff uses `L` for the label group, including the same
kinds of label symmetries.

The operation product() means "apply the first permutation, then the second":

```text
product(p, q)[i-1] = q[p[i-1]-1]
product(product(s, g), d)[i-1] = d[g[s[i-1]-1]-1]
```

Thus the old code writes its double coset as `S g D`. In ordinary function
composition, the same action is `d ∘ g ∘ s`; this is the `L g S` ordering used in
the paper's function-composition notation. The side on which a factor is written
must always be read together with the multiplication convention. The internal
F2() helper makes this convention concrete by evaluating a TAB pair `(s,d)`.

Generic xPerm group routines handle arbitrary degree-`n` permutations. Tensor
canonicalization additionally reserves the last two points for sign: keeping
`{n-1,n}` in that order is positive, and exchanging them is negative. These are
not tensor slots. If the symmetry class contains both signs of the same real-slot
configuration, the tensor vanishes. The old API returns an array of `n` zeros in
that case; an all-zero array is a result sentinel, not a group permutation.

## Entry points and their different conventions

| Entry point | Input configuration | Purpose |
|---|---|---|
| canonical_perm() | Index-to-slot; free/dummy lists give initial slots | Historical compatibility adapter for a single dummy set; inverts before and after the extended call |
| canonical_perm_ext() | Slot-to-label; lists give label identifiers | Main old canonicalizer, supporting several dummy and repeated sets |
| coset_rep() | Slot-to-label; `freeps` gives current free slots | First stage: place free labels and retain the slot stabilizer |
| double_coset_rep() | Slot-to-label, with free slots already fixed | Second stage: canonicalize remaining dummy/repeated labels |
| SGSD() | Dummy/repeated label metadata | Construct a base and strong generators for the current label group |

`LL_canonical_perm` uses the extended convention directly. Do not add an extra
inverse merely because the historical canonical_perm() wrapper does so. Also,
canonical_perm() counts dummy **pairs**, while the `dl` argument of
canonical_perm_ext() counts dummy **labels**.

The old algorithm's ordering is part of its historical behavior. It first places
free labels, and its sorting uses the supplied slot base and a derived `bSsort`.
The new canonicalizer fixes all real slots in natural order and minimizes numeric
labels. Therefore two valid results need not be byte-for-byte identical, especially
with a nonstandard base or mixed free/dummy labels. Equality of the symmetry
classes, the zero/nonzero decision, and consistency under allowed group actions
are the relevant mathematical comparisons. See @ref validation.

## Strong generating sets are the shared foundation

A base is an ordered list of points whose pointwise stabilizer is trivial. A
strong generating set (SGS) supplies generators for each stabilizer of a base
prefix. This permits a search to fix one more point without enumerating the
whole group.

The implementation divides that work among these routines:

| Routine | Role in both searches |
|---|---|
| schreier_sims() | Extend a supplied generating set to a base/SGS description |
| one_orbit(), one_orbit_chain() | Find the points reachable from the current base point |
| schreier_vector(), one_schreier_orbit_chain() | Record a predecessor tree and the generator used on each edge |
| trace_schreier() | Compose tree edges to obtain a representative moving the root to a specified point |
| stabilizer() | Filter generators fixing a requested base prefix |
| stab_chain() | Index the successive stabilizers by generator row numbers |
| basechange_chain() | Reorder a valid base/SGS while keeping its group unchanged |
| perm_member() | Sift a proposed element through a base/SGS to test membership |

The caveat on stabilizer() is essential: filtering an arbitrary generating set is
**not** a general algorithm for computing a group stabilizer. For example, a
product of two generators can fix a point even when neither generator fixes it.
The filtering operation works in these searches because the SGS/base-prefix
invariant has already been established. If `SGSQ` is zero,
canonical_perm_ext() computes an SGS; if it is one, the caller promises that the
supplied generators are strong for the supplied base.

The chain representation uses zero-based **row indices** into a flattened `GS`,
whereas points, labels, and base entries remain one-based mathematical values.
A Schreier tree stores a full degree-`n` edge permutation for each point, rather
than merely an index into a mutable generator list. This consumes `n*n` integer
entries for `nu`, but allows transversals to be traced after generator storage
changes. These are inherited representation choices, not new tensor symmetries.

## Stage one: canonicalize free indices

canonical_perm_ext() first converts each free **label** into its current slot by
applying the inverse configuration. It then calls coset_rep():

1. Take the orbit of the current base slot under the remaining slot group.
2. Intersect that orbit with the slots occupied by free indices.
3. Choose the least available free label according to the inherited `sortB`
   priority.
4. Trace a Schreier representative that moves the chosen position into the
   selected slot, and apply it to the configuration.
5. Update the free-slot positions and restrict the group to the relevant
   stabilizer.

The extended driver then removes the fixed free positions from the remaining
base and stabilizes their slots. If there are no dummy or repeated labels, it
returns the first-stage result immediately. Otherwise it calls
double_coset_rep() with the remaining problem.

Free labels have no renaming freedom in `D`; moving them is a slot-group action.
This separation into a free-index stage and a contracted/repeated-index stage
explains why the old canonical-order convention differs from the new one-stage
search even though both solve the tensor-equivalence problem.

## Stage two: canonicalize dummy and repeated labels

The input label group is a direct product of independent sets. Each dummy set is
a list of ordered pairs, and each repeated set is a list of distinct permutation
identifiers representing indistinguishable labels. A repeated set is not encoded
by literally repeating the same integer in a permutation.

SGSD() constructs the group's strong generators from these compact declarations:

| Label symmetry | Generator construction | Sign |
|---|---|---|
| Rename dummy pairs within one set | Exchange adjacent whole pairs | Positive |
| Flip endpoints with symmetric metric, `mQ=+1` | Exchange the two labels of each pair | Positive |
| Flip endpoints with antisymmetric metric, `mQ=-1` | Exchange the two labels and the sign points | Negative |
| No metric, `mQ=0` | Omit within-pair flips; retain whole-pair exchanges | Positive |
| Exchange repeated labels within one set | Adjacent transpositions | Positive |

The no-metric case preserves variance: a pair can be renamed but its endpoints
cannot be freely interchanged. Separate dummy/repeated sets do not mix. Metric
signs belong to label actions, while tensor antisymmetry belongs to slot actions;
both contribute to the resulting sign.

The main search then extends a canonical prefix one base point at a time:

1. Compute the current slot orbit `Deltab` and the label-group orbits `DeltaD`.
2. For every surviving TAB pair `(s,d)`, use F1() to collect all labels that can
   occupy the selected slot. Choose the least label under `bSsort`.
3. Reorder the presentation of `D` so that the chosen label is its first relevant
   base point, then compute its Schreier data.
4. Intersect the attainable slot positions with the positions that can carry the
   selected label. These intermediate positions form `NEXT`.
5. For **every** compatible intermediate position, construct new actions `(s1,d1)`
   using the two Schreier trees and append a node to `ALPHA`.
6. Evaluate the new candidates and run consistency(). Opposite signs of the same
   real-slot configuration mean zero.
7. Stabilize the selected slot. Remove a selected dummy pair or repeated label
   from the remaining description of `D`, then continue.

Moving a pair to the front of the metadata is only a change to the presentation
of `D`; it does not itself apply a label action to the tensor. Removing a fixed
dummy pair from that metadata also does not delete either tensor index. Once one
endpoint is fixed by the label stabilizer, its partner cannot be renamed either,
so both leave the remaining label group together.

## How the old search state maps to Niehoff's paper

Niehoff's Algorithm 3.1 is a presentation of the Butler-Portugal search idea,
not a literal listing of the old C implementation. The following map identifies
the corresponding responsibilities without equating their storage layouts:

| Old implementation | Mathematical responsibility | New approach |
|---|---|---|
| `ALPHA`, `TAB`, and alphastruct | Retain candidates agreeing on the canonical prefix, with accumulated slot/label actions | Configurations retain the current map and slot history; see @ref niehoff_algorithm |
| `KS`, `bS`, `Deltab`, `nu`, `w` | Describe the remaining slot group and its transversals | Reuse SGS/Schreier infrastructure and indexed stabilizer levels |
| SGSD(), `KD`, `bD`, `DeltaD` | Describe the current label group and determine reachable labels | `Values` and label-type metadata give the needed orbit minima directly |
| F1(), `IMAGES`, `p` | Find the least attainable label | Appendix B.1, including reachability supplied by propagated symmetries |
| `NEXT` and TAB extension | Enumerate all intermediate placements yielding that least label | Algorithm 4.2 prunes equivalent choices within a total slot-symmetry subset |
| consistency() | Recognize equal configurations with opposite signs | Retain explicit sign checks and add Appendix B.3-style local zero detection |
| dropdummyset(), droprepeatedset() | Pass from the current label group to its stabilizer | Appendix B.4-style incremental label-state update |

The main bottleneck is not that the old algorithm enumerates every element of
`S` or `D` up front: it uses their strong generating sets. The problem is the
number of compatible **candidate paths** it may retain. In a contraction between
a totally symmetric rank-`k` tensor and a generic tensor, many slot permutations
can be compensated by dummy renamings. They share the same canonical prefix, so
the old scheme keeps them. The number can grow factorially.

Niehoff's acceleration records total symmetry classes and propagates their
consequences through contractions to partner slots. That identifies alternatives
already represented by another candidate and permits pruning before they all
materialize. The new implementation is detailed in @ref niehoff_algorithm;
@ref design_choices explains its explicit partner lookup, signed classes, and
validation rules. This acceleration does not make arbitrary permutation groups
polynomial: symmetry that is not captured by those total-symmetry classes can
still leave a large search.

## Low-level calling contract and limits

The inherited functions accept raw pointers and lengths. They do not validate
permutations, buffer capacities, metric flags, disjointness of label sets, or a
caller's claim that a generating set is strong. Tensor permutations must preserve
the last two sign points as a set; real-label declarations cannot use them. Use
the validated LibraryLink boundary or the owning C++ APIs when possible.

canonical_perm_ext() copies the slot base and generators before working on them,
but its dummy/repeated buffers and length vectors are mutable workspaces. Direct
callers that need to reuse those inputs must copy them first. The LibraryLink
adapter already owns temporary decoded input buffers, validates their structure,
and translates failures into LibraryLink error codes.

Some generic utilities have narrower contracts than their historical comments
might suggest. order_of_group() returns a fixed-width signed integer and does not
check overflow. The unrelated subgroup-backtracking property() implements
centralizer and set-stabilizer predicates; the normalizer and intersection cases
mentioned in older comments are placeholders. These routines should not be
mistaken for parts of the accelerated tensor search.

## Sources and reading order

The implementation's historical attribution names José M. Martín-García and
contributors from Cadabra; those notices remain in the source. Its comments
identify the coset/double-coset construction as the Portugal implementation of
the permutation-group approach based on Butler.

For the comparison used in this documentation, read B. E. Niehoff,
[*Faster tensor canonicalization*, Computer Physics Communications 228 (2018),
123-145](https://doi.org/10.1016/j.cpc.2018.02.014), especially Algorithm 3.1,
Algorithms 4.1/4.2, and Appendix B. The foundational sources are:

- G. Butler, [*Fundamental Algorithms for Permutation Groups*, Lecture Notes in
  Computer Science 559 (1991)](https://link.springer.com/book/10.1007/3-540-54955-2).
- L. R. U. Manssur and R. Portugal, [*Group-theoretic Approach for Symbolic Tensor
  Manipulation: II. Dummy Indices*](https://arxiv.org/abs/math-ph/0107032), the
  authors' preprint describing the tensor problem as a double-coset search. Its
  arXiv record also links the [published journal article](https://doi.org/10.1142/S0129183102004571).

For an implementation-oriented route, read the canonical_perm_ext() contract,
then coset_rep(), SGSD(), double_coset_rep(), and finally the comparison table
above. Follow @ref niehoff_algorithm for the corresponding accelerated path and
@ref validation for the independent checks used to assess it.
