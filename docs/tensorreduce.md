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

## Why two symbolic arrays represent the problem

Let `g[i]` be the label at slot `i`. Introduce two distinct symbolic rank-`n`
arrays, `A` and `B`. Give `A` the signed slot symmetries `S` and give `B` the
signed label symmetries `D`. Contract slot `i` of `A` with slot `g[i]` of `B`:

```text
TensorContract[TensorProduct[A, B], {{1,n+g[1]}, ..., {n,n+g[n]}}]
```

An `S` symmetry moves contraction endpoints on `A`; a `D` symmetry moves
the endpoints on `B`. Thus the allowed changes to this scalar contraction
are exactly the signed equivalences of the original configuration. Neither
array has been assigned numerical components. A common symbolic dimension
avoids introducing a finite-dimensional tensor identity that xPerm did not
request.

The label generators implement the existing contracts:

| Label class | Allowed generators on `B` |
| --- | --- |
| Free or unlisted label | Fixed; no generator moves it. |
| Dummy pool, no metric | Exchange whole ordered pairs, preserving their orientation. |
| Dummy pool, symmetric metric | Exchange whole pairs and reverse a pair with sign +1. |
| Dummy pool, antisymmetric metric | Exchange whole pairs and reverse a pair with sign -1. |
| Repeated-label pool | Exchange its labels with sign +1. |

Each pool has its own generators; distinct bundles or repeated classes are
not merged. Signs on the input configuration and slot generators are kept.
An opposite-sign equivalence makes the contraction zero.

`TensorReduce` returns zero or a canonical signed contraction. The adapter
checks that a nonzero result still pairs every slot of `A` with exactly one
slot of `B`, then reads those pairings as the resulting permutation. Unexpected
or unevaluated output produces a diagnostic and `$Failed`; it never silently
switches to another canonicalizer.

Rank-zero inputs need no contraction. A supplied negative identity generator
also gives zero directly: this elementary relation is checked explicitly
because the kernel's rank-one symbolic-array reduction can miss it.

The implementation is in `Kernel/TensorReduce.wl`. Public option dispatch and
restoration are in `Kernel/BackendMethods.wl`, while native loading and the
seven xPerm private hooks remain in `Kernel/Implementation.wl`.

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

No performance ordering is assumed. Encoding a problem as two large arrays
has different costs from native permutation search. The Wolfram kernel may
also change its canonical ordering or performance between versions.

On the validation machine with Wolfram 15.0.1, the sample notebook's
125-factor antisymmetric ring exceeded a 30-second limit with this
TensorReduce adapter. The native Niehoff route returned zero in about
0.47 seconds. These are individual observations, not benchmark medians or
portable speed guarantees. TensorReduce is an alternative backend for
comparison, and does not inherit Niehoff's acceleration.

The new option is deliberately limited to `ToCanonical`, `CanonicalPerm`,
and the paclet loader. `SchreierSims` already has its own unrelated `Method`
option; orbit and stabilizer operations are not tensor canonicalizers.

See @ref librarylink_integration for activation and ownership,
@ref validation for differential tests, and the official
[TensorReduce reference](https://reference.wolfram.com/language/ref/TensorReduce.html)
and [Arrays reference](https://reference.wolfram.com/language/ref/Arrays.html)
for the symbolic tensor interface used by the adapter.
