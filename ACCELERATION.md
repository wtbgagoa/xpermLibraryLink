# Symmetry-propagation acceleration, version 0.2.0

**Date:** 13 October 2026. **Baseline:** the GitHub `Niehoff` branch at
[`cb1ce2e0aefbf796a829d7a568c89cae10e2155a`](https://github.com/wtbgagoa/xpermLibraryLink/tree/cb1ce2e0aefbf796a829d7a568c89cae10e2155a).
The changes were developed in a fresh checkout of that revision.

## Result and scope

The production canonicalizer now uses the central mechanism of B. E. Niehoff,
[*Faster tensor canonicalization*](https://doi.org/10.1016/j.cpc.2018.02.014):
symmetries of total slot subsets propagate through dummy contractions and allow
redundant search branches to be discarded before expanding permutations.

For the symmetric-rank-k/generic-rank-k contraction in section 5 of
[ASSESSMENT.md](ASSESSMENT.md), the old implementation retains `k!`
configurations at its peak. The new implementation retains one configuration
per level. At rank 8 this changes the peak from **40,320 to 1**. This improvement
also applies with antisymmetric source tensors and the supported metric types.
Both the legacy and extended interfaces use the accelerated core; the legacy
interface does not require explicit total-subset declarations.

This is not a general polynomial tensor canonicalizer. Whole-block exchanges
without individual slot transpositions, alternating groups, and many inequivalent
contraction patterns can still require large searches. Constructing a strong
generating set is separate work and can dominate even a search with only one
surviving configuration. The standalone Stage 6 group-closure utility retains
its existing meaning and is not the new propagation implementation.

## Production algorithm and invariants

1. **Infer total subsets from the actual signed slot group.**
   `TotalSymmetry.cpp` proves membership of signed transpositions using Schreier
   sifting. Conjugation preserves transposition membership and sign, so one
   representative is tested per orbit on unordered slot pairs. Connected
   transpositions form maximal total subsets. This finds swaps hidden in
   products of strong generators as well as explicitly supplied swaps, without
   enumerating the group. Its pair-orbit storage is quadratic in the number of
   real slots; inference itself is not asserted to be linear.
2. **Retain `(g,s)` configurations.** `g` maps current slots to labels; `s` maps
   current slots to original slots. A slot representative acts on both. A label
   representative acts on `g`. Deduplication keeps the matching whole pair,
   even though only `g` determines the canonical output and duplicate key.
3. **Maintain label values incrementally (B.4).** The current label stabilizer
   supplies the least target for each active label. Fixing a dummy endpoint
   freezes its partner too. Explicit partner and variance tables support
   nonconsecutive, descending, interleaved, and separately pooled labels.
   Repeated classes and fixed/free labels keep their existing semantics.
4. **Track propagated classes (B.2).** Signed odd identifiers describe source
   subsets; matching even identifiers describe symmetry at contracted partners.
   Entries are stored in original-slot coordinates and accessed through `s`.
   Sources are processed in slot order, and existing classes retain precedence.
   Source singletons are not promoted into symmetries.
5. **Find least-value instances `(p,q)` (B.1).** `p` is in the current slot orbit;
   a lower-valued `q` may be reached outside that orbit through the same even
   propagated class. Class minima are cached per configuration.
6. **Check cancellation before pruning (B.3).** Repeated labels in an
   antisymmetric source, an internal metric/slot sign conflict, or at least two
   equal propagated entries meeting an opposite-sign total slot subset imply
   zero. A lone propagated entry meeting an unrelated subset does not.
7. **Discard only certified equivalent choices (4.2).** Candidates are grouped
   by current total slot subset, propagated class, and whether their partner is
   internal to that subset. A zero propagation entry supplies no certificate,
   so its candidate is retained. Signed propagated swaps, ordinary label
   representatives, and slot representatives construct the survivors. Opposite
   signed configurations and negative identity are still checked separately.

The pairing invariant behind step 7 is that dummy endpoints have propagation
entries `(0,0)`, `(oddK,oddK)`, or `(oddK,evenK)` in either order, with the same
sign. Source propagation, ordinary dummy relabeling, slot movement, and swaps
inside an even class preserve this invariant. Debug builds assert it. For a
common odd class, internal choices can be exchanged as whole pairs; external
choices leave a partner exchange represented by the even class. An even class
supplies actual signed label transpositions. The early sign checks establish
consistency with the slot character before either case is pruned.

## Why the pruning is more conservative than the printed procedure

The existing API promises the natural numeric lexicographic minimum for
arbitrary supported label layouts. Literal pruning solely by slot subset loses
that minimum in concrete small examples:

- With real configuration `{1,3,4,2}`, symmetric slots `{1,2,3}`, and symmetric
  dummy pairs `(1,2),(3,4)`, an internal pair and an external endpoint cannot be
  merged into one choice. The correct minimum is `{1,2,3,4}`.
- With real configuration `{1,2,3,5,6,4}`, pairs `(1,6),(2,4),(3,5)`, symmetric
  metric, and slot swaps `(1,6)` and `(2,3)`, an existing even class and a new
  unmarked source must remain distinct. The minimum is `{1,2,3,4,6,5}`.

Both examples, sparse variations, mixed label classes, and negative characters
are permanent independent-oracle regressions. The refined pruning retains the
one-configuration result on the central contraction family while preserving
these necessary alternatives. This implementation does not claim to be a
verbatim transcription of the printed pseudocode.

## Validation

- Existing canonicalizer suite: **7,817 checks**, zero oracle mismatches.
- New acceleration suite: **3,814 checks**, including independently enumerated
  signed double cosets, arbitrary label layouts, repeated and free labels,
  mixed metric pools, interleaved subsets, hidden group generators, both
  interface forms, explicit overlap regressions, early cancellation, and
  configuration-growth bounds through rank 14.
- New subset-inference suite: **5,842 checks**, including 600 random signed
  groups compared with independent group closure, hidden transpositions,
  negative identity, alternating and pair-block-only groups, and the
  125-factor slot group.
- Existing mock LibraryLink boundary and odd/even notebook ring regressions
  pass. Additional development stress runs compared 33,000 seeded small
  double cosets; failing cases discovered during development were reduced and
  added to the permanent suite. These counts are not a proof for all inputs.
- AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer results,
  and actual Wolfram integration results, are recorded below with the timings.

## Matched performance measurements

Measurements use an AMD Ryzen 7 7435HS, GCC 16.2.1, CMake Release (`-O3`),
portable CPU settings, and no IPO/LTO, identically for baseline and new code.
Each Wolfram library runs in a fresh Wolfram 15.0.1 kernel with xPerm 1.2.4 and
xTensor 1.3.0. Loading is excluded; the notebook comparison uses a warmup and
three measured runs. Native family timings include BSGS construction.

| Workload | GitHub baseline | Version 0.2.0 | Measurement |
| --- | ---: | ---: | --- |
| Rank-8 symmetric/generic contraction | 31.255 ms | 0.205623 ms | Native medians; about 152x faster |
| Rank-12 symmetric/generic contraction | Not run | 1.74579 ms | Native median; peak 1 configuration |
| Rank-14 symmetric/generic contraction | Not run | 4.00832 ms | Native median; peak 1 configuration |
| Captured native 125-factor input | 2.92108 s | 0.398130 s | Native medians; both zero |
| Actual `ToCanonical[FProduct[125]]` | 2.286359 s | 0.430271 s | Wolfram medians; both zero, about 5.3x faster |

The native rank-8 baseline has five measured repetitions; new family timings
have three. Native ring timings have three measured runs after a warmup.
The actual Wolfram runs were:

- Baseline: `2.322384, 2.286359, 2.261556` seconds.
- Version 0.2.0: `0.426062, 0.455263, 0.430271` seconds.

The live, connected WSTP handover regression also passed with the final library:
its 125-factor call returned zero, its even-factor check remained nonzero,
LibraryLink calls were counted, and **original WSTP calls stayed at zero**.
Unload, failed load/reload, source reload, and backend switching restored or
selected the appropriate implementations. Thus the fast result is not an
accidental call to the old xPerm executable.

All five CTest targets passed in Release and in a Debug build with
AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer enabled.
The sanitizer run used ordinary host execution because LeakSanitizer cannot
operate under this environment's sandbox process tracing. The debug pairing
invariant checks passed. The actual Wolfram native-interface and packaged
paclet tests are included in the delivered validation evidence.

The delivery's `evidence/` directory contains the raw measured runs and
validation logs. Timings are observations on this machine, not portable
performance guarantees. Earlier notebook numbers in `PERFORMANCE.md` used
other revisions and build options and should not be compared directly.


## Reproduce the native tests and work counts

```sh
cmake -S . -B build-core -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
./build-core/acceleration_tests --benchmark 14 3
./build-core/notebook_performance 125 3
```

The first benchmark prints rank, tensor signs, metric type, API, elapsed time,
peak surviving configurations, expanded and pruned candidates, propagation
count, and early-zero detection. There are no wall-clock assertions in CTest.
A C++ caller may pass an optional `CanonicalizationStats*` to either
`canonicalizeLegacyInput` or `canonicalizeExtendedInput`; existing one-argument
calls and LibraryLink signatures remain unchanged.

For a baseline comparison, build the unmodified revision separately. The new
benchmark source also supports the old header and library:

```sh
# BASELINE is a separate checkout at cb1ce2e, with its build-core configured
# and built using the same compiler and options as above.
c++ -O3 -std=c++17 -DXPERM_TEST_BASELINE -I"$BASELINE/include" \
  tests/acceleration_tests.cpp \
  "$BASELINE/build-core/libNiehoffCanonicalizer.a" \
  "$BASELINE/build-core/libxperm.a" -o /tmp/acceleration-baseline
/tmp/acceleration-baseline --benchmark 8 5
```

Run the baseline only at small ranks: it still has the factorial search.
The baseline benchmark reports timings and validated results; it has no new
statistics API.

## Build and activate the paclet

```sh
cmake -S . -B build-acceleration -DCMAKE_BUILD_TYPE=Release
cmake --build build-acceleration --parallel
ctest --test-dir build-acceleration --output-on-failure
cmake --build build-acceleration --target install-paclet
```

Version 0.2.0 is the default for a new build directory. An old CMake cache can
retain an older version; override it with `-DXPERM_PACLET_VERSION=0.2.0` or use
the fresh directory shown above. `paclet` creates an archive without installing;
`stage-paclet` stages the folder without invoking a kernel.

In a **fresh Wolfram kernel**:

```wl
Needs["xAct`xTensor`"];
Needs["xAct`xPermLibraryLink`"];
xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, True]
xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[]
```

Both final expressions must return `True`. The first selects the Niehoff core;
`False` selects the original xPerm algorithm through LibraryLink. Loading the
package alone does not activate the native library. Manual WSTP uninstallation
is unnecessary: the existing wrapper replaces the old definitions and restores
them on unload. The live handover test checks actual native call counters.

The current patch applies to `cb1ce2e`; it does not require any of the older
patches from this conversation. Rebuild the native binary after applying it.
The bundled upstream `binaries/` files are historical and remain unchanged.
