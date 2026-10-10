# xpermLibraryLink
This is a fork of xperm C-sources used in the [xAct](http://www.xact.es/xPerm/index.html) project. The purpose of this fork is to
replace the [WSTP](https://www.wolfram.com/wstp/) connection used by the original xperm C-source by a newer and faster
[LibraryLink](https://reference.wolfram.com/language/guide/LibraryLink.html) connection. This is a shared library that is loaded
on-demand when required. This branch also contains an experimental canonicalizer
inspired by [Ben Niehoff's work](https://github.com/bniehoff/tensor-canonicalizer);
see the implementation status below.


# Prerequisites
* A working installation of [xAct](http://www.xact.es/).
* CMake >= 3.15 (only for building from source).
* A C\C++ compiler (only for building from source).

# Precompiled libraries
If you just want a ready to use compiled library you can simply download it from the `binaries`
folder of this repository (choose the library suitable for your platform). You should then copy the library to one
of the directories that is in the `$LibraryPath` variable (this is a Mathematica variable that you can
check from within a Mathematica session).

## Status of the Niehoff branch

The C++ canonicalizer is experimental. It uses xPerm stabilizer chains and
implicit label normalization, but does **not** yet implement the incremental
symmetry propagation and redundant-branch pruning in Algorithms 4.1/4.2 and
B.1-B.4 of Niehoff's *Faster tensor canonicalization*. The standalone Stage 6
utility enumerates an explicitly supplied signed group; it is not a substitute
for that algorithm. Highly symmetric contractions can still generate factorially
many search configurations.

`canonicalizeLegacyInput` and `LL_niehoff_canonical_perm` use the existing signed
permutation data layout (the last two points encode the sign). Canonical results
are lexicographically minimal in natural slot/label order, independently of the
supplied stabilizer-chain base. This convention can differ from the original
xPerm canonical representative. Unlisted real labels are fixed. Dummy pairs
preserve their listed variance when there is no metric; pairs and repeated
labels need not be listed in ascending order.

The extended API treats each total-symmetry subset as an additional declaration
of symmetric or antisymmetric slot permutations. It combines these with the
supplied slot generators and rebuilds the group. Subset entries are unordered,
must be disjoint, and cannot contain sign points. They currently provide
correct group semantics, not the paper's acceleration.

Build and run the independent small-group oracle and LibraryLink boundary tests
without a Wolfram installation:

```sh
cmake -S . -B build-tests -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

See [BUILD.md](BUILD.md) for shared-library and sanitizer builds. Existing files
under `binaries/` are upstream artifacts and are not rebuilt by source changes;
rebuild the library to use these fixes.

See [ASSESSMENT.md](ASSESSMENT.md) for the paper comparison, confirmed defects,
repairs, validation results, and remaining algorithmic work.
