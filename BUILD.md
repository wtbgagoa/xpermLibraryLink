# Building and testing

A C++17 compiler and CMake 3.15 or newer are required. The default build includes
both the LibraryLink shared library and the standalone regression tests.

## Standalone core tests (no Wolfram installation needed)

```sh
cmake -S . -B build-core -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

The canonicalizer tests independently enumerate small signed slot and label
groups and every element of their double coset. They check the resulting
minimum and whether opposite signs imply zero. Coverage includes every input
permutation through four tensor slots for representative groups, fixed-seed
random cases through five slots, arbitrary base and label orderings, supplied
strong generating sets, and symmetric/antisymmetric total subsets. The oracle
does not call the canonicalizer's permutation, normalization, group, or pruning
helpers. Separate LibraryLink tests use mock tensor callbacks and the bundled
SDK header; they do not require a running Wolfram kernel.

For an AddressSanitizer/UndefinedBehaviorSanitizer build with GCC or Clang:

```sh
cmake -S . -B build-sanitize -DBUILD_LIBRARYLINK=OFF -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## LibraryLink on Linux

Configure with the LibraryLink SDK include directory if it is not automatically
found in your Wolfram installation:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
  -DWOLFRAM_LIBRARY_PATH=/path/to/Wolfram/SystemFiles/IncludeFiles/C
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

The shared library is `build-linux/libxpermLL.so`. Place it in a directory on
Wolfram's `$LibraryPath`, for example
`~/.Wolfram/SystemFiles/LibraryResources/Linux-x86-64`, and locate it with
`FindLibrary["libxpermLL"]`. Mock interface tests exercise the C boundary; loading
and running the library with an actual Wolfram kernel is a separate integration
check. With a licensed Wolfram kernel available, run:

```sh
WolframKernel -noinit -noprompt -script tests/interface_wolfram.wl \
  "$PWD/build-linux/libxpermLL.so"
```

This script also tests the xAct wrapper when xAct is installed.

## Notebook performance regression

The antisymmetric tensor ring from `xpermLibraryLinkSample.nb` has a dedicated
benchmark. It checks that odd products vanish and even products do not, and
asserts that the native LibraryLink path was actually called. Run each library
in a fresh kernel and compare the median of warmed runs:

```sh
WolframKernel -noinit -noprompt -script tests/notebook_benchmark.wl \
  "$PWD/build-linux/libxpermLL.so" 125 3
```

For comparable optimization settings to the original branch's local build,
configure with `-DCMAKE_BUILD_TYPE=Release -DXPERM_NATIVE_ARCH=ON
-DXPERM_ENABLE_IPO=ON`. Use identical compiler settings for comparisons, and
avoid other CPU-heavy work during measurement. Do not compare timings from
different machines to the output saved inside a notebook.

A standalone equivalent is also available when `BUILD_TESTING` is enabled:

```sh
./build-core/notebook_performance 125 3
```

This uses the exact native permutation captured from `FProduct[125]` and
generates its slot symmetry group and dummy labels. Without arguments it checks
the odd/even results for one through ten factors; that mode is part of CTest.
Neither benchmark has a machine-dependent timing assertion. See
[PERFORMANCE.md](PERFORMANCE.md) for the measured regression and repair.

Use `-DBUILD_TESTING=OFF` to omit test executables. Single-configuration generators
(such as Unix Makefiles and Ninja) default to `Release` when no build type is
specified. Explicit `Debug`, `RelWithDebInfo`, and other build types retain their
usual compiler flags. With a multi-configuration generator such as Visual
Studio, build with `cmake --build build --config Release`. Portable binaries
are the default; optional
`-DXPERM_NATIVE_ARCH=ON` enables `-march=native` with GCC/Clang for binaries that
will only run on compatible processors. `-DXPERM_ENABLE_IPO=ON` enables IPO/LTO
only after CMake verifies compiler support. On macOS, set
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` explicitly if a universal binary is
needed and both architectures are supported by your toolchain.

Existing `ENABLE_PGO_GENERATE` and `ENABLE_PGO_USE` options are available with
GCC/Clang. They are mutually exclusive and apply to compilation and linking,
including standalone executables. Run the intended workload with generation
enabled before rebuilding with profile use enabled, keeping the same compiler
and profile locations.

## Cross compiling for Windows on Linux

```sh
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-win --parallel
```

The resulting library is normally `build-win/libxpermLL.dll`. Windows test
executables need a Windows runner or a configured cross-compiling emulator.
