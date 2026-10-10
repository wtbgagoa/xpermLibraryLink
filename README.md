# xpermLibraryLink
This is a fork of xperm C-sources used in the [xAct](http://www.xact.es/xPerm/index.html) project. The purpose of this fork is to
replace the [WSTP](https://www.wolfram.com/wstp/) connection used by the original xperm C-source by a newer and faster
[LibraryLink](https://reference.wolfram.com/language/guide/LibraryLink.html) connection. This is a shared library that is loaded
on-demand when required. This branch also contains an experimental canonicalizer
inspired by [Ben Niehoff's work](https://github.com/bniehoff/tensor-canonicalizer);
see the implementation status below.


# Wolfram Language paclet

The wrapper is available as the **xPermLibraryLink** paclet in the
``xAct`xPermLibraryLink` `` context. Build and install it for the current user:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target install-paclet --parallel
```

Then, in a fresh Wolfram kernel:

```wl
PacletDataRebuild[]; (* Only if you install this paclet for the first time *)
Needs["xAct`xPermLibraryLink`"];
Uninstall@$xpermLink;
LoadxPermLibraryLink[Automatic, True]
```

`Needs` loads xPerm when necessary. `LoadxPermLibraryLink` uses the native library
bundled with this paclet; `True` selects the Niehoff canonicalizer. Use
`LoadxPermLibraryLink[]` for the original xPerm canonicalizer and
`UnloadxPermLibraryLink[]` to restore xPerm's previous native definitions and
availability flag. Loading the package alone does not replace those definitions.

See [PACLET.md](PACLET.md) for prerequisites, installation, archive creation,
context migration, and integration tests.

# Prerequisites
* A working installation of [xAct](http://www.xact.es/).
* Wolfram Language 12.1 or newer for the paclet format, with a licensed kernel
  for the `paclet` and `install-paclet` targets. Runtime validation is described
  in [PACLET.md](PACLET.md).
* CMake >= 3.15 (only for building from source).
* A C\C++ compiler (only for building from source).

# Precompiled libraries
The original libraries under `binaries/` predate the fixes on this branch.
Build the paclet to include the current native library. A distributed `.paclet`
archive can be installed with `PacletInstall["/absolute/path/to/archive.paclet"]`;
there is no need to copy its native library onto `$LibraryPath`.

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
