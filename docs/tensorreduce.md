# Backend selection and TensorReduce {#tensorreduce_backend}

Version 0.3.0 adds a third canonicalization backend in the Wolfram kernel.
The two C++ algorithms remain unchanged. The new adapter translates the same
signed double-coset problem into symbolic arrays, calls ``System`TensorReduce``,
and translates its answer back into xPerm's signed permutation notation.
It does not run a native canonicalizer to obtain or normalize that answer.

## Select a backend

After installing the updated paclet, start a fresh kernel:

```wl
Needs["xAct`xPermLibraryLink`"];
LoadxPermLibraryLink[Method -> "Niehoff"]
```

The method names are `"xPerm"`, `"Niehoff"`, and `"TensorReduce"`. Loading
selects the session default. `LoadxPermLibraryLink[]` retains the original
`"xPerm"` default; the older `[Automatic, True]` and `[Automatic, False]`
forms still select Niehoff and xPerm respectively. Native group utilities
remain available even when the default canonicalizer is TensorReduce.

Select a backend for an individual operation without unloading the library:

```wl
ToCanonical[expr, Method -> "TensorReduce"]
ToCanonical[expr, Method -> "Niehoff"]
ToCanonical[expr, Method -> "xPerm"]

xAct`xPerm`CanonicalPerm[
  xAct`xPerm`Cycles[{1, 2}], 2,
  xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2}]],
  {1, 2}, {}, Method -> "TensorReduce"]
(* -xAct`xPerm`Images[{1, 2}] *)
```

These options are installed when `LoadxPermLibraryLink` succeeds. Loading
the paclet with `Needs` alone does not activate them. Unloading restores the
previous xAct definitions, options and native-dispatch state.

`Method -> Automatic` uses the current default. `SetOptions` can also set a
default on either public canonicalization function. An explicit backend
selection takes priority over the legacy `MathLink` transport flag; for
example, `Method -> "TensorReduce", MathLink -> False` still invokes
TensorReduce. With automatic selection, `MathLink -> False` retains xPerm's
original Wolfram-language implementation.

### Preserve xTensor's existing derivative method

xTensor already uses `Method` for metric-incompatible covariant derivatives.
Its existing values, including `Implode` and
`{ChangeCovD, "ExpandChristoffel" -> False}`, keep that meaning. They use the
current canonicalization backend. To select both explicitly, use:

```wl
ToCanonical[expr,
  Method -> {"TensorReduce", "DerivativeMethod" -> Implode}]
```

A plain backend string uses xTensor's saved derivative default. These settings
are scoped to the call, including recursive canonicalization; a one-off method
does not reload the library or change the session default.

## Formal contractions and the signed double coset

Let `g[i]` be the label at slot `i`. One formal rank-`n` array `A` has exactly
xPerm's supplied signed slot group `S`. No factorization of this group is
assumed, so cyclic, coupled and other general signed groups retain their
original meaning.

For the largest symmetric-metric `DummySet`, the adapter puts ordinary
contraction edges directly between the corresponding slots of `A`. If a
label pair is `{a,b}`, the edge is `{g^-1[a],g^-1[b]}`. Reversing an edge or
reordering these edges is already immaterial to `TensorContract`. These are
precisely the positive endpoint flips and whole-pair exchanges of this pool.
Only one pool is represented this way: contracting two distinct pools into
uncolored edges would incorrectly allow their dummy labels to mix.

The remaining labels attach to auxiliary array factors:

| Label class | Auxiliary representation |
| --- | --- |
| Free or unlisted label | A distinct rank-one symbol, fixing that label. |
| Dummy pool, no metric | Identical rank-two symbols with no internal symmetry; endpoints retain their orientation. |
| Dummy pool, symmetric metric | Identical symmetric rank-two symbols, unless this is the one directly contracted pool. |
| Dummy pool, antisymmetric metric | Identical antisymmetric rank-two symbols; endpoint reversal contributes -1. |
| Repeated-label pool | Identical rank-one symbols within that pool. |

Every auxiliary pool has its own symbol. Exchanges of identical factors in
the tensor product supply exactly the allowed pair/label permutations. Thus
internal symmetries of `A` supply `S`, while the direct edges and auxiliary
factors supply exactly `D`. The scalar network represents the signed double
coset without introducing numerical components. A common symbolic dimension
avoids finite-dimensional identities that xPerm did not request.

For the sample antisymmetric ring, all labels belong to one symmetric dummy
pool, so the expression is simply:

```text
TensorContract[A, {{g^-1[1],g^-1[2]}, ..., {g^-1[n-1],g^-1[n]}}]
```

`TensorReduce` returns zero or a signed scalar contraction. The decoder
validates the factor multiset and every edge. Edges from `A` to an auxiliary
factor determine their labels directly. The direct `A`–`A` edges are sorted
and assigned the original dummy pairs; that assignment differs only by the
permitted positive pair exchanges/flips. An unexpected result produces a
diagnostic and `$Failed`, with no switch to another canonicalizer.

Rank-zero inputs need no contraction. A supplied negative identity generator
also gives zero directly: this elementary relation is checked explicitly
because the kernel's rank-one symbolic-array reduction can miss it.

The implementation is in `Kernel/TensorReduce.wl`. Public option dispatch and
restoration are in `Kernel/BackendMethods.wl`, while native loading and the
seven xPerm private hooks remain in `Kernel/Implementation.wl`.

### Why version 0.3.1 changes the representation

Version 0.3.0 instead put the complete label group `D` on a second rank-`n`
array and contracted it with `A`. That encoding is mathematically valid but
computationally unsuitable for this example: with nine factors the actual
xTensor call exhausted a 512 MiB additional-allocation limit in about 0.47 s.
Constructing `Arrays` assumptions was cheap; the growth occurred inside
`TensorReduce`. Splitting the two arrays into smaller factors did not resolve
it. Removing the redundant dummy-array representation did.

Version 0.3.1 uses the direct-edge construction above. This is a general
conversion of symmetric dummy pools, not an odd-ring recognition rule.
No native canonicalizer is called to discover zero or normalize the answer.
The tests require both nine and 125 factors to return zero within 30 seconds
and 512 MiB of additional kernel allocation. A timeout or memory-limit result
fails the test. Nonzero even rings and distinct dummy pools are checked too.

## Canonical form and scope

TensorReduce chooses its own representative. It need not produce the
lexicographic minimum used by Niehoff, or xPerm's base-dependent result.
Compare results using a common canonicalizer when testing equivalence;
do not require identical raw signed permutations from different methods.
Use one backend consistently when collecting equivalent tensor expressions.

The adapter handles the single-term symmetries represented by `CanonicalPerm`.
It does not supply Bianchi identities, dimension-dependent identities,
metric contraction rules beyond the supplied label symmetries, or a new
model of tensor derivatives. xTensor still performs expression analysis and
reconstruction around the chosen permutation backend.

No universal performance ordering is assumed. On the validation machine,
the corrected 125-factor xTensor call completed in about one second on Wolfram
15.0.1. The old 30-second timeout is superseded by this fix. Other symmetry
patterns, particularly several large auxiliary pools, can still be expensive;
this change is not a constant-memory guarantee for arbitrary TensorReduce
problems. The adapter does not impose a hidden production time or memory cap.
For an exploratory run, explicit bounds can be supplied by the caller:

```wl
MemoryConstrained[
  TimeConstrained[ToCanonical[expr, Method -> "TensorReduce"], 30, $Aborted],
  512 * 1024^2, $Aborted]
```

The new option is deliberately limited to `ToCanonical`, `CanonicalPerm`,
and the paclet loader. `SchreierSims` already has its own unrelated `Method`
option; orbit and stabilizer operations are not tensor canonicalizers.

See @ref librarylink_integration for activation and ownership,
@ref validation for differential tests, and the official
[TensorReduce reference](https://reference.wolfram.com/language/ref/TensorReduce.html),
[TensorContract reference](https://reference.wolfram.com/language/ref/TensorContract.html),
and [Arrays reference](https://reference.wolfram.com/language/ref/Arrays.html)
for the symbolic tensor interface used by the adapter.
