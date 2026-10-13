#ifndef XPERM_TOTAL_SYMMETRY_HPP
#define XPERM_TOTAL_SYMMETRY_HPP

#include "NiehoffCanonicalizer.hpp"

namespace xperm::niehoff {

struct TotalSymmetryAnalysis {
  std::vector<TotalSymmetrySubset> subsets;
  // One-based real-slot lookup. Zero denotes no total symmetry; the sign of
  // every other entry denotes symmetry (+) or antisymmetry (-).
  IntegerRow subsetOfSlot;
  bool zero = false;
};

// Infer the maximal disjoint total (anti)symmetric subsets of a signed slot
// group. Requires a valid complete strong generating set and stabilizer chain,
// with real slots preceding any sign points in the base (as in legacyChain).
// The final two permutation points encode the sign. No additional symmetry is
// assumed: each inferred transposition is proved to belong to the slot group.
TotalSymmetryAnalysis
inferTotalSymmetrySubsets(const StabilizerChainState &chain);

} // namespace xperm::niehoff

#endif
