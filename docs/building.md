# Build and maintain this documentation {#building_documentation}

Install Doxygen 1.9.1 or newer. Graphviz is optional: when its `dot` program is
available, Doxygen also draws class relationship graphs. The prose, tables,
API links and source browser remain available without it. No Wolfram SDK,
licensed kernel, LaTeX installation or internet connection is needed to
generate or read the HTML documentation once the tools are installed. PDF
compilation additionally requires a TeX distribution and `latexmk`.

From the source root:

```sh
cmake -S . -B build-docs \
  -DBUILD_LIBRARYLINK=OFF -DBUILD_TESTING=OFF \
  -DBUILD_DOCUMENTATION=ON
cmake --build build-docs --target docs
```

Open `build-docs/documentation/html/index.html` in a browser. All assets are
local, so the complete `html` directory can be copied to another machine or
served as a static site. Do not copy only its `index.html` file.

To use a Doxygen installation outside the executable search path, add
`-DDOXYGEN_EXECUTABLE=/absolute/path/to/doxygen` at configuration time.
Documentation can also be enabled in a regular native/LibraryLink build; the
two `OFF` options above simply make a documentation-only configuration
independent of the Wolfram SDK and test executables.

## Generate LaTeX and compile a PDF

Enable `DOCUMENTATION_LATEX` to generate LaTeX sources alongside HTML. Doxygen
itself does not compile them:

```sh
cmake -S . -B build-docs \
  -DBUILD_LIBRARYLINK=OFF -DBUILD_TESTING=OFF \
  -DBUILD_DOCUMENTATION=ON -DDOCUMENTATION_LATEX=ON
cmake --build build-docs --target docs
```

The main document is `build-docs/documentation/latex/refman.tex`. On a Unix-like
system, compile it using Doxygen's generated Makefile:

```sh
make -C build-docs/documentation/latex
```

Alternatively, enable `DOCUMENTATION_PDF` to add a CMake target that performs
both Doxygen generation and PDF compilation in the correct order:

```sh
cmake -S . -B build-docs \
  -DBUILD_LIBRARYLINK=OFF -DBUILD_TESTING=OFF \
  -DBUILD_DOCUMENTATION=ON -DDOCUMENTATION_PDF=ON
cmake --build build-docs --target docs-pdf
```

This implies LaTeX generation even if `DOCUMENTATION_LATEX` is left off. The
output is `build-docs/documentation/latex/refman.pdf`. Install a TeX distribution
such as TeX Live with `pdflatex`, `makeindex`, Doxygen's required LaTeX packages,
the `idxlayout` package, and `latexmk` available. The executable tools must be
on the executable search path. The `docs-pdf` target
uses `latexmk` directly, so it does not depend on whether CMake uses Makefiles,
Ninja or another generator. Missing LaTeX packages are reported by the compiler.

The supplied `latex-unicode.sty` maps the composition and arrow characters
used by the guide for pdfLaTeX. It also permits long reference links to wrap
and uses a single-column alphabetical index to fit qualified C++ names.
Edit source comments, guide pages and this
stylesheet; generated `.tex` files are replaced by the next Doxygen run.
The PDF contains the guide and full extracted API, so it is substantially
longer than the introductory guide alone. See
[Doxygen output formats](https://www.doxygen.nl/manual/output.html) and
[latexmk](https://ctan.org/pkg/latexmk) for the underlying tools.

## Use the VS Code CMake Tools extension

Open the project root in VS Code and install Microsoft's **CMake Tools**
extension. With the current kit/settings workflow, merge these entries into
your workspace `.vscode/settings.json`:

```json
{
  "cmake.configureSettings": {
    "BUILD_DOCUMENTATION": "ON",
    "DOCUMENTATION_PDF": "ON"
  }
}
```

An equivalent copyable file is in `docs/examples/vscode-settings.json`. Keep
your existing settings; no editor configuration is changed automatically.
Omit `DOCUMENTATION_PDF` for HTML only, or use `DOCUMENTATION_LATEX` for LaTeX
sources without the PDF target. If Doxygen is not on the executable search
path, add `"DOXYGEN_EXECUTABLE": "/absolute/path/to/doxygen"` inside the same
`cmake.configureSettings` object.

Then use the Command Palette:

1. **CMake: Configure** (select your usual compiler kit if prompted).
2. **CMake: Set Build Target**, selecting `docs` or `docs-pdf`.
3. **CMake: Build**.

The targets also appear in the CMake Tools project view after configuration.
The files are under `<active-build-directory>/documentation/`; use the build
directory selected by CMake Tools, which may differ from the command-line
`build-docs` example. Selecting `docs` generates HTML and any enabled LaTeX
sources; selecting `docs-pdf` additionally compiles the PDF. Neither target
is included in the ordinary default `all` build.

If you use CMake presets, place the same CMake variables in the active
configure preset's `cacheVariables` instead of relying on kit settings. A
build preset whose `targets` is `["docs-pdf"]` can then be selected with
**CMake: Select Build Preset**. The preset must name your existing configure
preset in `configurePreset`. These workflows are described in the official
[CMake Tools guide](https://code.visualstudio.com/docs/cpp/cmake-linux) and
[settings reference](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-settings.md).

## Build options and generated files

| Setting or path | Purpose |
| --- | --- |
| `BUILD_DOCUMENTATION` | Default `OFF`; when enabled, find Doxygen and create the explicit `docs` target. Ordinary builds acquire no documentation dependency. |
| `DOCUMENTATION_WARNINGS_AS_ERRORS` | Default `ON`; fail the target on Doxygen warnings so broken references and malformed comments are visible. Set `OFF` only while investigating a warning. |
| `DOCUMENTATION_LATEX` | Default `OFF`; include LaTeX source generation in `docs`. A TeX installation is not needed until compilation. |
| `DOCUMENTATION_PDF` | Default `OFF`; imply LaTeX generation and add the explicit `docs-pdf` target using `latexmk`. |
| `docs/Doxyfile.in` | Source template; configured by `cmake/XPermDocumentation.cmake`. |
| `build-docs/documentation/Doxyfile` | Generated configuration with the source/build paths filled in. Edit the template, not this file. |
| `build-docs/documentation/warnings.log` | Doxygen warning output; empty for a clean build. Configuration-parser diagnostics may also appear on the terminal. |
| `build-docs/documentation/html/` | Offline HTML guide, searchable reference and linked source views. |
| `build-docs/documentation/xml/` | Structured reference output for external tooling. |
| `build-docs/documentation/latex/` | Optional LaTeX sources, generated Makefile and compiled `refman.pdf`. |

The target regenerates on each explicit invocation so comment and guide edits
cannot be missed by a partial dependency list. It is not part of the default
`all` target. Generated files stay in the build directory and are not installed
as part of the Wolfram paclet.

The tool integration uses CMake's
[FindDoxygen](https://cmake.org/cmake/help/latest/module/FindDoxygen.html);
configuration tags are documented in the
[Doxygen configuration reference](https://www.doxygen.nl/manual/config.html).

## Reading and extending the source comments

The public headers describe contracts, representations and error behavior.
Implementation comments explain the invariants and identify the algorithmic
steps where they are used. The guides provide the cross-file explanation and
paper correspondence that a function list alone cannot provide.

Private/static helpers and anonymous-namespace implementation classes are
included intentionally: they contain the production propagation logic.
Stable `niehoff_*` anchors allow guide links to those internal objects without
depending on Doxygen's spelling of anonymous namespaces. Source links make
the implementation available alongside its explanation.

All inherited functions are extracted so the old backend stays browsable.
This does not imply every old utility has a full parameter manual. The
canonicalization paths, data contracts, major group operations, interfaces and
new acceleration receive the detailed commentary.

When changing an algorithm, update its source comment, the correspondence
table in @ref niehoff_algorithm, and the rationale in @ref design_choices
together. Preserve the distinction between production and staged helpers.
Keep `@anchor` commands in detailed descriptions: put `@details` before an
anchor following `@brief`, so LaTeX indices do not receive paragraph breaks
inside a brief description.
Run the documentation target with warnings treated as errors, check the
affected links in HTML, and rerun the relevant native/integration tests.
If the worked example changes, compile it and update its expected output in
@ref validation. Do not infer performance from a diagram or survivor count
without including the group-preparation costs.
