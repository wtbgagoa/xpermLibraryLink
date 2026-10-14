#ifndef XPERM_TOTAL_SYMMETRY_HPP
#define XPERM_TOTAL_SYMMETRY_HPP

/**
 * @file TotalSymmetry.hpp
 * @brief Exact recovery of total symmetric/antisymmetric slot subsets.
 *
 * This supplies the paper's Symmetric-Subsets array from the actual slot group,
 * allowing the legacy API to use the acceleration without new input metadata.
 * See @ref niehoff_algorithm and @ref design_choices.
 */

#include "NiehoffCanonicalizer.hpp"

/**
 * @defgroup symmetry_inference Total-symmetry inference
 * @brief Prove signed transposition membership without enumerating the group.
 *
 * Connected signed transpositions generate a total subset. An orbit on
 * unordered slot pairs represents conjugate transpositions, so only one
 * membership query per such orbit is needed. A general slot orbit is not a
 * total subset: cyclic, alternating and whole-block permutation groups can
 * be transitive without containing the required individual transpositions.
 */

namespace xperm::niehoff {

/// Inferred total subsets, a direct lookup table, and any negative-identity result.
/// @ingroup symmetry_inference
struct TotalSymmetryAnalysis {
  std::vector<TotalSymmetrySubset> subsets; ///< Maximal disjoint subsets, excluding singletons.
  // One-based real-slot lookup. Zero denotes no total symmetry; the sign of
  // every other entry denotes symmetry (+) or antisymmetry (-).
  IntegerRow subsetOfSlot; ///< Signed one-based subset IDs; entry 0 is unused.
  bool zero = false; ///< The signed group contains a negative identity.
};

/**
 * @brief Infer maximal total (anti)symmetric subsets of a signed slot group.
 * @ingroup symmetry_inference
 *
 * Direct transposition generators seed connected components. For the remaining
 * pairs, Schreier sifting proves membership; conjugation extends each answer to
 * its orbit on unordered pairs. Thus transpositions hidden in products of SGS
 * generators are found, without signed-group closure. This is exact inference
 * for a valid complete SGS, not a heuristic inspection of generator shapes.
 *
 * @param chain Complete valid SGS and chain, with all real base points before
 * any sign points. The final two permutation points encode sign. `level` is
 * ignored: inference uses the complete group, before the production search.
 * @return Disjoint total subsets and their signed lookup, or zero=true when a
 * negative identity is detected. On zero, partially built subsets need not be
 * complete and must not be used for canonicalization.
 * @pre The SGS and generatorPositions accurately represent the full group;
 * malformed chain content is not exhaustively validated here.
 * @throws std::invalid_argument Invalid degree, chain size or base-point order.
 *
 * With r real slots, the pair-visit table has r(r-1)/2 entries. Lazy Schreier
 * trees and inverse generators are additional storage; each tree has linear
 * size in the degree. No linear-time inference claim is made. This cost is
 * separate from the reduction in surviving search configurations.
 * @see canonicalizeLegacyInput(), canonicalizeExtendedInput()
 */
TotalSymmetryAnalysis
inferTotalSymmetrySubsets(const StabilizerChainState &chain);

} // namespace xperm::niehoff

#endif
