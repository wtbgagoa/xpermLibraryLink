#ifndef XPERM_NIEHOFF_CANONICALIZER_HPP
#define XPERM_NIEHOFF_CANONICALIZER_HPP

/**
 * @file NiehoffCanonicalizer.hpp
 * @brief Production canonicalization API and separately testable search utilities.
 *
 * Start with @ref algorithm_overview and @ref representations. The production
 * entry points are canonicalizeLegacyInput() and canonicalizeExtendedInput().
 * The historical "Stage 1" through "Stage 6" utilities expose useful building
 * blocks, but calling them in sequence does not reproduce the production
 * symmetry-propagation algorithm; see @ref niehoff_algorithm.
 */

#include <cstddef>
#include <vector>

/**
 * @defgroup niehoff_api Niehoff production canonicalizer
 * @brief Signed tensor canonicalization with incremental symmetry propagation.
 *
 * The result minimizes real-slot images in natural numeric lexicographic order.
 * Slot and label symmetries act on opposite sides of a configuration, as in the
 * Butler–Portugal double-coset problem. Niehoff's additional propagated classes
 * eliminate certified equivalent branches. See @ref niehoff_algorithm.
 */
/**
 * @defgroup staged_helpers Staged permutation and search utilities
 * @brief Independently usable and testable development interfaces.
 *
 * These utilities illustrate permutation operations, Schreier chains, implicit
 * label representatives and bounded signed-group equivalence. In particular,
 * Stage 6 enumerates a group closure and is not Niehoff's incremental B.2.
 * Production shares low-level utilities with these stages, but has its own
 * search and sign representation. See @ref algorithm_overview.
 */

/// C++ canonicalization interfaces; all permutation images are one-based.
namespace xperm::niehoff {

/** @brief Image-list permutation: entry `p[i-1]` is the image of point `i`.
 * @ingroup staged_helpers
 *
 * Generic helpers do not reserve sign points. Production inputs do: their last
 * two images are either unchanged (+1) or exchanged (-1), and never mix with
 * real slots. Composition is left to right: `product(a,b)[i] == b[a[i]-1]`.
 * See @ref representations.
 */
using Permutation = std::vector<int>;
/// Collection of permutations of a common degree. @ingroup staged_helpers
using PermutationBatch = std::vector<Permutation>;
/// Integer data row; interpretation depends on the containing type. @ingroup staged_helpers
using IntegerRow = std::vector<int>;
/// Rectangular integer rows when required by an operation. @ingroup staged_helpers
using IntegerMatrix = std::vector<IntegerRow>;

/// Lexicographic minimum on requested columns and rows attaining it. @ingroup staged_helpers
struct MinimumImagesResult {
  IntegerRow minimum; ///< Values in the requested column order.
  std::vector<std::size_t> survivorIndices; ///< Zero-based indices in the input.
};

/** @brief Configuration used by the staged interfaces. @ingroup staged_helpers
 *
 * A slot representative `t` changes `(g,s)` to `(t*g,t*s)`; a label
 * representative `d` changes `g` to `g*d`. Here products use the left-to-right
 * convention of product(). Metadata is carried through the search and is part
 * of deduplication. Unlike production, staged metric/propagation signs can be
 * recorded separately in @ref sign.
 */
struct SearchConfiguration {
  // Accumulated slot action s and current slot-to-label configuration g.
  // A slot transversal t acts on BOTH fields on the left; a label renaming
  // acts on g on the right. The separate sign is used by staged helpers.
  Permutation slotPermutation; ///< Accumulated slot action `s`.
  Permutation labelPermutation; ///< Current slot-to-label map `g`.
  int sign = 1; ///< Separate character, restricted to +1 or -1.
  IntegerRow metadata; ///< Opaque caller data, retained in equality comparisons.
  IntegerRow fixedLabels; ///< Labels fixed pointwise by later label actions.
};

/// One explicit candidate descriptor per transversal pair. @ingroup staged_helpers
struct SearchLevelInput {
  std::vector<SearchConfiguration> configurations; ///< Parent configurations.
  PermutationBatch slotTransversals; ///< Left slot actions, one per candidate.
  PermutationBatch labelTransversals; ///< Right label actions, one per candidate.
  std::vector<std::size_t> parentIndices; ///< Zero-based parent for each candidate.
  int selectedPoint = 1; ///< One-based slot whose label image is minimized.
};

/// Minimum-filtered result of an explicit search level. @ingroup staged_helpers
struct SearchLevelResult {
  int minimumImage = 0; ///< Least selected-point image; zero for empty input.
  std::vector<SearchConfiguration> configurations; ///< Distinct surviving states.
  std::vector<std::size_t> parentIndices; ///< Original parent of each survivor.
};

/** @brief Base and strong generating set for a permutation group. @ingroup staged_helpers
 * Schreier–Sims data reused from xPerm, common to Butler–Portugal and Niehoff.
 */
struct BSGS {
  int degree = 0; ///< Number of permutation points, including sign points if used.
  IntegerRow base; ///< Ordered one-based points defining the stabilizer chain.
  PermutationBatch strongGenerators; ///< Generators strong relative to @ref base.
};

/// Orbit and xPerm Schreier-tree storage at one chain level. @ingroup staged_helpers
struct BasicOrbit {
  int root = 0; ///< One-based base point whose orbit is represented.
  IntegerRow points; ///< Reachable one-based points, beginning with the root.
  IntegerRow nu; ///< Flat degree-by-degree edge-generator image lists.
  IntegerRow w; ///< Parent point per orbit vertex; zero marks the root.
};

/// One slot-group search step, rebuilding a BSGS. @ingroup staged_helpers
struct GroupSearchLevelInput {
  std::vector<SearchConfiguration> configurations; ///< Parent search states.
  IntegerRow tentativeBase; ///< Preferred base, reordered to start at selectedSlot.
  PermutationBatch slotGenerators; ///< Generators, not necessarily strong.
  int selectedSlot = 1; ///< One-based slot to minimize.
};

/// Survivors and group data from a single group-driven step. @ingroup staged_helpers
struct GroupSearchLevelResult {
  int minimumImage = 0; ///< Least label at the selected slot.
  std::vector<SearchConfiguration> configurations; ///< Distinct least-image states.
  std::vector<std::size_t> parentIndices; ///< Zero-based source parent indices.
  BSGS group; ///< BSGS with the selected slot first.
  BasicOrbit orbit; ///< Orbit used for candidate generation.
  PermutationBatch nextStabilizerGenerators; ///< SGS fixing the first base point.
};

/** @brief Reusable slot stabilizer chain. @ingroup staged_helpers
 * Level `i` uses the subgroup fixing `base[0]` through `base[i-1]`.
 * Keeping generator indices avoids rebuilding a BSGS at every search step.
 */
struct StabilizerChainState {
  int degree = 0; ///< Full permutation degree.
  IntegerRow base; ///< Ordered one-based base points.
  PermutationBatch strongGenerators; ///< Complete SGS for this base.
  std::vector<IntegerRow> generatorPositions; ///< Zero-based SGS indices by level.
  std::size_t level = 0; ///< Zero-based current base position.
};

/// Multi-level slot-only search with a persistent BSGS. @ingroup staged_helpers
struct PersistentSearchInput {
  std::vector<SearchConfiguration> configurations; ///< Initial search states.
  IntegerRow tentativeBase; ///< Preferred points after the selected prefix.
  PermutationBatch slotGenerators; ///< Slot generators, not necessarily strong.
  IntegerRow selectedSlots; ///< Distinct one-based slots, in search order.
};

/// Work summary for one completed staged search level. @ingroup staged_helpers
struct SearchLevelSummary {
  int selectedSlot = 0; ///< One-based slot fixed by this level.
  int minimumImage = 0; ///< Least surviving label there.
  IntegerRow orbit; ///< Slot orbit before fixing the selected slot.
  std::size_t configurationCount = 0; ///< Survivors after deduplication.
};

/// Final staged slot-only search states and work history. @ingroup staged_helpers
struct PersistentSearchResult {
  std::vector<SearchConfiguration> configurations; ///< Final surviving states.
  StabilizerChainState group; ///< Chain advanced past processed levels.
  std::vector<SearchLevelSummary> levels; ///< One record per selected slot.
};

/** @brief Supported factors of the label symmetry group. @ingroup niehoff_api
 * Only monoterm permutation symmetries are represented; no multiterm identities
 * are imposed. Distinct pools never exchange labels. See @ref representations.
 */
enum class LabelGroupType : int {
  Fixed = 0, ///< Every listed label is fixed pointwise.
  Free = 1, ///< Named free labels; also fixed pointwise by label renaming.
  Dummy = 2, ///< Exchange whole pairs, preserving the two variance positions.
  SymmetricMetric = 3, ///< Exchange pairs and reverse endpoints with sign +1.
  AntisymmetricMetric = 4, ///< Exchange pairs; each endpoint reversal has sign -1.
  Repeated = 5 ///< Permute equal component occurrences with sign +1.
};

/// One disjoint label pool, with pair order defining variance. @ingroup niehoff_api
struct LabelGroup {
  LabelGroupType type = LabelGroupType::Fixed; ///< Allowed renaming operations.
  IntegerRow labels; ///< For dummy pools, consecutive entries form pairs.
};

/// Least-image representative for one staged label operation. @ingroup staged_helpers
struct LabelCandidate {
  int canonicalLabel = 0; ///< Least admissible image of the source label.
  Permutation representative; ///< Right action sending source to canonicalLabel.
  IntegerRow newlyFixedLabels; ///< Target label and, for a dummy, its partner.
  int sign = 1; ///< Metric character, separate from the representative images.
};

/// Staged search combining a slot chain and structured label pools. @ingroup staged_helpers
struct LabelGroupSearchInput {
  std::vector<SearchConfiguration> configurations; ///< Initial search states.
  IntegerRow tentativeBase; ///< Preferred base after the selected prefix.
  PermutationBatch slotGenerators; ///< Slot generators, not necessarily strong.
  IntegerRow selectedSlots; ///< Distinct one-based slots in search order.
  std::vector<LabelGroup> labelGroups; ///< Disjoint pools covering the full degree.
};

/// Staged label-search result; it does not perform production propagation. @ingroup staged_helpers
struct LabelGroupSearchResult {
  std::vector<SearchConfiguration> configurations; ///< Final search states.
  StabilizerChainState group; ///< Advanced slot chain.
  std::vector<SearchLevelSummary> levels; ///< Work history.
  bool zero = false; ///< Reserved result flag; Stage 5 currently leaves it false.
};

/// Permutation with an independent character for Stage 6. @ingroup staged_helpers
struct SignedPermutation {
  Permutation permutation; ///< Ordinary point permutation.
  int sign = 1; ///< Separate character, +1 or -1.
};

/** @brief Input to the bounded explicit-equivalence utility. @ingroup staged_helpers
 * This historical name does not refer to production Propagated-Symmetries.
 */
struct PropagatedSymmetryInput {
  LabelGroupSearchInput labelSearch; ///< Stage 5 search performed first.
  std::vector<SignedPermutation> generators; ///< Extra left actions for reduction.
  std::size_t maximumOrbitSize = 100000; ///< Maximum enumerated signed group size.
};

/// Result of explicit signed-group reduction. @ingroup staged_helpers
struct PropagatedSymmetryResult {
  std::vector<SearchConfiguration> configurations; ///< Distinct orbit representatives.
  StabilizerChainState group; ///< Copied from Stage 5 by the combined utility.
  std::vector<SearchLevelSummary> levels; ///< Copied Stage 5 work history.
  bool zero = false; ///< Opposite signed representatives were encountered.
  std::size_t mergedConfigurationCount = 0; ///< Duplicate inputs discarded.
  std::size_t cancelledConfigurationCount = 0; ///< Inputs implicated when zero found.
};

/** @brief Native counterpart of the established `LL_canonical_perm` layout.
 * @ingroup niehoff_api
 *
 * All label lists refer to labels, not slot positions. Each dummy pool consists
 * of consecutive pairs in dummyLabels; the numeric label values need not be
 * consecutive or ascending. Pools and free labels must be disjoint. Labels not
 * listed are fixed. The final two points encode the sign and cannot appear in
 * the free, dummy or repeated lists. See @ref representations.
 *
 * @warning Setting slotGeneratorsAreStrong asserts a complete SGS for base;
 * the adapter validates shapes but does not prove this assertion. Supply false
 * for an arbitrary generating set so Schreier–Sims constructs the SGS.
 */
struct LegacyCanonicalPermInput {
  Permutation permutation; ///< Signed slot-to-label image list, including two sign points.
  int degree = 0; ///< Real slot count plus two; must match every permutation length.
  bool slotGeneratorsAreStrong = false; ///< Whether generators are already a valid SGS.
  IntegerRow base; ///< One-based BSGS base; does not set canonical comparison order.
  PermutationBatch slotGenerators; ///< Signed slot permutations; an empty set is allowed.
  IntegerRow freeLabels; ///< Distinct named labels, fixed under label renaming.
  IntegerRow dummySetLengths; ///< Positive even endpoint counts, one per dummy pool.
  IntegerRow dummyLabels; ///< Concatenated pools of ordered dummy endpoint pairs.
  IntegerRow metricSymmetries; ///< Per pool: 0=no metric, +1=symmetric, -1=antisymmetric.
  IntegerRow repeatedSetLengths; ///< Positive occurrence counts, one per repeated pool.
  IntegerRow repeatedLabels; ///< Concatenated pools of interchangeable occurrences.
};

/// Total slot symmetry: every transposition in slots has the same sign. @ingroup niehoff_api
struct TotalSymmetrySubset {
  IntegerRow slots; ///< At least two distinct one-based real slots; order is irrelevant.
  int sign = 1; ///< +1 for symmetry, -1 for antisymmetry.
};

/** @brief Input that augments the slot group by declared total subsets.
 * @ingroup niehoff_api
 *
 * Subsets must be mutually disjoint. Each contributes signed adjacent
 * transpositions to the group before constructing its SGS. These are additional
 * mathematical symmetries, not optional optimization hints; declaring a false
 * symmetry changes the problem. Legacy inputs already infer their total
 * subsets from the supplied group. See @ref design_choices.
 */
struct ExtendedCanonicalPermInput {
  LegacyCanonicalPermInput legacy; ///< Original permutation, slot group and label pools.
  // Additional declared slot symmetries, combined with legacy.slotGenerators.
  // The order within a subset has no meaning. These are not pruning hints.
  std::vector<TotalSymmetrySubset> totalSymmetrySubsets; ///< Additional slot relations.
};

/// Signed canonical result or an algebraic zero certificate. @ingroup niehoff_api
struct LegacyCanonicalPermResult {
  bool zero = false; ///< True when a real configuration is equivalent to its negative.
  Permutation permutation; ///< Canonical signed image list; empty when zero is true.
};

/** @brief Optional work counters for production correctness/scaling checks.
 * @ingroup niehoff_api
 *
 * Reset at the start of each call. Counts describe search work, not BSGS
 * construction or subset-inference cost. They impose no timing threshold and
 * do not change the mathematical result. See @ref validation.
 */
struct CanonicalizationStats {
  std::vector<std::size_t> survivingConfigurations; ///< Survivors per completed real-slot level.
  std::size_t candidatesExpanded = 0; ///< Least instances expanded after certificate pruning.
  std::size_t symmetryEquivalentCandidatesPruned = 0; ///< Least instances removed by certificates.
  std::size_t propagatedGroupsCreated = 0; ///< Non-singleton odd/even class pairs allocated by B.2.
  std::size_t earlyZeroChecks = 0; ///< B.3 calls, before branch pruning.
  bool earlyZeroDetected = false; ///< B.3 found zero; other zero mechanisms do not set it.
};

/** @addtogroup staged_helpers
 * @{
 */

/** @name Stage 1: permutation and batch primitives
 * Generic helpers use one-based permutation images but zero-based C++ row and
 * column indices. They do not attach tensor meaning to any points.
 * @{
 */
/// Test whether images contain each point 1..n exactly once; empty is valid.
bool isPermutation(const Permutation &permutation);
/** @brief Compose left to right: `result[i] = second[first[i]-1]`.
 * @throws std::invalid_argument Invalid permutations or unequal degrees.
 */
Permutation product(const Permutation &first, const Permutation &second);
/// Invert a valid image list; throws std::invalid_argument on invalid images.
Permutation inverse(const Permutation &permutation);
/// Apply a permutation to a one-based point; checks the permutation and range.
int onPoint(int point, const Permutation &permutation);
/// Rowwise product() of equally shaped valid permutation batches.
PermutationBatch batchProduct(const PermutationBatch &first,
                              const PermutationBatch &second);
/// Rowwise inverse() of a valid batch of common degree.
PermutationBatch batchInverse(const PermutationBatch &permutations);
/// Apply permutation `i` to one-based point `i`; batch sizes must agree.
std::vector<int> batchOnPoints(const std::vector<int> &points,
                               const PermutationBatch &permutations);
/// Rearrange rows by `result[i][j] = values[i][permutations[i][j]-1]`.
IntegerMatrix batchApply(const IntegerMatrix &values,
                         const PermutationBatch &permutations);
/** @brief Find the lexicographic minimum on an ordered list of zero-based columns.
 * @throws std::invalid_argument Empty rows/columns, ragged rows or invalid columns.
 */
MinimumImagesResult minimumImages(const IntegerMatrix &rows,
                                  const std::vector<std::size_t> &columns);
/// Sort whole rows lexicographically and remove exact duplicates.
IntegerMatrix sortUniqueRows(IntegerMatrix rows);
/** @} */

/** @name Stage 2: explicit-transversal search level
 * @{
 */
/// Build the candidate triples `(t*g*d, t*s, sign)` from supplied representatives.
std::vector<SearchConfiguration>
expandConfigurations(const SearchLevelInput &input);
/// Read each configuration's image at the one-based selected point.
std::vector<int>
candidatePointImages(const std::vector<SearchConfiguration> &configurations,
                     int selectedPoint);
/// Keep least selected-point images; deduplicate all fields and retain parent indices.
SearchLevelResult
filterMinimumAndDeduplicate(std::vector<SearchConfiguration> configurations,
                            std::vector<std::size_t> parentIndices,
                            int selectedPoint);
/// Expand explicit transversal candidates, then filter and deduplicate.
SearchLevelResult advanceSearchLevel(const SearchLevelInput &input);
/** @} */

/** @name Stage 3: xPerm-backed BSGS and orbit operations
 * These are shared group-theoretic ingredients of the Butler–Portugal and
 * Niehoff approaches, rather than the propagated-symmetry acceleration.
 * @{
 */
/** @brief Construct a BSGS using xPerm's Schreier–Sims implementation.
 * @param tentativeBase Preferred ordered base prefix.
 * @param generators Nonempty common-degree valid generator list.
 * @throws std::invalid_argument Malformed base or generators.
 */
BSGS makeBSGS(const IntegerRow &tentativeBase,
              const PermutationBatch &generators);
/// Select SGS elements fixing the first prefixLength base points pointwise.
PermutationBatch stabilizerOfBasePrefix(const BSGS &group,
                                        std::size_t prefixLength);
/// Orbit of `base[level]` under the subgroup fixing earlier base points.
BasicOrbit basicOrbit(const BSGS &group, std::size_t level);
/** @brief Trace a slot representative sending orbit.root to point.
 * @pre point belongs to this valid orbit, and degree matches its Schreier data.
 * The representative lies in the subgroup used to build the orbit and hence
 * preserves any earlier fixed base prefix. No orbit-membership check is added.
 */
Permutation traceRepresentative(const BasicOrbit &orbit, int point, int degree);
/// Rebuild a BSGS with selectedSlot first, expand its orbit, retain least images.
GroupSearchLevelResult
advanceGroupSearchLevel(const GroupSearchLevelInput &input);
/** @} */

/** @name Stage 4: persistent stabilizer-chain search
 * @{
 */
/** @brief Construct one chain with selectedSlots as the required base prefix.
 * @pre Generators are nonempty; selected slots are distinct valid points.
 * @throws std::runtime_error Schreier–Sims fails to retain the requested prefix.
 */
StabilizerChainState
initializeStabilizerChain(const IntegerRow &tentativeBase,
                          const PermutationBatch &generators,
                          const IntegerRow &selectedSlots);
/// Construct the current orbit from stored SGS indices, without rebuilding the BSGS.
BasicOrbit currentBasicOrbit(const StabilizerChainState &state);
/// Minimize selected slots in order using only slot actions; no label propagation.
PersistentSearchResult
runPersistentGroupSearch(const PersistentSearchInput &input);
/** @} */

/** @name Stage 5: implicit structured label representatives
 * @{
 */
/// Require disjoint nonempty pools covering 1..degree, with even sizes for dummy pools.
void validateLabelGroups(const std::vector<LabelGroup> &groups, int degree);
/** @brief Find least admissible label representatives fixing a given label set.
 * Pair exchange preserves variance without a metric; an antisymmetric metric
 * assigns sign -1 when the source pair is reversed. Fixing either endpoint
 * freezes the pair. The returned sign is separate from its image list.
 * @pre groups partition all labels; fixedLabels contains distinct valid labels.
 */
std::vector<LabelCandidate>
canonicalizeLabel(int sourceLabel, const IntegerRow &fixedLabels,
                  const std::vector<LabelGroup> &groups, int degree);
/// Combine a persistent slot chain and implicit label actions; no B.1–B.3 pruning.
LabelGroupSearchResult runLabelGroupSearch(const LabelGroupSearchInput &input);
/** @} */

/** @name Stage 6: bounded explicit signed-group equivalence
 * @warning These routines enumerate the generated signed group. They are not
 * the paper's incremental Propagated-Symmetries mechanism and may grow
 * factorially. Production canonicalization does not call them.
 * @{
 */
/// Validate ordinary permutations of the required degree and separate signs ±1.
void validatePropagatedSymmetries(
    const std::vector<SignedPermutation> &generators, int degree);
/** @brief Reduce configurations under a supplied signed group of left actions.
 * Opposite signs for the same orbit representative yield zero. The closure-size
 * limit is a resource guard, not an approximation: exceeding it throws.
 * @throws std::runtime_error The generated signed group exceeds maximumOrbitSize.
 */
PropagatedSymmetryResult
reduceByPropagatedSymmetries(std::vector<SearchConfiguration> configurations,
                             const std::vector<SignedPermutation> &generators,
                             std::size_t maximumOrbitSize = 100000);
/// Run Stage 5, then explicit Stage 6 reduction (not production canonicalization).
PropagatedSymmetryResult
runPropagatedSymmetrySearch(const PropagatedSymmetryInput &input);
/** @} */
/** @} */

/** @addtogroup niehoff_api
 * @{
 */
/** @brief Validate the legacy layout and construct explicit disjoint label pools.
 * Unlisted labels, including both sign points, become singleton fixed groups.
 * @throws std::invalid_argument Malformed permutations, overlapping/out-of-range
 * labels, inconsistent counts, odd dummy lengths or invalid metric codes.
 */
std::vector<LabelGroup>
makeLegacyLabelGroups(const LegacyCanonicalPermInput &input);
/** @brief Canonicalize a signed tensor using Niehoff's accelerated search.
 *
 * Builds or rebases the slot SGS, infers total subsets from actual group
 * membership, then executes incremental Values, least-instance selection,
 * propagation and certified branch pruning. The result is the natural numeric
 * lexicographic minimum in the signed double coset, or zero when both signs
 * occur. A supplied BSGS base does not change the comparison order, so the
 * representative need not match the original xPerm backend byte for byte.
 *
 * @throws std::invalid_argument Invalid input layout.
 * @throws std::runtime_error An internal search invariant cannot be maintained.
 * @note Arbitrary groups can still require factorial search. See
 * @ref niehoff_algorithm, @ref design_choices and @ref validation.
 */
LegacyCanonicalPermResult
canonicalizeLegacyInput(const LegacyCanonicalPermInput &input);
/// Augment the slot group with declared total subsets, then run the same core.
LegacyCanonicalPermResult
canonicalizeExtendedInput(const ExtendedCanonicalPermInput &input);
/** @brief Canonicalize legacy input and optionally collect search work counts.
 * @param input Valid legacy data, as for the one-argument overload.
 * @param statistics Output counters reset on entry; nullptr disables counting.
 * @see canonicalizeLegacyInput(const LegacyCanonicalPermInput &)
 */
LegacyCanonicalPermResult
canonicalizeLegacyInput(const LegacyCanonicalPermInput &input,
                        CanonicalizationStats *statistics);
/** @brief Canonicalize extended input and optionally collect search work counts.
 * @param input Valid legacy data plus disjoint declared slot subsets.
 * @param statistics Output counters reset on entry; nullptr disables counting.
 * @see canonicalizeExtendedInput(const ExtendedCanonicalPermInput &)
 */
LegacyCanonicalPermResult
canonicalizeExtendedInput(const ExtendedCanonicalPermInput &input,
                          CanonicalizationStats *statistics);
/** @} */

} // namespace xperm::niehoff

#endif
