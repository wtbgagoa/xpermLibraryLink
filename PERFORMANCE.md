# Correction of the notebook performance regression

**Date:** 10 October 2026

## Finding

The user's report was correct. The first patch introduced a measurable slowdown
in `xpermLibraryLinkSample.nb`'s `ToCanonical[FProduct[125]]`, the odd ring of
125 antisymmetric rank-2 tensors. My initial review did not benchmark this
notebook workload before delivery. That was a gap in the validation, and the
correctness tests did not excuse the regression.

## Controlled measurements

Each measurement used a fresh Wolfram 15.0.1 kernel, xAct/xTensor 1.3.0,
xAct/xPerm 1.2.4, and libraries built with GCC 16.2.1 on the same Linux x86-64
machine. Runs were sequential. Each kernel performed one untimed warmup,
followed by three timed evaluations of the notebook expression, including
`FProduct` construction. Library loading and compilation were excluded.
The table reports elapsed seconds, not the historical timings saved in the
notebook. All runs returned exactly zero.

| Version | Three warmed runs (seconds) | Median | Build settings |
| --- | --- | --- | --- |
| Original Niehoff branch | 5.135, 5.180, 5.217 | 5.180 | Release, native CPU, LTO |
| First delivered patch | 8.907, 9.025, 8.783 | 8.907 | Release, native CPU, LTO |
| Revised patch | 2.124, 2.154, 2.141 | 2.141 | Release, native CPU, LTO |
| Revised patch, portable settings | 2.273, 2.321, 2.296 | 2.296 | Release, native CPU/LTO disabled |

Under the matched native/LTO settings, the first patch was
**71.9% slower** than the original.
The revised patch is **2.42 times as fast as
the original** and **4.16 times as fast as
the first patch** on this workload. These are local measurements of this
particular expression, not a guarantee for every tensor expression or machine.

The installed WSTP executable could not connect in this environment. The
benchmark explicitly selected the LibraryLink-backed path and counted the
native call during warmup; it did not silently time xAct's pure-Wolfram fallback.
The same wrapper was used across the timed library versions.

First-evaluation times, excluded from the warmed medians, were 6.814 s, 11.919 s, 2.793 s, 2.934 s in the same table order. The improvement is also present on that first evaluation.

## Causes

1. **Repeated work in label normalization.** To handle arbitrary label ordering
   correctly, the first patch allocated and sorted both dummy-target arrays for
   every candidate. Those arrays depend only on the input label groups. At
   125 dummy pairs, an isolated optimized microbenchmark measured about 3.06
   microseconds per normalization versus 1.28 for the baseline. This is direct
   evidence of an introduced cost even when compiler settings match.
2. **An unoptimized default configuration.** The first patch removed the
   original unconditional optimization flags but did not select a build type
   when none was supplied. A plain single-configuration CMake build therefore
   had no optimization flag. This could cause an additional slowdown; the
   measured native/LTO comparison above uses explicit Release builds and does
   not rely on that explanation.
3. **Other avoidable work in the search.** Revalidating already validated
   internal permutations and constructing a map for sign conflicts added
   unnecessary overhead. The input's supplied base was already the natural
   complete base; base reordering was not the cause for this notebook case.

## Repair

- Compile the label normalization plan once per canonicalization: group
  membership, dummy partners, variances, and sorted target orders.
- Reuse scratch arrays and scan slots in order to discover first occurrences.
  Each candidate now uses a linear pass instead of sorting occurrence and
  target arrays repeatedly. The greedy minimum and antisymmetric sign rules
  are preserved, including unsorted pairs and metrics of either symmetry.
- Keep validation at the public boundary, and compose validated internal
  permutations directly in the hot loop.
- Find opposite signs in adjacent already-sorted configurations, avoiding a
  separate ordered map and copied keys.
- Default an unspecified single-configuration build to Release. Explicit Debug
  and multi-configuration build modes are still respected. Native CPU tuning
  and LTO remain opt-in; the portable result is included in the table above.

This repair retains the previous correctness fixes. It does not introduce an
odd-factor shortcut: the same search handles both odd and even rings.
It also does not implement the paper's missing incremental symmetry propagation;
the earlier assessment of factorial growth for other highly symmetric families
still applies.

## Validation and reproducibility

The original independent suite still passes **7,817 checks with zero oracle
mismatches**. A new CTest target checks that rings with one through ten factors
vanish exactly for odd factor counts. All three CTest targets pass in Release
and with AddressSanitizer/UndefinedBehaviorSanitizer. Leak detection remains
disabled because of the environment's tracing restrictions.

Two reproducible benchmarks are included:

```sh
# Actual Mathematica/xAct workload, in a fresh kernel:
WolframKernel -noinit -noprompt -script tests/notebook_benchmark.wl \
  "$PWD/build-linux/libxpermLL.so" 125 3

# Native workload without Wolfram; includes the captured permutation at 125:
./build-core/notebook_performance 125 3
```

Use these settings to match the original branch's local optimized build:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
  -DXPERM_NATIVE_ARCH=ON -DXPERM_ENABLE_IPO=ON
cmake --build build-linux --parallel
```

Restart the Wolfram kernel before testing a rebuilt shared library, and pass
its absolute path. Otherwise an already loaded library can obscure which
version is being measured. The benchmark prints its library path and confirms
that the native function was invoked.

## Applying this correction

- If the first delivered patch is already applied, use
  `xpermLibraryLink-performance-followup.patch`.
- For a clean checkout of the reviewed GitHub commit
  `058f072ad1a790b5db574af6b946a1cc38a9e97a`, use
  `xpermLibraryLink-Niehoff-fixes-v2.patch`.
- Alternatively, extract `xpermLibraryLink-Niehoff-fixed-v2.zip` and build it.

Do not apply both patches to the same checkout. Both patch paths are checked
against their respective starting versions and must produce identical revised
source files. The original deliverables are retained for comparison.
