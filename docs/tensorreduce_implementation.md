# TensorReduce implementation walkthrough {#tensorreduce_implementation}

This chapter follows the **0.3.1 implementation** from an xTensor expression to
a formal Wolfram tensor contraction and back. For loading and choosing a
backend, see @ref tensorreduce_backend. The conversion is implemented in
Wolfram Language; Mathematica's built-in `TensorReduce` performs the actual
canonicalization.

The adapter works at xPerm's signed-permutation boundary. It does not pass
xTensor tensor heads or abstract index names directly to `TensorReduce`.
xTensor first describes the expression through its slots, symmetry generators
and label classes; the adapter encodes that description as symbolic arrays.

## Where the backend enters the calculation {#tensorreduce_call_path}

For an expression requiring permutation canonicalization, the call path is:

```text
ToCanonical[expression, Method -> "TensorReduce"]
  DispatchToCanonical / DispatchToCanonicalScoped
    OriginalToCanonical             xTensor analysis and recursive processing
      CanonicalPerm[permutation, degree, group, frees, sets, ...]
        DispatchCanonicalPerm
          TensorReduceCanonicalPerm
            System`TensorReduce     formal scalar contraction
            TensorReduceDecode      signed Images permutation, or zero
    xTensor reconstructs the expression from that permutation
```

`OriginalToCanonical` retains xTensor's original definitions. The wrapper
keeps the selected backend in a dynamic scope, so recursive canonicalization
uses the same backend. It separately preserves xTensor's existing derivative
`Method`, rather than passing a backend name to derivative expansion. Trivial
expressions that xTensor handles before calling `CanonicalPerm` need no call
to the permutation backend.

An explicit `Method -> "TensorReduce"` routes `DispatchCanonicalPerm` directly
to the Wolfram adapter, before either native canonicalizer is selected. The
paclet still loads its native library and group-operation hooks at activation,
but this branch does not call `$llCanonicalPerm` or `$llNiehoffCanonicalPerm`.
It also does not use a native canonicalizer to normalize the returned result.

### Source map

| Source | Function | Responsibility |
| --- | --- | --- |
| `Kernel/Implementation.wl` | `LoadxPermLibraryLink`, `UnloadxPermLibraryLink` | Load the native functions; install or restore the public wrappers and private hooks. The file also reads the backend modules when the package is loaded. |
| `Kernel/BackendMethods.wl` | `DispatchToCanonicalScoped` | Resolve backend and derivative methods; preserve the selected backend through recursive xTensor calls. |
| `Kernel/BackendMethods.wl` | `DispatchCanonicalPerm` | Select the canonicalizer and propagate adapter failure through xTensor. |
| `Kernel/TensorReduce.wl` | `TensorReduceSignedImages` | Convert and validate a permutation with phase +1 or -1. |
| `Kernel/TensorReduce.wl` | `TensorReduceSlotGenerators` | Convert the supplied slot generators to `Arrays` symmetry notation. |
| `Kernel/TensorReduce.wl` | `TensorReduceLabelFactors` | Describe auxiliary tensors for labels that are not directly contracted. |
| `Kernel/TensorReduce.wl` | `TensorReduceCanonicalPerm` | Validate labels, select the direct dummy pool, construct assumptions and contractions, and invoke the built-in reducer. |
| `Kernel/TensorReduce.wl` | `TensorReduceDecode` | Validate the reduced contraction and recover its signed slot-to-label permutation. |

These helpers live in the paclet's private context. Their names are a map for
reading the implementation, not an additional public API.

## Input conversion and signs {#tensorreduce_input_conversion}

Write the input as `epsilon Images[g]`, where `epsilon` is +1 or -1 and
`g[[i]]` is the label at slot `i`. `degree = n` counts the real tensor slots.
`TensorReduceSignedImages` uses xPerm's notation conversion and then requires
`g` to be a bijection of `Range[n]`. The explicit phase check matters because
xPerm's general `PermQ` predicate also accepts expressions outside the
adapter's signed-permutation domain.

`TensorReduceSlotGenerators` accepts `GenSet`, `StrongGenSet`, and the
`Symmetric`/`Antisymmetric` shortcuts. A generator becomes
`{imageList, phase}`, the notation supplied to `Arrays`. Shortcut symmetries
are expanded into adjacent signed transpositions. For a `StrongGenSet`, the
base is validated and the generators are reused; this adapter does not run
Schreier–Sims or use the base to choose its canonical representative.

Thus the two extra sign points used by the native interface do not become
array dimensions. The sign is an explicit scalar phase. The input phase is
held aside until decoding; the reducer's output phase accounts for slot
symmetries and antisymmetric auxiliary factors.

All listed free, dummy and repeated labels must be disjoint integers in
`Range[n]`. Labels omitted from these lists are fixed individually, just like
free labels. A `DummySet` accepts metric symmetry flags +1, 0 and -1:
symmetric endpoint interchange, no endpoint interchange, and antisymmetric
endpoint interchange respectively. These flags describe index-exchange
symmetry, not the numerical signature of a metric.

## Constructing the formal tensor expression {#tensorreduce_construct_expression}

The slot array is the private expression `tensorReduceFactor[1, 1]`, called
`A` below. It has rank `n` and exactly the supplied signed slot generators
`S`. Even when the original expression contains many tensor factors, the
adapter keeps **one slot array**. Its full supplied group already contains
the permitted internal symmetries and exchanges of identical original
factors. The implementation does not infer a new factorization of `S`.

### Direct symmetric dummy contractions

For a symmetric dummy pool, exchanging whole pairs and reversing a pair
both have positive sign. Those are already invariances of ordinary
`TensorContract` pairs. The adapter therefore represents the largest such
pool directly as self-contractions of `A`. A label pair `{a,b}` becomes
the slot pair `{inverse[[a]], inverse[[b]]}`, where:

```wl
inverse = Ordering[g];
```

For a permutation image list, `Ordering[g]` is the inverse map: label to slot.
The largest pool is chosen to remove the most auxiliary endpoints; this is
a representation heuristic, not a proof of optimal reduction time. With
equal sizes the first eligible pool is chosen. If no pool has flag +1, all
labels use auxiliary factors.

Only **one** pool may use these uncolored self-contractions. Inlining two
distinct pools would allow their pairs to exchange and enlarge the label
group. For example, consider the negative slot permutation
`-Cycles[{1,3},{2,4}]` and separate symmetric pools `{{1,2}}` and `{{3,4}}`.
Exchanging the pools is forbidden, so this alone does not force zero.
Treating both pools as uncolored edges would permit that exchange and
incorrectly make the expression equal to its own negative.

### Auxiliary factors for all remaining labels

`TensorReduceLabelFactors` returns records of the form
`{originalLabels, class, internalSymmetry}`. A class identifies one pool;
different pool entries stay distinct even if their metadata names coincide.

| Labels | Factors and internal symmetry | Label operations they permit |
| --- | --- | --- |
| Symmetric dummy pool not selected above | Copies of one rank-two symbol, `Symmetric[{1,2}]` | Whole-pair exchanges and positive endpoint flips. |
| Antisymmetric dummy pool | Copies of one rank-two symbol, `Antisymmetric[{1,2}]` | Whole-pair exchanges and negative endpoint flips. |
| Metric-free dummy pool | Copies of one rank-two symbol, no internal symmetry | Whole-pair exchanges, preserving endpoint orientation. |
| Repeated-label pool | Copies of one rank-one symbol | All positive permutations within that pool. |
| Free or unlisted label | Its own distinct rank-one symbol | No interchange with another label. |

Repeated occurrences of the same auxiliary symbol encode permitted exchanges
without constructing the entire label group `D`. Distinct symbols preserve
the distinction between pools and fixed labels. Even free labels attach to
auxiliary vectors, so the formal expression is a scalar and its fixed-label
identities remain available to the decoder.

The complete records have the form
`{symbol, originalSlotsOrLabels, internalSymmetry, side}`. Side 1 denotes `A`;
side 2 denotes an auxiliary factor, named `tensorReduceFactor[2, class]`.
`factorLabels` lists the labels of auxiliary endpoints in tensor-product
order. Its `j`th entry becomes the edge
`{inverse[[factorLabels[[j]]]], n + j}`. These edges are joined to the
direct self-contractions, then applied to the product of all recorded factors.
If the direct pool has `m` pairs, the product has `2 n - 2 m` endpoints, all
contracted. If it covers every label, only `TensorContract[A, pairs]` remains.

### Assumptions and evaluation

Each distinct symbol is declared to belong to
`Arrays[ConstantArray[tensorReduceDimension, rank], Reals, symmetry]`.
The dimension is symbolic: no numerical dimension is assigned, so this model
does not introduce finite-dimensional identities. `Reals` supplies the domain
for these formal arrays with real signs; it does not assert that the user's
original tensor components are real or assign them numerical values.

The implementation passes these assumptions explicitly to
``System`TensorReduce`` inside a `Block` with `$Assumptions = True`.
Unrelated assumptions from the user's session therefore do not enter the
formal problem. Stable indexed private symbols keep their ordering consistent
between calls and avoid allocating new symbols with `Unique` for every tensor.

## Worked conversion and reconstruction {#tensorreduce_worked_example}

Take four slots, an antisymmetric exchange of slots 1 and 2, a symmetric dummy
pair `{1,3}`, and fixed labels 2 and 4:

```wl
g = {2, 1, 3, 4};
group = xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2}]];
frees = {2, 4};
sets = {xAct`xPerm`DummySet[bundle, {{1, 3}}, 1]};
```

Here `inverse` is also `{2,1,3,4}`. The direct dummy pair joins slots 2 and 3.
Give fixed labels 2 and 4 distinct auxiliary vectors `u` and `v`. Their
endpoints are positions 5 and 6 in `TensorProduct[A,u,v]`, giving:

```wl
TensorContract[TensorProduct[A, u, v], {{2, 3}, {1, 5}, {4, 6}}]
```

The assumptions can be written explicitly for this small formal example:

```wl
assumptions = {
  Element[A, Arrays[{d, d, d, d}, Reals, {{{2, 1, 3, 4}, -1}}]],
  Element[u, Arrays[{d}, Reals]],
  Element[v, Arrays[{d}, Reals]]
};
```

Applying `TensorReduce` to the contraction with
`Assumptions -> assumptions` gives the following on Wolfram 15.0.1:

```wl
-TensorContract[TensorProduct[A, u, v], {{1, 3}, {2, 5}, {4, 6}}]
```

The decoder assigns the direct edge `{1,3}` its original labels `{1,3}`,
reads label 2 from `u` at slot 2, and reads label 4 from `v` at slot 4.
The result is `-Images[{1,2,3,4}]`. The following public call reproduces it
after loading and activating the paclet:

```wl
xAct`xPerm`CanonicalPerm[
  xAct`xPerm`Images[g], 4, group, frees, sets, Method -> "TensorReduce"]
(* -xAct`xPerm`Images[{1, 2, 3, 4}] *)
```

Changing `g` to `{3,1,4,2}` puts the direct contraction on antisymmetric
slots 1 and 2. The result is then zero. Both cases use the same conversion
and built-in reducer; there is no expression-specific zero shortcut.
The displayed nonzero representative is a checked example, not a promise
that every Wolfram version or another backend chooses that same ordering.

## Decoding and failure handling {#tensorreduce_decode_result}

`TensorReduceDecode` accepts a complete scalar `TensorContract`, possibly
multiplied by -1. The main routine handles a zero result before calling it.
The decoder does not assume that the reducer leaves `A` first in the product:
it matches each returned factor to an unused record and reconstructs its
original endpoint identities. Identical auxiliary occurrences are assigned
to their records in order, an allowed permutation within their pool.

It checks that every expected factor occurs exactly once with its recorded
multiplicity and that every tensor endpoint occurs in exactly one contraction
pair. An `A`–auxiliary edge assigns its label directly. An `A`–`A` edge belongs
to the direct symmetric pool; these edges are sorted and assigned the input
pool's pairs in their original order. This arbitrary but deterministic choice
is allowed because that pool permits positive pair exchanges and reversals.
Auxiliary–auxiliary edges, extra or missing endpoints, and an incorrect number
of direct edges are rejected. Finally, the recovered image list must be a
bijection of `Range[n]`.

The result is the recovered `Images` multiplied by the reducer's phase and
the saved input phase. An unevaluated or unsupported built-in result generates
the adapter's `::reduce` message and `$Failed`; malformed input uses `::input`.
Within `ToCanonical`, a tagged failure unwinds to the outer wrapper so xTensor
does not try to reconstruct a tensor from `$Failed`. Aborts propagate, while
the dynamically scoped backend selection is restored.

Two elementary cases bypass reduction: degree zero returns the signed empty
permutation, and an explicitly supplied negative identity generator returns
zero. The latter also avoids a rank-one kernel limitation. The adapter does
not enumerate the slot group to search for negative identities.

## Relation to Butler–Portugal and Niehoff {#tensorreduce_algorithm_relation}

All three routes receive the same signed slot/label problem. Internal
symmetries of `A` realize `S`; the direct edges and auxiliary factors realize
exactly `D`. Moving contraction endpoints by those operations traverses the
same signed double coset. Opposite signs for an equivalent configuration imply
zero. The unchanged xTensor reconstruction can therefore consume a signed
permutation from any of the three routes.

| Backend | Where canonicalization happens | Adapter's role |
| --- | --- | --- |
| xPerm | Native Butler–Portugal permutation search; see @ref butler_portugal. | Prepare the native interface's permutation, sign points and label data. |
| Niehoff | Native accelerated permutation search with propagated symmetries; see @ref niehoff_algorithm. | Prepare the same interface data and select the accelerated export. |
| TensorReduce | Wolfram kernel, on the constructed symbolic tensor contraction. | Encode the group actions, call the reducer, validate and decode its answer. |

The project does not reimplement or inspect Mathematica's internal reduction
algorithm. In particular, this backend should not be described as another
implementation of Niehoff's propagation algorithm. It preserves the same
equivalence problem but may choose a different representative; there is no
native postprocessing to force xPerm's or Niehoff's ordering.

## Why the dummy representation changed {#tensorreduce_memory_design}

Version 0.3.0 assigned the whole label group to a second large symbolic array.
That construction represented the right equivalences, but its redundant
dummy-array symmetries caused severe intermediate allocation inside
`TensorReduce`. Merely splitting arrays into smaller factors did not cure it.
The 0.3.1 change uses ordinary contractions for one symmetric dummy pool,
eliminating that extra representation of symmetries the contraction already
has. It retains the single slot array and the exact supplied slot group.

For the 125-factor example this leaves a rank-250 slot array with 125 direct
contraction pairs. The code does not recognize an odd ring or select Niehoff
to obtain zero. This encoding change fixes that regression without claiming
bounded memory or uniformly fast reduction for arbitrary symmetry patterns.
Several large auxiliary pools can still be expensive. The production adapter
imposes no hidden resource limits; callers can use `TimeConstrained` and
`MemoryConstrained` explicitly.

`tests/backend_methods.wl` checks signed orbit membership against an independent
finite double-coset oracle, nonzero examples, mixed dummy pools, decoding
idempotence, backend selection and failure restoration. It also requires the
nine- and 125-factor examples to finish with zero under both time and memory
limits. See @ref validation for commands and `TENSORREDUCE_MEMORY_FIX.md` for
the measured regression and fix.
