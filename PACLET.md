# xPermLibraryLink paclet

The paclet is named `xPermLibraryLink`, initially version `0.1.0`, and provides
the context ``xAct`xPermLibraryLink` ``. It contains the Wolfram Language wrapper
and a freshly built native library for the selected platform.

## Build and install

Run these commands from the source checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target install-paclet --parallel
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`install-paclet` builds the library and archive, then invokes Wolfram's
`PacletInstall` for the current user. The default destination is
`FileNameJoin[{$UserBasePacletsDirectory, "Repository"}]`, normally
`~/.Wolfram/Paclets/Repository` on current Linux installations. No administrator
privileges are needed. Repeating the target reinstalls the same version so local
rebuilds take effect. Restart kernels using an earlier version before loading
the rebuilt package.

The package requires an existing xAct installation discoverable through
`Needs["xAct`xPerm`"]`. It does not download xAct or assume it is distributed as
a paclet. A working xPerm WSTP executable is not required for the LibraryLink
replacement. Wolfram Language 12.1 or newer supports the paclet metadata format;
the compiled library must also be compatible with the destination kernel's
LibraryLink ABI. Build against the SDK of the installation you intend to use.

If automatic discovery fails, configure explicit paths:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DWOLFRAM_KERNEL_EXECUTABLE=/absolute/path/to/WolframKernel \
  -DWOLFRAM_LIBRARY_PATH=/absolute/path/to/Wolfram/SystemFiles/IncludeFiles/C
```

For the native optimization settings used in the earlier performance comparison,
also set `-DXPERM_NATIVE_ARCH=ON -DXPERM_ENABLE_IPO=ON`. Keep the default portable
settings for distribution to other processors.

## Use from Mathematica or Wolfram Language

In a fresh kernel:

```wl
PacletDataRebuild[]; (* Only if you install this paclet for the first time *)
Needs["xAct`xPermLibraryLink`"];
Uninstall@$xpermLink;
LoadxPermLibraryLink[Automatic, True]
```

`Needs` checks whether ``xAct`xPerm` `` is already present in `$Packages` and loads
it if necessary, before referring to its private MathLink functions. If xPerm
cannot be loaded, the wrapper reports the missing dependency and does not finish
loading. Installing xAct and retrying `Needs` in that session is supported.

`LoadxPermLibraryLink[Automatic, True]` activates the Niehoff implementation and
returns `True` on success. `LoadxPermLibraryLink[]` activates the original xPerm
canonicalizer. The default `Automatic` resolves the library inside this exact
paclet, avoiding unrelated copies of `libxpermLL` elsewhere on the library path.
There is no need to call `FindLibrary`, edit `$LibraryPath`, or copy binaries.
An explicit path remains supported for source development:

```wl
LoadxPermLibraryLink["/absolute/path/to/libxpermLL.so", True]
```

Loading the package alone does not activate the replacement. Activation saves
xPerm's original private definitions and `$xpermQ` flag, installs the native
hooks, and sets the flag to enable native dispatch from xTensor.
`UnloadxPermLibraryLink[]` restores the saved definitions and flag.
`xPermLibraryLinkLoadedQ[]` reports whether the native hooks are active.

The previous context ``xPermLibraryLink` `` has moved to
``xAct`xPermLibraryLink` ``. Change fully qualified references accordingly.
Existing unqualified calls work after loading the new context. The source-root
`xPermLibraryLink.m` can still be loaded with `Get`; in that case use an explicit
native-library path. For installed use, replace the notebook's old `Get` and
`FindLibrary` setup with the two lines above.

## CMake targets and configuration

| Target | Result | Licensed kernel needed |
| --- | --- | --- |
| `stage-paclet` | Unpacked paclet including the freshly built library | No |
| `paclet` | Distributable `.paclet` archive | Yes |
| `install-paclet` | Archive and installation in the user paclet repository | Yes |

For a single-configuration build, the staged directory is
`build/paclet/xPermLibraryLink` and the archive is
`build/paclet/xPermLibraryLink-0.1.0.paclet`. Multi-configuration generators add
the configuration name below `paclet`; use `--config Release` when building.
These targets require `BUILD_LIBRARYLINK=ON`, the default.

| CMake setting | Purpose |
| --- | --- |
| `WOLFRAM_KERNEL_EXECUTABLE` | Kernel used to create and install archives |
| `XPERM_PACLET_VERSION` | Version recorded in metadata; default `0.1.0` |
| `XPERM_WOLFRAM_SYSTEM_ID` | Override the target platform's Wolfram `$SystemID` |
| `XPERM_PACLET_BASE_DIRECTORY` | Optional alternative paclet repository base; passed as `-pacletbase` |

The platform is derived from the compiler target, not the host kernel, so a
cross-built archive is labelled for its destination. Install that archive with
a matching kernel on the destination machine. A build packages its own target
library; it does not include the repository's old precompiled binaries.

To distribute an archive without installing it:

```sh
cmake --build build --target paclet --parallel
```

Recipients install it with:

```wl
PacletInstall["/absolute/path/to/xPermLibraryLink-0.1.0.paclet"]
```

For development, `PacletDirectoryLoad["/absolute/path/to/build/paclet/xPermLibraryLink"]`
makes the staged paclet visible only in the current session.

## Integration tests

The native CTest suite does not require a running Wolfram kernel. The separate
`tests/paclet_wolfram.wl` script requires a licensed kernel and xAct. It covers
dependency autoload and failure/retry, bundled-library lookup, both canonicalizers,
native dispatch from xTensor, unloading, failed library loads, and source reloads.
Each case must run in a fresh kernel with an isolated paclet base:

```sh
cmake --build build --target stage-paclet --parallel
mkdir -p build/paclet-test-base
WolframKernel -noinit -noprompt \
  -pacletbase "$PWD/build/paclet-test-base" \
  -script tests/paclet_wolfram.wl \
  "$PWD/build/paclet/xPermLibraryLink" staged \
  "$PWD/build/paclet-test-base" auto
```

Run the same command with the final argument `preload` to test an already loaded
xPerm. To test installation and fresh-session discovery, configure
`XPERM_PACLET_BASE_DIRECTORY` to that isolated base, build `install-paclet`,
and run the script with mode `installed` instead of `staged`.
The test refuses to install into the normal Wolfram user directory.

### Validation on 2026-10-10

On Linux x86-64 with Wolfram 15.0.1 and xAct xPerm 1.2.4 / xTensor 1.3.0:

- All three native CTest targets passed.
- All four fresh-kernel integration cases passed: staged and installed paclets,
  each with automatic dependency loading and with xPerm already loaded.
- The explicit-path LibraryLink integration suite passed.
- Archive creation and repeated same-version installation succeeded in an
  isolated paclet repository. Staging also succeeded without a usable kernel;
  archive creation then failed with the intended missing-kernel diagnostic.
- The notebook's 125-factor antisymmetric ring returned zero through the native
  implementation. Three warmed runs took 2.251718, 2.271542, and 2.241453 seconds
  (median 2.251718 seconds, portable Release build). The wrapper itself enabled
  native dispatch; the benchmark did not force the flag.

The C++ sources and headers are unchanged from the performance-corrected v2
delivery. Paclet runtime validation was performed on Linux; macOS and Windows
build paths have not been runtime-tested here.

## References

The layout uses Wolfram's documented Kernel and LibraryLink extensions and its
archive/install functions: [Paclets Overview](https://reference.wolfram.com/language/tutorial/Paclets.html).
The installation override follows
[$UserBasePacletsDirectory](https://reference.wolfram.com/language/ref/$UserBasePacletsDirectory.html).
