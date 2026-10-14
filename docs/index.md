# xPermLibraryLink: algorithms and implementation

This documentation explains **how tensor canonicalization becomes permutation
code**, and how the two production algorithms differ. It describes the
accelerated 0.2.0 implementation, including its deliberate departures from
literal paper pseudocode. The comments beside the code provide the local
contracts; these pages connect those contracts into an algorithm.

## Start here

1. @subpage algorithm_overview — the mathematical problem, backend selection,
   shared machinery, and a source map.
2. @subpage representations — permutation direction, composition, signs,
   dummy partners, and array conventions. Read this before following a loop.
3. @subpage butler_portugal — the original xPerm free-index and double-coset
   search, its relation to Butler and Portugal, and its actual work arrays.
4. @subpage niehoff_algorithm — a paper-to-code map for Algorithms 4.1/4.2 and
   B.1–B.4, with the maintained invariants and a worked contraction.
5. @subpage design_choices — why the implementation uses these representations
   and pruning rules, and which costs and limitations remain.
6. @subpage librarylink_integration — Wolfram/paclet dispatch, ownership,
   failure handling, and the distinction between production and helper exports.
7. @subpage validation — how to check the claims against independent oracles,
   run the documentation example, and interpret benchmarks.
8. @subpage building_documentation — generate, browse, and maintain this guide.
9. @subpage references — primary references and numbering conventions.

## Jump from a question to the code

| Question | Entry point |
| --- | --- |
| Where does the original backend start? | @ref LL_canonical_perm and @ref canonical_perm_ext |
| Where does the accelerated backend start? | @ref LL_niehoff_canonical_perm and @ref xperm::niehoff::canonicalizeLegacyInput |
| Where is the accelerated main loop? | @ref niehoff_search "incrementalSearch" in `src/NiehoffCanonicalizer.cpp` |
| Where are total slot subsets discovered? | @ref xperm::niehoff::inferTotalSymmetrySubsets |
| How are dummy partners and their signs represented? | @ref niehoff_incremental_labels "IncrementalLabels" and @ref representations |
| What prevents unsafe pruning? | @ref niehoff_propagated_symmetries "PropagatedSymmetries" and @ref niehoff_algorithm |
| Does Stage 6 implement the paper's B.2? | No; @ref xperm::niehoff::runPropagatedSymmetrySearch is a separate bounded group-closure utility. |

The **Files** and **Namespaces** views provide browsable source and API
references. Internal implementation classes are included because the most
important paper correspondence is inside the production search. Generic
inherited utility functions are also visible; visibility is not a promise that
every low-level raw-pointer entry point performs public-boundary validation.

## Build the guide

With Doxygen 1.9.1 or later available:

```sh
cmake -S . -B build-docs -DBUILD_LIBRARYLINK=OFF -DBUILD_TESTING=OFF \
  -DBUILD_DOCUMENTATION=ON
cmake --build build-docs --target docs
```

Open `build-docs/documentation/html/index.html`. Wolfram is not needed for this
target. Graphviz is optional; the text, source links, and algorithm explanations
remain available without it. See @ref building_documentation for all options.
