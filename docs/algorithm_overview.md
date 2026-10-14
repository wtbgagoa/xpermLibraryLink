# The canonicalization problem and the two backends {#algorithm_overview}

## What is being canonicalized?

Fix the order of tensor factors and number their slots. A configuration `g`
assigns a label to each slot. There are two kinds of allowed transformations:

- **Slot symmetries** form a signed permutation group `S`: intrinsic tensor
  symmetries, and any permitted exchanges of tensor factors supplied by xAct.
- **Label symmetries** form a group `L`: dummy-pair renamings, allowed metric
  reversals, and exchanges within repeated-label classes. In the original
  xPerm dummy/repeated search this group is named `D` in the code.

In ordinary function composition, an equivalent configuration has the form
`ell ∘ g ∘ t`, with `t` in `S` and `ell` in `L`. This is the double-coset
formulation written `L g S` in Niehoff's discussion. The code's multiplication
convention is reversed relative to function composition:

```text
product(p, q)[i] = q[p[i] - 1]
new_g = product(product(t, g), ell)
```

Thus a source comment using `S*g*L` with this `product` denotes the same action.
The order convention must be stated before comparing a paper formula with an
array expression. See @ref representations for concrete examples.

Canonicalization chooses a reproducible representative of this orbit. If the
orbit contains the same real-slot arrangement with both signs, the tensor is
zero. This is a **single-term symmetry** problem: these routines do not perform
multi-term identities such as a full Bianchi reduction, nor do they supply
dimension-dependent identities merely from knowing a manifold's dimension.

## What both backends share

Both reduce a potentially enormous group action to computations with a base
and strong generating set (BSGS), point stabilizers, orbits, and Schreier
representatives. The inherited implementation lives in `src/xperm.cpp`,
documented under @ref permutation_groups. A BSGS describes a group compactly;
it does not imply that the subsequent double-coset search has only one branch.

At a high level the searches freeze a prefix, find the least admissible next
image in their chosen ordering, and retain the alternatives that might still
lead to the final representative. Equal real arrangements with opposite signs
must be detected, including alternatives that a faster search would otherwise
throw away.

## The original Butler–Portugal path

```text
LoadxPermLibraryLink[Automatic, False]
  -> xPerm's private MLCanonicalPerm wrapper
  -> LL_canonical_perm
  -> canonical_perm_ext
       -> prepare/complete the slot SGS
       -> coset_rep: arrange free indices
       -> double_coset_rep: canonicalize dummy/repeated labels
            -> SGSD: construct/update the label group's generators and base
```

@ref canonical_perm_ext is the production entry used by LibraryLink. The older
@ref canonical_perm wrapper is still present but accepts the historical inverse
permutation convention. It should not be substituted for the extended entry
without translating that convention.

The original code retains its base-dependent ordering and free-index stage.
Its output need not be the same integer array as the accelerated backend's
natural-order minimum, even when both describe the same tensor. The precise
original search and work tables are explained in @ref butler_portugal.

## The accelerated Niehoff path

```text
LoadxPermLibraryLink[Automatic, True]
  -> xPerm's private MLCanonicalPerm wrapper
  -> LL_niehoff_canonical_perm
  -> canonicalizeLegacyInput
  -> canonicalizeCore
       -> validate structured label classes
       -> legacyChain: build/rebase the slot chain in natural slot order
       -> inferTotalSymmetrySubsets: certify total signed slot subsets
       -> incrementalSearch: Values, propagation, zero checks, certified pruning
```

The extended native API additionally combines declared total slot symmetries
with the input group before making the chain. These declarations change the
group; they are not merely optimization hints.

The central speedup is that several ways of filling a slot can be represented
by **one configuration plus a recorded symmetry at the contracted partners**.
Niehoff's B.1 can later use that recorded symmetry to reach outside the current
slot orbit. B.3 must check the possible sign conflicts before 4.2 discards the
redundant configurations. @ref niehoff_algorithm follows this sequence.

## Source map and scope of the helpers

| File | Responsibility | Which algorithm uses it? |
| --- | --- | --- |
| `src/xperm.cpp`, `include/xperm.h` | Permutations, SGS/Schreier machinery, original canonicalization | Original backend; group primitives also used by Niehoff |
| `src/NiehoffCanonicalizer.cpp` | Validated vector interfaces, staged utilities, and the incremental production search | Niehoff production plus separately exposed helpers |
| `src/TotalSymmetry.cpp` | Prove which signed transpositions belong to the slot group | Niehoff production preprocessing |
| `src/LLInterface.cpp` | Convert tensors, validate ABI arguments, call a backend, convert results/errors | Both backends |
| `Kernel/Implementation.wl` | Load native functions and replace/restore xPerm's private dispatch | Both backends |

The numbered “Stage 1–7” comments describe API development layers. They are
**not** a sequence of seven calls executed by the production adapter. In
particular, Stage 5 enumerates structured label candidates for a reusable
search utility; production uses @ref niehoff_incremental_labels "IncrementalLabels" instead. Stage 6 closes
an explicitly supplied group with a size cap; production uses compact
@ref niehoff_propagated_symmetries "PropagatedSymmetries" instead. This distinction is central to understanding
the relationship between the code and the paper.

See @ref references for the original group-theoretic literature and Niehoff's
reformulation; the source map describes this repository rather than asserting
that either backend is a verbatim transcription of one printed algorithm.
