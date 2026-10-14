# Examples, tests and performance evidence {#validation}

The algorithm guide describes invariants; the tests check them against
independent computations where feasible. Passing finite tests is not a proof
for arbitrary groups or ranks.

## Run the native tests without Wolfram Language

```sh
cmake -S . -B build-core -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

| CTest name | What it establishes within its tested families |
| --- | --- |
| `canonicalizer_tests` | Canonical results and signs against small signed double-coset enumeration; label representatives, group helpers, arbitrary layouts and base orders. |
| `total_symmetry_tests` | Inferred signed transpositions/subsets against independent group closure; hidden swaps, negative identity, alternating groups, whole-pair-only exchanges and the 125-factor slot group. |
| `acceleration_tests` | Propagated search against an independent oracle, internal/external and overlapping-class regressions, both input APIs, metric characters, repeated/free labels, and configuration-growth bounds through rank 14. |
| `interface_regression` | Argument shape, integer narrowing, errors and output ownership at the mock LibraryLink boundary. This does not establish real-kernel ABI behavior. |
| `notebook_ring_regressions` | Native odd/even antisymmetric-ring results corresponding to the sample notebook family. |

The independent acceleration oracle explicitly generates the small slot and
label groups, evaluates every signed double-coset element, and checks the
minimum and opposite-sign zero criterion. It does not call the production
composition, group, label-normalization or pruning routines. Randomized cases
use fixed seeds; group-size budgets keep enumeration limited to small cases.

## A complete symmetry-propagation example

The example below constructs the six-real-slot contraction traced in
@ref niehoff_algorithm. It runs both symmetric and antisymmetric source
tensors, checks the full signed result, and prints search-work counts. It
does not require a Wolfram installation.

After the native build above, on a Unix-like system with a C++17 compiler:

```sh
c++ -std=c++17 -Iinclude docs/examples/propagated_contraction.cpp \
  build-core/libNiehoffCanonicalizer.a build-core/libxperm.a \
  -o build-core/propagated-contraction
./build-core/propagated-contraction
```

With other compilers, compile the same file and link the two CMake-built
static libraries using that compiler's usual syntax. Use a matching compiler
and compatible build flags for the libraries and example.

@include propagated_contraction.cpp

Expected output:

```text
symmetric: [1,3,5,2,4,6,7,8]
survivors: 1 1 1 1 1 1
antisymmetric: [1,3,5,2,4,6,8,7]
survivors: 1 1 1 1 1 1
```

The source generators only exchange the first three slots. The exchange of
labels at the last two real slots must therefore account for the dummy
partners of that source symmetry. The antisymmetric case also checks its
negative character.

## Work counts and timings answer different questions

```sh
./build-core/acceleration_tests --benchmark 14 3
./build-core/notebook_performance 125 3
```

The first reports search counts and timings for increasing contraction ranks.
The second reproduces the captured native 125-factor notebook input. Neither
sets a wall-clock pass threshold. The exact surviving-configuration bound is
a stronger regression check for the intended pruning mechanism than a
machine-dependent time threshold.

@ref xperm::niehoff::CanonicalizationStats counts candidates, propagation and
completed-level survivors. It does not count the work spent constructing the
SGS or inferring total subsets; wall-clock measurements include those costs.
Early zero may stop before every level has a survivor count. The
`earlyZeroDetected` flag refers specifically to B.3, not every zero mechanism.

The repository's `ACCELERATION.md` records the 0.2.0 baseline revision,
compiler/platform, matched measurements, sanitizer checks and actual Wolfram
integration evidence. `PERFORMANCE.md` records earlier notebook regression
history. Their different revisions and build settings must not be mixed into
a single speed comparison.

## Real Wolfram integration is a separate check

The mock test cannot verify `LibraryFunctionLoad`, packed tensors, paclet
discovery or replacement of WSTP-owned definitions. With a licensed kernel,
follow `BUILD.md` and `PACLET.md` to build the shared library and run the real
kernel tests in `tests/`. The live handover test counts calls to distinguish
the accelerated LibraryLink backend from the original WSTP executable.

Loading the paclet alone does not activate either native backend. Select
`LoadxPermLibraryLink[Automatic, True]` explicitly for Niehoff and confirm it
returns `True`; see @ref librarylink_integration. Old notebook output cells
are historical measurements, not a benchmark of the currently loaded library.
