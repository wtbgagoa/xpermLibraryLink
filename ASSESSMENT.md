# Assessment of xpermLibraryLink's Niehoff branch

**Review date:** 10 October 2026

**Revision 2:** Includes the notebook performance correction documented in [PERFORMANCE.md](PERFORMANCE.md).
**Baseline:** [`Niehoff`, commit `058f072ad1a790b5db574af6b946a1cc38a9e97a`](https://github.com/wtbgagoa/xpermLibraryLink/tree/058f072ad1a790b5db574af6b946a1cc38a9e97a)
**Reference:** B. E. Niehoff, *Faster tensor canonicalization*, Computer Physics Communications 228 (2018), 123-145, [doi:10.1016/j.cpc.2018.02.014](https://doi.org/10.1016/j.cpc.2018.02.014). The supplied PDF was read, including the rendered Algorithm 4.1 and the related procedures in Section 4 and Appendix B.

## Overall assessment

**The baseline is an experimental tensor canonicalizer with significant correctness defects; it is not a complete implementation of Niehoff's algorithm.** It has useful foundations: xPerm's permutation-group machinery, explicit structured label classes, a LibraryLink transport layer, and a stabilizer-chain search. However, the characteristic incremental symmetry propagation and branch pruning of the paper are absent from the production canonicalization path.

This review supplies a tested correctness and robustness repair, a standalone test build, and documentation of the implementation's actual contract. It does **not** claim to complete the paper's algorithm or reproduce its speedups. The repaired code passes the independent small-group tests described below, but still exhibits factorial configuration growth for a family that Niehoff specifically accelerates.

The baseline's own commit history calls for verification and labels the new algorithm experimental. That assessment was appropriate. The repair is substantially better supported by tests, but further algorithm work is needed before advertising it as the full Niehoff implementation.

## 1. Comparison with the paper

The main entry points `canonicalizeLegacyInput` and `canonicalizeExtendedInput` call `canonicalizeCore` in `src/NiehoffCanonicalizer.cpp`. That core performs a stabilizer-chain search and repeatedly normalizes whole label configurations. The separately exported Stage 2-6 helpers are largely a parallel implementation path; their presence does not mean the production core uses them.

| Mechanism in the paper | Baseline and repaired implementation |
| --- | --- |
| Slot-group orbits and Schreier transversals | Present, using xPerm routines. |
| Progressive least-label selection and duplicate removal | Present in a different formulation. |
| `Values` and `Label-Codes` arrays, with the progressive updates of B.4 | No equivalent incremental data structures in the production loop. Revision 2 compiles target orders once and normalizes each candidate with a linear scan. |
| B.1 least-value instances `(p,q)`, including choices reached through propagated symmetries | Absent. |
| B.2 propagation of total slot symmetries through dummy contractions | Absent. |
| B.3 early sign-conflict checks before redundant branches are discarded | Absent as a propagation mechanism. The core instead checks opposite signed configurations, and the repair also detects negative identity in the slot group. |
| Algorithm 4.2 `Visited-Subsets` pruning | Absent. The unsafe subset-sorting shortcut has been replaced with ordinary, correct group operations. |
| Compact `Propagated-Symmetries` array | Stage 6 explicitly enumerates a supplied signed group, with a size cap. This is not the paper's propagation algorithm and is not used by the production adapter. |

The paper does not promise polynomial behavior for every symmetry group. Its important improvement is avoiding factorial explosion in common contractions with totally symmetric or antisymmetric subsets. That particular improvement remains missing here.

## 2. Confirmed defects and repairs

The examples below use one-based permutation images. For the production signed API, the last two points encode the sign and are not tensor slots. Function names identify both the baseline defect and the corresponding patched code; source line numbers change with the patch.

### 2.1 Identity generators corrupt the group base — high severity

**Location:** `src/xperm.cpp`, `nonstable_points`.

For an identity generator, `first_nonstable_point` returns zero. The baseline appended that zero to the base anyway. Subsequent group operations could access permutation entry `p[-1]`; repeated identity generators could also overrun the expected base storage. A redundant generating set containing the full signed permutation group on four real slots exposed a non-terminating test case.

**Repair:** append a base point only when the generator actually moves a point. Regression tests cover identity-only groups, repeated identity generators, valid Schreier representatives, and the redundant 48-element signed group. This repairs the underlying group routine rather than only avoiding the problematic canonicalization input.

### 2.2 Negative identity is not detected — high severity

**Location:** `canonicalizeCore`.

For degree 4, input `{1,2,3,4}`, free labels `{1,2}`, and generator `{1,2,4,3}`, the slot group contains negative identity. The tensor equals its own negative and must vanish, but the baseline returns a nonzero identity configuration. The same situation can arise from products of generators, even when negative identity is not explicitly supplied.

**Repair:** check direct generators, then the completed strong generating set, for a sign reversal fixing every real slot. The search still performs its opposite-sign configuration checks for cancellations involving label symmetries.

### 2.3 Canonical results depend on representation order — high severity

**Locations:** `realSearchBase`, `normalizeLabelGroups`, `canonicalizeLabel`.

The baseline selected slots in the caller's base order, while normalizing labels in natural slot order. These are incompatible ordering conventions. It also assigned repeated and dummy labels according to their supplied list order, rather than always choosing the least admissible label.

A simple example is a repeated-label class listed as `{2,1}`: the baseline can turn the identity arrangement into `{2,1,...}`. For no-metric dummy pairs `(1,4),(2,3)`, the arrangement `{4,1,2,3,5,6}` should normalize to `{3,2,1,4,5,6}`. Minimizing the first member of each pair alone is insufficient because the second variance occurs first in this arrangement.

**Repair:** use natural slot order independently of the supplied BSGS base; sort repeated targets numerically; choose dummy targets using the variance of the first encountered endpoint. With a metric, either endpoint is available. Antisymmetric-metric signs account for the orientation of both the source and target pair. Sorted target arrays with advancing cursors avoid per-pair tree-node allocations.

### 2.4 Extended subset sorting is not a valid combined-group search — high severity

**Locations:** the removed `normalizeTotalSubsets` shortcut and `canonicalizeExtendedInput`.

With three real slots, input `{3,2,1,4,5}`, generator `(1 3)`, and declared symmetric subset `{1,2}`, the combined slot group is the full symmetric group on three slots. The baseline returns `{1,3,2,4,5}` instead of identity. Even changing the listing of the same subset from `{1,2}` to `{2,1}` can change the answer.

Sorting an entire subset after each search step can move already-fixed slots and does not correctly account for the interaction with other generators.

**Repair:** treat the subsets as additional declared slot symmetries, add their signed adjacent transpositions to the generating set, and rebuild the group. This makes the meaning explicit and handles combined group actions correctly. Subsets remain disjoint, as required by the existing API. This is a conservative correctness repair, not Niehoff's subset pruning.

### 2.5 Staged searches use stale configurations — high severity for those APIs

**Locations:** `expandConfigurations`, `advanceGroupSearchLevel`, `runPersistentGroupSearch`, `runLabelGroupSearch`, and the explicit Stage 6 utility.

The baseline updates the accumulated slot permutation but not the current slot-to-label configuration. Later levels therefore inspect labels at the wrong positions. For current labels `{3,2,1}`, generator `(1 2)`, all labels free, and selected slots `{1,2,3}`, Stage 5 records minima `{2,2,1}`, which cannot describe the resulting permutation.

**Repair:** document and maintain the `(s,g)` invariant: a slot transversal acts on both the accumulated slot action and the current configuration; a label renaming acts on the current configuration. Stage 6 now uses the same convention for its explicit action. Ordering used for deduplication also includes `fixedLabels`, matching the equality test and preventing missed duplicates.

### 2.6 Antisymmetric pair representatives carry the wrong sign — high severity for Stage 5

**Location:** `pairRepresentative` / `canonicalizeLabel`.

When a reversed source pair maps to a different target pair, the baseline uses two cross-transpositions. That reverses both pairs, whose antisymmetric-metric character is positive, while the returned candidate sign is negative.

For example, with antisymmetric pairs `{1,2,3,4}`, mapping source label 4 to canonical label 1 must use a representative with a single pair reversal. The returned permutation and its separately reported sign must belong to the same signed label group.

**Repair:** exchange the pairs while reversing only the source pair, using a four-cycle when necessary. An independent label-group oracle checks both the minimal target and signed representative membership, including partially fixed labels and unsorted pair descriptions.

### 2.7 Moved-from data breaks LibraryLink Stage 6 — high severity for that export

**Location:** `src/LLInterface.cpp`, `LL_niehoff_propagated_symmetry_search`.

The baseline moves the input permutation rows into configurations and then reads `slots.front().size()` to obtain the degree. Reading a moved-from vector is permitted, but its size no longer represents the original input. In this environment it became zero, rejecting nonempty symmetry generators and potentially under-sizing serialized results.

**Repair:** read the degree from the owning configuration. Validate staged permutation widths, generator degrees, signs, bases, and selected points before output serialization, including when no search levels are requested.

### 2.8 Incomplete native input validation — high severity for malformed inputs

**Locations:** common LibraryLink helpers, group wrappers, `validateLegacyInput`, and staged public entry points.

Previously, invalid permutations, base points, inconsistent set lengths, and sign points mixed into real labels could reach unchecked group routines. Summing signed lengths could overflow, and negative lengths could offset oversized positive lengths before slicing arrays. Empty-base base changes and stabilizing an entire base also had incorrect boundary handling.

**Repair:** validate permutations and dimensions, distinct in-range points, sign-point separation, positive bounded set lengths, metric counts/types, and disjoint label groups before unsafe operations. Accept degree-2 scalars. Handle empty-base and full-base group operations. Free an allocated output tensor if its storage is unavailable. Bound explicit signed-group closure as elements enter the pending set, instead of allowing the queue to grow beyond the intended cap.

These improvements are not a claim that every historical raw-pointer xPerm function now validates arbitrary direct C++ calls. Such routines still have caller preconditions.

### 2.9 Unloading destroys existing xAct definitions — medium severity

**Location:** `xPermLibraryLink.m`.

The baseline clears xAct's wrapped `ML*` symbols on unload, including cleanup after a failed load, instead of restoring their previous definitions.

**Repair:** snapshot and restore the relevant symbol definitions. Successful unload, repeated unload, and failed loading are covered by real-kernel tests. Standard LibraryLink version/initialization exports were also added for explicit ABI negotiation; their prior absence was **not** a demonstrated load blocker, since the original shipped library loaded successfully here.

## 3. Build and maintainability improvements

- Added `BUILD_LIBRARYLINK=OFF` so the core and mock-interface tests can run without proprietary software.
- Added CTest targets and a timeout to catch non-termination regressions.
- Retained the default LibraryLink shared-library build.
- Made native-CPU and LTO flags explicit options. Revision 2 restores an optimized Release default when no single-configuration build type is supplied; explicit Debug still works.
- Made PGO modes mutually exclusive and applied their link options to standalone test programs as well as the shared library.
- Corrected build instructions and the README's implementation claims.
- Changed production candidate collection to discard worse-than-current-minimum rows immediately, reducing unnecessary temporary storage. This does not reduce the number of tied surviving configurations.

## 4. Validation performed

Final tests were run on Linux x86-64 with GCC 16.2.1. Actual integration used Wolfram 15.0.1 and xAct/xPerm 1.2.4.

| Validation | Final result |
| --- | --- |
| Release CTest: independent canonicalizer tests and mock LibraryLink boundary tests | All three passed, including the new odd/even notebook-ring regression. |
| Canonicalizer/helper test executable | **7,817 checks; zero oracle mismatches.** |
| AddressSanitizer + UndefinedBehaviorSanitizer, Debug build | All three CTest targets passed, with no reported address/undefined-behavior errors. Leak detection was disabled because LeakSanitizer cannot operate under this execution environment's tracing restrictions. |
| Real `LibraryFunctionLoad` and wrapper integration | Passed. |
| Real xAct `CanonicalPerm`, explicitly selecting the native path | Symmetric free indices, antisymmetric free indices, and a vanishing contraction passed. |
| Wrapper unload/repeated unload/failed-load restoration | Passed, including equality of saved definitions and a callable restored sentinel. |
| Patch whitespace validation | Passed. |

The independent canonicalization oracle enumerates small signed slot and label groups, computes their double coset, and checks the lexicographic minimum and opposite-sign zero condition. It does not use the implementation's permutation composition, normalization, or group algorithms. Coverage includes every arrangement through four real slots for selected representative groups, both input signs, 300 fixed-seed randomized cases through five slots, arbitrary base and label ordering, supplied strong generating sets, multiple label classes, and extended symmetric/antisymmetric subsets. Separate label-candidate tests enumerate signed label-group actions and fixed-label masks.

This is exhaustive within the enumerated families, not exhaustive over every possible tensor symmetry group or rank. It is not a formal proof. Windows/macOS builds and a broad real-world xTensor workload were not tested. The installed original xAct WSTP executable reported a connection failure; LibraryLink integration nevertheless passed. Restoration of its definitions was verified without claiming its old external executable worked.

Reproduction commands from the patched source directory:

```sh
cmake -S . -B build-core -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
./build-core/canonicalizer_tests

cmake -S . -B build-sanitize -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize --output-on-failure
```

On a machine where LeakSanitizer works, omit `ASAN_OPTIONS=detect_leaks=0` to include leak checking. For the actual SDK build and real-kernel test:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
  -DWOLFRAM_LIBRARY_PATH=/path/to/Wolfram/SystemFiles/IncludeFiles/C
cmake --build build-linux --parallel
WolframKernel -noinit -noprompt -script tests/interface_wolfram.wl \
  "$PWD/build-linux/libxpermLL.so"
```

## 5. Performance finding and remaining work

Consider a fully symmetric rank-k tensor contracted with an otherwise generic rank-k tensor, with a symmetric metric. Use `2k` real slots, dummy pairs `(1,2),(3,4),...`, the configuration `{1,3,...,2k-1,2,4,...,2k}`, and adjacent slot transpositions on the first k slots. Supply that same symmetric subset explicitly to the extended API.

At k=8, instrumentation after each level's duplicate removal recorded the following retained configuration counts:

```text
8, 56, 336, 1680, 6720, 20160, 40320, 40320,
5040, 720, 120, 24, 6, 2, 1, 1
```

The peak is `8! = 40,320`. This occurs in the baseline and remains after the repairs. Consequently, **the paper's central acceleration has not been established or implemented by this patch**. Small timing measurements would not override this direct evidence of factorial growth, and no general speedup claim is made.

A faithful next implementation should:

1. Maintain the paper's configuration pair `(g,s)` and incremental `Values` / `Label-Codes` in the production path.
2. Implement B.1, B.2, and B.4, including propagation through dummy partners and the changing label stabilizer.
3. Implement B.3's zero checks **before** discarding symmetry-equivalent branches.
4. Only then enable Algorithm 4.2's redundant-instance pruning. Replacing this step with sorting or unchecked branch deletion risks losing cancellations.
5. Retain the independent oracle and add scaling families from Section 5 of the paper, covering symmetric, antisymmetric, mixed, repeated-index, no-metric, antisymmetric-metric, and known difficult cases.

Stage 6's explicit group closure should remain clearly identified as a bounded utility, or be removed from the production-facing API if it has no independent use. It should not be presented as the missing incremental mechanism.

## 6. Compatibility and delivery

The repaired Niehoff API uses natural slot/label lexicographic order. The supplied BSGS base describes the group, rather than defining a different canonical-order convention. This can change representatives returned for nonstandard bases, and it need not reproduce original xPerm's byte-for-byte representative. The old `LL_canonical_perm` entry point remains available; its boundary validation was strengthened.

The extended API now explicitly treats subset declarations as additional symmetries. If a caller intended them only as hints, it should supply only subsets already valid for its tensor. They are not inferred from the tensor, and declaring a false symmetry changes the mathematical problem.

The delivery consists of this assessment, an apply-ready patch against the baseline commit, and a patched source archive. The archive omits the upstream precompiled binaries so they cannot be mistaken for repaired builds. Rebuild the shared library before use. No changes were pushed to GitHub and no pull request was created.

For an existing clean checkout at the reviewed commit:

```sh
git switch -c niehoff-review-fixes
git apply --check /path/to/xpermLibraryLink-Niehoff-fixes-v2.patch
git apply /path/to/xpermLibraryLink-Niehoff-fixes-v2.patch
```

The patch also includes this assessment as `ASSESSMENT.md`, the new tests, and updated build documentation.

## 7. Correction of the notebook performance regression

The initial patch slowed the sample notebook's `ToCanonical[FProduct[125]]`. The first review did not benchmark this workload before delivery. Revision 2 fixes the repeated normalization work and restores an optimized default build, while retaining the correctness repairs. The measured medians for the original, first patch, and revision 2 are respectively 5.180 s, 8.907 s, 2.141 s under matched Release/native/LTO settings. All return zero. Full measurements, causes, tests, and instructions are in [PERFORMANCE.md](PERFORMANCE.md).
