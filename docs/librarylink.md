# Wolfram paclet and LibraryLink integration {#librarylink_integration}

The native library contains **two canonicalizers**. The Wolfram paclet selects
one while preserving the calling convention used by xPerm and xTensor. This
selection is separate from the choice of transport: both backends run through
LibraryLink when the paclet is activated.

Read @ref algorithm_overview for the mathematical problem, @ref niehoff_algorithm
for the accelerated search, and @ref butler_portugal for the inherited algorithm.
The native export reference is @ref librarylink_api.

## Activating the accelerated backend {#librarylink_activation}

The paclet is named `xPermLibraryLink`; its package context is
`` xAct`xPermLibraryLink` ``. In a Wolfram Language session:

~~~~~~~~~~~~~~~{.m}
Needs["xAct`xPermLibraryLink`"];
LoadxPermLibraryLink[Automatic, True]
xPermLibraryLinkLoadedQ[]
~~~~~~~~~~~~~~~

The two final expressions should return `True`. `Automatic` resolves the library
bundled under `LibraryResources/$SystemID` in this paclet. An explicit library
path or name may replace `Automatic`. `xPermLibraryLinkLoadedQ[]` checks that
eight native functions are loaded; it does not identify the selected backend
or prove which route a particular computation took.

`Needs` checks whether `` xAct`xPerm` `` is in `$Packages` and loads it if needed. The
implementation is read only if that dependency was loaded. Loading the package
alone leaves native activation explicit. In particular, **`LoadxPermLibraryLink[]`
defaults to `False`, which selects the original Butler–Portugal implementation**.
Use the explicit `True` argument to obtain the Niehoff acceleration.

| Wolfram selection | Native entry point | Mathematical implementation |
|---|---|---|
| `LoadxPermLibraryLink[Automatic, True]` | LL_niehoff_canonical_perm() | Production Niehoff search with total-symmetry inference, propagation, early zero detection, and pruning |
| `LoadxPermLibraryLink[Automatic, False]` or `LoadxPermLibraryLink[]` | LL_canonical_perm() | Original `canonical_perm_ext()` implementation in `src/xperm.cpp` |
| `UnloadxPermLibraryLink[]` | None of these wrappers | Restores the xPerm definitions present before activation |

After successful activation, ordinary xPerm `CanonicalPerm` calls and xTensor
`ToCanonical` calls that use xPerm's native path reach the selected wrapper.
Conceptually, the route is:

~~~~~~~~~~~~~~~{.text}
xTensor ToCanonical / xPerm CanonicalPerm
  -> xAct`xPerm`Private`MLCanonicalPerm
  -> selected LibraryFunction
       -> LL_niehoff_canonical_perm -> canonicalizeLegacyInput
       or
       -> LL_canonical_perm         -> canonical_perm_ext
~~~~~~~~~~~~~~~

Here **“legacy” in `canonicalizeLegacyInput` describes the compatible input
format, not the old algorithm**. The production accelerated algorithm is reached
through that function. The separately exported staged helpers described below
are not involved in this route.

## Why the old WSTP definitions are replaced {#librarylink_handover}

xPerm historically installs a separate native executable through WSTP (formerly
MathLink). That installation can leave fixed-arity `ExternalCall` definitions on
its private `ML*` symbols. Adding a more general `rest__` LibraryLink rule without
removing those definitions is insufficient: Wolfram's pattern ordering may keep
sending calls to the old executable.

`Kernel/Implementation.wl` therefore saves the `OwnValues`, `DownValues`,
`SubValues`, and `UpValues` of seven private symbols, clears those value tables,
and installs the new wrappers:

- `MLCanonicalPerm`
- `MLSchreierSims`
- `MLOrbit`
- `MLSetStabilizer`
- `MLBaseChange`
- `MLBaseChangeStabilizerChain`
- `MLStabilizerSGS`

The original WSTP link is left connected but idle; users need not call
`Uninstall` themselves. After native functions load successfully, the package
sets `` xAct`xPerm`$xpermQ = True `` so that xTensor can use the native route even if the
old executable was unavailable. `UnloadxPermLibraryLink[]` unloads the native
functions, restores all saved value tables, and restores the previous own values
of `$xpermQ`. Loading the implementation again during development first performs
that restoration.

Both canonicalizer functions and six group utility functions are loaded before
new private definitions are installed. If a native load fails, partial loads are
unloaded and `$Failed` is returned. Switching an already active backend first
unloads it; a failed replacement load therefore leaves the original xPerm
behavior restored. Failure to resolve the bundled library occurs before that
unload step and leaves a previously active backend intact.

`tests/wstp_handover.wl` verifies this with a real connected WSTP executable and
call counters. It checks that the 125-factor example reaches the Niehoff native
function without reaching the old executable, and that unloading restores the
old route. This is stronger evidence than seeing a library-loaded flag alone.
See @ref validation.

## Ownership, validation, and failures {#librarylink_errors}

The C ABI adapters in `src/LLInterface.cpp` use Wolfram's `MTensor` callbacks. They
copy borrowed input tensors into `std::vector<int>` values before calling the
algorithms. No wrapper frees or modifies the incoming tensor. Local vectors own
their buffers and release them when the call returns or unwinds.

Result helpers allocate a fresh integer `MTensor`. Before it is assigned to the
result argument, the helper owns it and frees it on a local data-pointer failure.
After `MArgument_setMTensor`, ownership passes to the Wolfram runtime. Native
search state is local to each call; the lifecycle exports allocate no persistent
native resources.

Wolfram's `mint` may be wider than the inherited core's `int`. Checked conversion
rejects values outside the `int` range instead of truncating them. Adapters also
check argument counts, tensor rank/type, dimensions, generator lengths, and
permutation images. The shared canonicalization validator checks the signed
permutation convention, disjoint label groups, matching set lengths, and metric
signs before either mathematical backend runs. It does **not** prove that a
caller-provided strong generating set is strong; that remains the meaning of
the caller's strong-generator flag.

| C return code | Meaning |
|---|---|
| `LIBRARY_NO_ERROR` | The result tensor is available, including a mathematical zero result |
| `LIBRARY_MEMORY_ERROR` | An allocation failed, or a required result data buffer could not be obtained |
| `LIBRARY_FUNCTION_ERROR` | Input was rejected or a `std::exception` other than `std::bad_alloc` was caught |

Exception text is not transported as a Wolfram message by these C wrappers.
LibraryLink reports the failure at the native-call boundary. The production
Niehoff wrappers encode a vanishing expression as a degree-length vector of
zeros, not as an error code; the Wolfram compatibility wrapper applies xPerm's
`Images` and private `ToSign` conversion to obtain its usual result form.

These choices keep symbolic expression construction in Wolfram, mathematical
state in C++, and ownership at a small audited boundary. See @ref design_choices.

## Canonicalization argument contract {#librarylink_canonical_input}

LL_canonical_perm() and LL_niehoff_canonical_perm() share this exact argument
order. All vectors are rank-one integer tensors; only `degree` and `strongFlag`
are scalar integers. Positions in this table are **one-based Wolfram argument
positions**, whereas `MArgument *arguments` is indexed from zero in C++.

| Position | Value | Interpretation |
|---|---|---|
| 1 | `permutation` | One-based image permutation with two final sign points |
| 2 | `degree` | Full permutation length, including the two sign points |
| 3 | `strongFlag` | `1` if the supplied generators form a strong generating set for the supplied base; `0` to construct one |
| 4 | `base` | Base or tentative base, according to `strongFlag` |
| 5 | `flattenedGenerators` | Generator images concatenated in degree-sized blocks |
| 6 | `freeLabels` | Distinct free-index labels |
| 7 | `dummySetLengths` | Number of labels in each dummy set; each length is positive and even |
| 8 | `dummyLabels` | Concatenated dummy sets, each as consecutive ordered pairs |
| 9 | `metricSymmetries` | One entry per dummy set: `0` no metric, `+1` symmetric, `-1` antisymmetric |
| 10 | `repeatedSetLengths` | Positive number of labels in each repeated-index set |
| 11 | `repeatedLabels` | Concatenated repeated-index sets |

Set lengths count **labels, not pairs**. Labels denote canonical names; they are
not the current slots containing those names. The ordering within a dummy pair
records its variance orientation when there is no metric. With a metric,
reversing a pair is allowed with the corresponding metric sign. Labels need not
be consecutive integers within each class. See @ref representations.

For example, a positive identity on two real slots has images `{1,2,3,4}`. Its
negative has images `{1,2,4,3}`. Real slots and labels must not mix with the final
two sign points. The two canonicalizers solve the same signed double-coset
problem, but their canonical-order conventions are not asserted to return the
same raw representative in every supported input layout; see
@ref butler_portugal and @ref validation.

LL_niehoff_canonical_perm_ext() adds three vector arguments:

| Position | Value | Interpretation |
|---|---|---|
| 12 | `subsetOffsets` | Zero-based boundaries into `subsetSlots` |
| 13 | `subsetSlots` | Flattened lists of real slots in declared total-symmetry subsets |
| 14 | `subsetSigns` | `+1` for a symmetric subset, `-1` for an antisymmetric subset |

For example, offsets `{0,3,5}`, slots `{1,2,3,5,6}`, and signs `{1,-1}` declare
symmetry of slots `{1,2,3}` and antisymmetry of slots `{5,6}`. Offsets start at
zero, end at the flattened slot count, and have one more entry than the signs.
Both an empty offsets vector and `{0}` represent no subsets when the other two
vectors are empty. Core validation requires disjoint subsets with at least two
real slots and sign `+1` or `-1`.

These declarations **augment the slot group** with the asserted symmetries. They
can change the answer if they are not already symmetries of the expression.
Automatic inference in the ordinary 11-argument production path needs no such
assertions. The extended export is available for direct native calls; the
standard paclet activation currently loads the 11-argument export.

## Group result streams {#librarylink_wire_formats}

The inherited xPerm group utilities remain useful to both algorithms. The
LibraryLink layer serializes their results as integer vectors so the native
library does not need WSTP expression construction.

A strong generating set is encoded as:

~~~~~~~~~~~~~~~{.text}
{degree, baseLength, generatorCount, base..., flattenedGenerators...}
~~~~~~~~~~~~~~~

Its length is exactly `3 + baseLength + degree * generatorCount`.
`DecodeStrongGenSet` in `Kernel/Implementation.wl` validates this envelope and
constructs xPerm's `StrongGenSet[base, flattenedGenerators, degree]`.

LL_basechangestabchain() returns a sequence of those streams:

~~~~~~~~~~~~~~~{.text}
{streamCount, streamLength1, ..., streamLengthN,
 stream1..., ..., streamN...}
~~~~~~~~~~~~~~~

Each level has a suffix of the changed base and the corresponding stabilizer's
generators. Length prefixes preserve the varying level sizes without requiring
a ragged tensor. LL_set_stabilizer() fixes a set **setwise**;
LL_stabsgs() fixes the listed points **pointwise**.

## Staged diagnostic exports {#librarylink_stages}

The names “Stage 4”, “Stage 5”, and “Stage 6” refer to development stages of this
repository, **not** to numbered algorithms in Niehoff's paper. They expose small
search components and structured traces for testing. In particular,
LL_niehoff_propagated_symmetry_search() explores an explicitly supplied signed
orbit with a size cap. The production Algorithm 4.2 acceleration uses incremental
classes and certified pruning instead; it does not enumerate that orbit.

The first seven staged arguments are:

| Position | Tensor rank | Contents |
|---|---|---|
| 1 | 2 | One accumulated slot permutation `s` per configuration |
| 2 | 2 | One current slot-to-label permutation `g` per configuration |
| 3 | 1 | One separate sign `+1/-1` per configuration |
| 4 | 2 | Per-configuration metadata rows |
| 5 | 2 | One slot generator per row |
| 6 | 1 | Tentative base |
| 7 | 1 | Selected slots to process |

Configuration degrees agree, metadata has a common row width, and point lists
are one-based. The separate sign convention differs from the production ABI's
two trailing sign points. LL_niehoff_persistent_search() consumes these seven
arguments. LL_niehoff_label_group_search() adds a rank-two label-group matrix:

~~~~~~~~~~~~~~~{.text}
{type, labelCount, label1, ..., labelCount, padding...}
~~~~~~~~~~~~~~~

The type codes are `0` fixed, `1` free, `2` dummy without metric, `3` symmetric
metric, `4` antisymmetric metric, and `5` repeated. See
@ref xperm::niehoff::LabelGroupType. LL_niehoff_propagated_symmetry_search() additionally
accepts a rank-two signed-generator matrix with rows
`{sign, image1, ..., imageN}`, followed by a positive scalar maximum orbit size.

Every result is a rectangular integer matrix padded with zeros. The first
column is a row tag:

| Tag | Contents |
|---|---|
| 0 | Configuration: sign, permutations, metadata, and (Stages 5/6) fixed labels |
| 1 | Search-level summary: one-based level number, selected slot, minimum image, survivor count, and orbit |
| 2 | Base length, current chain level, and base |
| 3 | One strong generator |
| 4 | One-based chain-level number, count, and generator-position list |
| 5 | Stage 6 only: zero flag, merged-configuration count, and cancelled-configuration count |

Stage 4 reserves **five** leading columns before variable payloads. Stage 5/6
reserve **six**. For a tag-0 configuration with permutation degree `n` and metadata
width `q`, those payloads are `s[n]`, `g[n]`, `metadata[q]`, then the fixed-label
list in Stage 5/6; its length is in column 3. For tag 1, Stage 5/6 stores the orbit
length in column 6; Stage 4 uses the padded orbit payload without a separate
length. Tags 2 and 4 carry payload lengths in columns 2 and 3 respectively.
Generator-position entries are internal **zero-based** indices into the strong
generator array; mathematical points remain one-based.

The stage matrices are inspection formats, not substitutes for the production
canonicalization interface. Native callers should use the C++ result structures
when possible, as those retain types and lengths directly.
