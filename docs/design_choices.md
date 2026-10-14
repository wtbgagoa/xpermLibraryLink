# Why the implementation is structured this way {#design_choices}

The two backends solve the same signed equivalence problem, but use different
search representations and comparison orders. This page explains choices in
the current implementation, including places where a direct transcription of
the paper would not meet the existing API contract. Read @ref representations
before changing these structures.

## Retain the original backend and reuse its group machinery

The inherited xPerm implementation remains callable through
@ref canonical_perm_ext. It supplies the original Butler–Portugal behavior and
the Schreier–Sims, orbit and transversal routines also needed by Niehoff's
search. Reusing these operations avoids a second implementation of the same
group-theoretic foundation. It does not make the new search a wrapper around
the old double-coset search: its production loop is
@ref niehoff_search "incrementalSearch", rather than @ref double_coset_rep.

The original routines use flat integer arrays, caller-owned buffers and some
mutable metadata. The new C++ facade owns permutations and intermediate data
in vectors and translates at that boundary. The C++ header is the natural
entry point for native clients; the LibraryLink facade translates Wolfram
integer tensors without changing the underlying algorithms. See
@ref butler_portugal and @ref librarylink_integration.

## Canonical order is an explicit contract

The new API minimizes the images of real slots in natural numeric order. Its
base is rebuilt or reordered to support that search, even if a caller supplies
a different valid SGS base. The old backend uses a free-index stage followed
by a dummy/repeated-index stage and its base-related ordering. Consequently,
two equivalent nonzero results need not have identical permutation arrays.

Tests compare the new result with an independent enumeration under the new
comparison rule. Direct array equality with the old backend is not a general
correctness oracle. The zero status must agree, and nonzero signed
representatives must belong to the same signed double coset. Their standalone
signs can differ when their real-slot representatives differ. See
@ref algorithm_overview.

## Infer total symmetry from membership, not generator appearance

A totally symmetric subset can exist even if no supplied generator is a
single transposition. Conversely, swapping entire pairs does not imply that
their individual slots are interchangeable. Inspecting generator cycles alone
would therefore miss valid acceleration and could infer invalid acceleration.

@ref xperm::niehoff::inferTotalSymmetrySubsets sifts signed transpositions
through the slot stabilizer chain. Conjugation preserves transposition
membership and its character, so one test per orbit on unordered pairs is
enough. Connected proven transpositions determine total subsets. This avoids
enumerating the full group, while still requiring quadratic pair-orbit
storage and the cost of group membership tests. Negative identity is handled
as a zero certificate before using a signed subset classification.

The extended API's declared subsets **add** group generators. They are
mathematical assumptions, not hints that the code checks against the original
group. Both legacy and extended inputs then enter the same inference and
search path. Supplying a false declared symmetry changes the problem.

## Explicit partner and variance tables

The public input can list dummy pairs in any numeric order, including
nonconsecutive or descending labels, and can contain several independent
dummy and repeated pools. Numeric parity cannot reliably identify a partner
or a no-metric variance. @ref niehoff_incremental_labels "IncrementalLabels"
therefore stores those relations explicitly. Free and otherwise unlisted
labels remain fixed. The last two permutation points carry the sign and
cannot enter a real-label pool.

Values are recalculated after a chosen label and, for a dummy, its partner are
frozen. This scans the label tables rather than constructing a new explicit
label-group BSGS at each slot. It deliberately does not claim constant-time
updates of an entire pool. See @ref niehoff_algorithm for B.4.

## Preserve provenance when moving slots

A production configuration stores both `g` (current slot to label) and `s`
(current slot to original slot). Propagation entries are shared in original
slot coordinates. A slot action must act on both arrays; a label action acts
only on `g`. Without `s`, a later access to the propagation table can silently
refer to a different slot after an earlier transversal.

Duplicate canonical rows are compared by `g`, while the retained row keeps
its own matching `s`. A debug invariant checks dummy-pair class relationships.
The two arrays cost linear storage per surviving configuration; the central
acceleration reduces the number of configurations rather than eliminating
that storage. See @ref niehoff_configuration "PropagatedConfiguration".

## Conservative pruning preserves two necessary distinctions

The production key refines a total-subset identifier with the propagated
class and whether the selected dummy partner is internal to that subset.
This is an implementation-specific refinement of Algorithm 4.2; it should
not be described as the paper's exact pseudocode.

**Internal versus external contraction.** With source slots `{1,2,3}` and
`g=[1,3,4,2,5,6]`, pairs `(1,2),(3,4)` put one complete pair inside the source
and leave another endpoint contracted outside it. A subset-only certificate
would treat both kinds of least-label choice alike. They can have different
least completions, so the key distinguishes them. The source character and
metric character are matched in the positive and negative regression cases.

**Old versus new propagated class.** Interleaved total subsets, repeated
labels and arbitrarily paired dummy labels can cause an existing even class
to meet a newly discovered odd class at a later level. Belonging to the same
total subset alone is insufficient to merge those choices. The explicit
class identifier prevents that loss. The permanent
`interleavedPropagationRegression()` and `overlappingPropagatedClasses()`
tests in `tests/acceleration_tests.cpp` cover these cases against independent
double-coset enumeration.

Unmarked choices are retained. B.3 zero checks run before pruning so that
discarding a branch does not discard the only evidence of an opposite sign.
This can retain more states than a more ambitious equivalence analysis, but
does not speculate about symmetries absent from its certificate.

## Cache only while the stabilizers are unchanged

The loop caches slot transversals and ordinary label representatives within
one level. Even-class minima are computed once per parent configuration,
rather than scanning the same class for every slot-orbit point. These caches
are discarded when moving to the next stabilizer. Reusing them across levels
would require keys that encode the changed fixed-label and slot subgroups.

## Keep experimental helpers separate from the production path

The Stage 1–6 interfaces expose useful independently testable pieces and are
retained for compatibility. Stage 6 explicitly enumerates a bounded signed
group. Its limit throws when exceeded; it is not an approximation and does
not implement the production B.2 propagation table. Naming similarity is not
evidence that the production backend invokes it. See @ref staged_helpers and
@ref algorithm_overview.

## Validation and remaining costs

The typed facade rejects malformed permutations, label pools and signs before
calling low-level operations. A caller asserting an already strong generating
set still has to supply a genuine SGS; structural validation does not prove
that assertion. The inherited raw-pointer API has additional caller-side
preconditions documented in @ref permutation_groups.

Independent small-group oracles, sign regressions and work-count bounds test
the pruning decisions. They do not prove all-rank correctness or polynomial
complexity. BSGS construction, subset inference and genuinely inequivalent
configurations can dominate other families. @ref validation describes the
checks and how to reproduce them.
