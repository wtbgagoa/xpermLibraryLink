#ifndef XPERM_NIEHOFF_CANONICALIZER_HPP
#define XPERM_NIEHOFF_CANONICALIZER_HPP

#include <cstddef>
#include <vector>

namespace xperm::niehoff {

using Permutation = std::vector<int>;
using PermutationBatch = std::vector<Permutation>;
using IntegerRow = std::vector<int>;
using IntegerMatrix = std::vector<IntegerRow>;

struct MinimumImagesResult {
  IntegerRow minimum;
  std::vector<std::size_t> survivorIndices;
};

struct SearchConfiguration {
  // Accumulated slot action s and current slot-to-label configuration g.
  // A slot transversal t acts on BOTH fields on the left; a label renaming
  // acts on g on the right. The separate sign is used by staged helpers.
  Permutation slotPermutation;
  Permutation labelPermutation;
  int sign = 1;
  IntegerRow metadata;
  IntegerRow fixedLabels;
};

struct SearchLevelInput {
  std::vector<SearchConfiguration> configurations;
  PermutationBatch slotTransversals;
  PermutationBatch labelTransversals;
  std::vector<std::size_t> parentIndices;
  int selectedPoint = 1;
};

struct SearchLevelResult {
  int minimumImage = 0;
  std::vector<SearchConfiguration> configurations;
  std::vector<std::size_t> parentIndices;
};

struct BSGS {
  int degree = 0;
  IntegerRow base;
  PermutationBatch strongGenerators;
};

struct BasicOrbit {
  int root = 0;
  IntegerRow points;
  IntegerRow nu;
  IntegerRow w;
};

struct GroupSearchLevelInput {
  std::vector<SearchConfiguration> configurations;
  IntegerRow tentativeBase;
  PermutationBatch slotGenerators;
  int selectedSlot = 1;
};

struct GroupSearchLevelResult {
  int minimumImage = 0;
  std::vector<SearchConfiguration> configurations;
  std::vector<std::size_t> parentIndices;
  BSGS group;
  BasicOrbit orbit;
  PermutationBatch nextStabilizerGenerators;
};

struct StabilizerChainState {
  int degree = 0;
  IntegerRow base;
  PermutationBatch strongGenerators;
  std::vector<IntegerRow> generatorPositions;
  std::size_t level = 0;
};

struct PersistentSearchInput {
  std::vector<SearchConfiguration> configurations;
  IntegerRow tentativeBase;
  PermutationBatch slotGenerators;
  IntegerRow selectedSlots;
};

struct SearchLevelSummary {
  int selectedSlot = 0;
  int minimumImage = 0;
  IntegerRow orbit;
  std::size_t configurationCount = 0;
};

struct PersistentSearchResult {
  std::vector<SearchConfiguration> configurations;
  StabilizerChainState group;
  std::vector<SearchLevelSummary> levels;
};

enum class LabelGroupType : int {
  Fixed = 0,
  Free = 1,
  Dummy = 2,
  SymmetricMetric = 3,
  AntisymmetricMetric = 4,
  Repeated = 5
};

struct LabelGroup {
  LabelGroupType type = LabelGroupType::Fixed;
  IntegerRow labels;
};

struct LabelCandidate {
  int canonicalLabel = 0;
  Permutation representative;
  IntegerRow newlyFixedLabels;
  int sign = 1;
};

struct LabelGroupSearchInput {
  std::vector<SearchConfiguration> configurations;
  IntegerRow tentativeBase;
  PermutationBatch slotGenerators;
  IntegerRow selectedSlots;
  std::vector<LabelGroup> labelGroups;
};

struct LabelGroupSearchResult {
  std::vector<SearchConfiguration> configurations;
  StabilizerChainState group;
  std::vector<SearchLevelSummary> levels;
  bool zero = false;
};

struct SignedPermutation {
  Permutation permutation;
  int sign = 1;
};

struct PropagatedSymmetryInput {
  LabelGroupSearchInput labelSearch;
  std::vector<SignedPermutation> generators;
  std::size_t maximumOrbitSize = 100000;
};

struct PropagatedSymmetryResult {
  std::vector<SearchConfiguration> configurations;
  StabilizerChainState group;
  std::vector<SearchLevelSummary> levels;
  bool zero = false;
  std::size_t mergedConfigurationCount = 0;
  std::size_t cancelledConfigurationCount = 0;
};

struct LegacyCanonicalPermInput {
  Permutation permutation;
  int degree = 0;
  bool slotGeneratorsAreStrong = false;
  IntegerRow base;
  PermutationBatch slotGenerators;
  IntegerRow freeLabels;
  IntegerRow dummySetLengths;
  IntegerRow dummyLabels;
  IntegerRow metricSymmetries;
  IntegerRow repeatedSetLengths;
  IntegerRow repeatedLabels;
};

struct TotalSymmetrySubset {
  IntegerRow slots;
  int sign = 1;
};

struct ExtendedCanonicalPermInput {
  LegacyCanonicalPermInput legacy;
  // Additional declared slot symmetries, combined with legacy.slotGenerators.
  // The order within a subset has no meaning. These are not pruning hints.
  std::vector<TotalSymmetrySubset> totalSymmetrySubsets;
};

struct LegacyCanonicalPermResult {
  bool zero = false;
  Permutation permutation;
};

// Stage 1: basic xPerm-backed permutation operations and batch helpers.
bool isPermutation(const Permutation &permutation);
Permutation product(const Permutation &first, const Permutation &second);
Permutation inverse(const Permutation &permutation);
int onPoint(int point, const Permutation &permutation);
PermutationBatch batchProduct(const PermutationBatch &first,
                              const PermutationBatch &second);
PermutationBatch batchInverse(const PermutationBatch &permutations);
std::vector<int> batchOnPoints(const std::vector<int> &points,
                               const PermutationBatch &permutations);
IntegerMatrix batchApply(const IntegerMatrix &values,
                         const PermutationBatch &permutations);
MinimumImagesResult minimumImages(const IntegerMatrix &rows,
                                  const std::vector<std::size_t> &columns);
IntegerMatrix sortUniqueRows(IntegerMatrix rows);

// Stage 2: explicit-transversal search level.
std::vector<SearchConfiguration>
expandConfigurations(const SearchLevelInput &input);
std::vector<int>
candidatePointImages(const std::vector<SearchConfiguration> &configurations,
                     int selectedPoint);
SearchLevelResult
filterMinimumAndDeduplicate(std::vector<SearchConfiguration> configurations,
                            std::vector<std::size_t> parentIndices,
                            int selectedPoint);
SearchLevelResult advanceSearchLevel(const SearchLevelInput &input);

// Stage 3: BSGS and single group-driven search level.
BSGS makeBSGS(const IntegerRow &tentativeBase,
              const PermutationBatch &generators);
PermutationBatch stabilizerOfBasePrefix(const BSGS &group,
                                        std::size_t prefixLength);
BasicOrbit basicOrbit(const BSGS &group, std::size_t level);
Permutation traceRepresentative(const BasicOrbit &orbit, int point, int degree);
GroupSearchLevelResult
advanceGroupSearchLevel(const GroupSearchLevelInput &input);

// Stage 4: persistent stabilizer-chain multi-level search.
StabilizerChainState
initializeStabilizerChain(const IntegerRow &tentativeBase,
                          const PermutationBatch &generators,
                          const IntegerRow &selectedSlots);
BasicOrbit currentBasicOrbit(const StabilizerChainState &state);
PersistentSearchResult
runPersistentGroupSearch(const PersistentSearchInput &input);

// Stage 5: structured label groups and implicit label representatives.
void validateLabelGroups(const std::vector<LabelGroup> &groups, int degree);
std::vector<LabelCandidate>
canonicalizeLabel(int sourceLabel, const IntegerRow &fixedLabels,
                  const std::vector<LabelGroup> &groups, int degree);
LabelGroupSearchResult runLabelGroupSearch(const LabelGroupSearchInput &input);

// Stage 6: bounded explicit signed-group equivalence utility, NOT the paper's
// incremental symmetry propagation. The closure may grow factorially.
void validatePropagatedSymmetries(
    const std::vector<SignedPermutation> &generators, int degree);
PropagatedSymmetryResult
reduceByPropagatedSymmetries(std::vector<SearchConfiguration> configurations,
                             const std::vector<SignedPermutation> &generators,
                             std::size_t maximumOrbitSize = 100000);
PropagatedSymmetryResult
runPropagatedSymmetrySearch(const PropagatedSymmetryInput &input);

// Stage 7: adapter for the existing LL_canonical_perm data layout. Results use
// natural slot/label lexicographic order, independent of the supplied BSGS base;
// they need not match the original xPerm representative byte for byte.
std::vector<LabelGroup>
makeLegacyLabelGroups(const LegacyCanonicalPermInput &input);
LegacyCanonicalPermResult
canonicalizeLegacyInput(const LegacyCanonicalPermInput &input);
LegacyCanonicalPermResult
canonicalizeExtendedInput(const ExtendedCanonicalPermInput &input);

} // namespace xperm::niehoff

#endif
