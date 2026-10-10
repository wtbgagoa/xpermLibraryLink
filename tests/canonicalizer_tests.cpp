#include "NiehoffCanonicalizer.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace n = xperm::niehoff;
using P = n::Permutation;
using Input = n::LegacyCanonicalPermInput;

namespace {
int checks = 0;
int failures = 0;

void require(bool condition, const std::string &message) {
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}

template <typename F> void invalid(F action, const std::string &message) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument &) { rejected = true; }
  require(rejected, message);
}

P identity(int degree) {
  P result(static_cast<std::size_t>(degree));
  std::iota(result.begin(), result.end(), 1);
  return result;
}

// Deliberately independent of the implementation and its xPerm helpers.
P compose(const P &first, const P &second) {
  P result(first.size());
  for (std::size_t i = 0; i < result.size(); ++i)
    result[i] = second[static_cast<std::size_t>(first[i] - 1)];
  return result;
}

P swap(int degree, int first, int second, int sign = 1) {
  P result = identity(degree);
  std::swap(result[first - 1], result[second - 1]);
  if (sign < 0)
    std::swap(result[degree - 2], result[degree - 1]);
  return result;
}

std::vector<P> closure(int degree, const std::vector<P> &generators) {
  std::set<P> seen{identity(degree)};
  std::vector<P> result(seen.begin(), seen.end());
  for (std::size_t cursor = 0; cursor < result.size(); ++cursor) {
    const P element = result[cursor];
    for (const auto &generator : generators) {
      P next = compose(element, generator);
      if (seen.insert(next).second)
        result.push_back(std::move(next));
    }
  }
  return result;
}

std::vector<P> labelGenerators(const Input &input) {
  std::vector<P> generators;
  std::size_t offset = 0;
  for (std::size_t set = 0; set < input.dummySetLengths.size(); ++set) {
    const int length = input.dummySetLengths[set];
    for (int j = 0; j < length; j += 2) {
      const int first = input.dummyLabels[offset + j];
      const int second = input.dummyLabels[offset + j + 1];
      if (input.metricSymmetries[set] != 0)
        generators.push_back(swap(input.degree, first, second,
                                  input.metricSymmetries[set]));
      if (j > 0) {
        P exchange = identity(input.degree);
        std::swap(exchange[input.dummyLabels[offset + j - 2] - 1],
                  exchange[first - 1]);
        std::swap(exchange[input.dummyLabels[offset + j - 1] - 1],
                  exchange[second - 1]);
        generators.push_back(std::move(exchange));
      }
    }
    offset += static_cast<std::size_t>(length);
  }
  offset = 0;
  for (const int length : input.repeatedSetLengths) {
    for (int j = 1; j < length; ++j)
      generators.push_back(swap(input.degree, input.repeatedLabels[offset + j - 1],
                                 input.repeatedLabels[offset + j]));
    offset += static_cast<std::size_t>(length);
  }
  return generators;
}

// A total subset contributes all its adjacent transpositions to S. Generating
// the entire two groups and every s*g*d gives a small-degree reference without
// sharing normalization, stabilizer chains, pruning, or zero detection code.
n::LegacyCanonicalPermResult oracle(const Input &input,
                                    const std::vector<n::TotalSymmetrySubset> &subsets) {
  auto slotGenerators = input.slotGenerators;
  for (const auto &subset : subsets)
    for (std::size_t j = 1; j < subset.slots.size(); ++j)
      slotGenerators.push_back(swap(input.degree, subset.slots[j - 1],
                                    subset.slots[j], subset.sign));
  const auto slots = closure(input.degree, slotGenerators);
  const auto labels = closure(input.degree, labelGenerators(input));
  std::map<P, int> signs;
  P minimum;
  for (const auto &slot : slots) {
    const P left = compose(slot, input.permutation);
    for (const auto &label : labels) {
      P candidate = compose(left, label);
      const int sign = candidate[input.degree - 2] == input.degree - 1 ? 1 : -1;
      P key(candidate.begin(), candidate.end() - 2);
      const auto inserted = signs.emplace(std::move(key), sign);
      if (!inserted.second && inserted.first->second != sign)
        return {true, {}};
      if (minimum.empty() || candidate < minimum)
        minimum = std::move(candidate);
    }
  }
  return {false, minimum};
}

std::string list(const P &values) {
  std::ostringstream out;
  out << '{';
  for (std::size_t j = 0; j < values.size(); ++j)
    out << (j ? "," : "") << values[j];
  out << '}';
  return out.str();
}

void compare(const Input &input, const std::vector<n::TotalSymmetrySubset> &subsets = {}) {
  if (std::getenv("XPERM_TEST_TRACE")) {
    std::cerr << "Case " << checks << " permutation=" << list(input.permutation)
              << " base=" << list(input.base) << " strong=" << input.slotGeneratorsAreStrong
              << " generators=" << input.slotGenerators.size() << " subsets=";
    for (const auto &subset : subsets)
      std::cerr << list(subset.slots) << ':' << subset.sign;
    std::cerr << '\n';
  }
  const auto expected = oracle(input, subsets);
  const auto actual = subsets.empty()
      ? n::canonicalizeLegacyInput(input)
      : n::canonicalizeExtendedInput({input, subsets});
  ++checks;
  if (expected.zero == actual.zero && (expected.zero || expected.permutation == actual.permutation))
    return;
  ++failures;
  if (failures <= 12) {
    std::cerr << "Oracle mismatch: permutation=" << list(input.permutation)
              << " base=" << list(input.base) << " strong=" << input.slotGeneratorsAreStrong
              << " free=" << list(input.freeLabels)
              << " dummyLengths=" << list(input.dummySetLengths)
              << " dummies=" << list(input.dummyLabels)
              << " metric=" << list(input.metricSymmetries)
              << " repeatedLengths=" << list(input.repeatedSetLengths)
              << " repeated=" << list(input.repeatedLabels) << " generators=";
    for (const auto &generator : input.slotGenerators)
      std::cerr << list(generator);
    std::cerr << " subsets=";
    for (const auto &subset : subsets)
      std::cerr << list(subset.slots) << ':' << subset.sign;
    std::cerr << " expected=" << (expected.zero ? "zero" : list(expected.permutation))
              << " actual=" << (actual.zero ? "zero" : list(actual.permutation)) << '\n';
  }
}

Input freeInput(int realDegree) {
  Input input;
  input.degree = realDegree + 2;
  input.permutation = identity(input.degree);
  input.freeLabels = identity(realDegree);
  return input;
}

std::vector<Input> labelCases(int degree) {
  std::vector<Input> result{freeInput(degree)};
  Input repeated = freeInput(degree);
  repeated.freeLabels.clear();
  repeated.repeatedSetLengths = {degree};
  repeated.repeatedLabels = identity(degree);
  result.push_back(repeated);
  std::reverse(repeated.repeatedLabels.begin(), repeated.repeatedLabels.end());
  result.push_back(repeated);
  if (degree >= 2) {
    for (int metric : {-1, 0, 1}) {
      Input dummy = freeInput(degree);
      const int paired = degree - degree % 2;
      dummy.freeLabels.erase(dummy.freeLabels.begin(), dummy.freeLabels.begin() + paired);
      dummy.dummySetLengths = {paired};
      dummy.dummyLabels = identity(paired);
      dummy.metricSymmetries = {metric};
      result.push_back(dummy);
      std::reverse(dummy.dummyLabels.begin(), dummy.dummyLabels.end());
      result.push_back(dummy);
    }
  }
  if (degree >= 4) {
    Input mixed = freeInput(degree);
    mixed.freeLabels.erase(mixed.freeLabels.begin(), mixed.freeLabels.begin() + 4);
    mixed.dummySetLengths = {2};
    mixed.dummyLabels = {3, 1};
    mixed.metricSymmetries = {-1};
    mixed.repeatedSetLengths = {2};
    mixed.repeatedLabels = {4, 2};
    result.push_back(mixed);
    mixed.repeatedSetLengths.clear();
    mixed.repeatedLabels.clear();
    mixed.dummySetLengths = {2, 2};
    mixed.dummyLabels = {3, 1, 4, 2};
    mixed.metricSymmetries = {-1, 1};
    result.push_back(mixed);
  }
  return result;
}

void standaloneHelpers() {
  require(n::product({2, 3, 1}, {1, 3, 2}) == P({3, 2, 1}), "product convention");
  require(n::inverse({2, 3, 1}) == P({3, 1, 2}), "inverse");
  require(n::batchApply({{10, 20, 30}}, {{3, 1, 2}}) == n::IntegerMatrix{{30, 10, 20}}, "batch apply");
  require(n::batchProduct({}, {}).empty(), "empty batch product");
  require(!n::isPermutation({1, 1}), "duplicate permutation entry");
  invalid([] { n::product({1, 2}, {1}); }, "mismatched product sizes");
  invalid([] { n::batchOnPoints({0}, {{1, 2}}); }, "zero point rejected");
  invalid([] { n::batchApply({{1}}, {{1, 2}}); }, "mismatched matrix shape");
  const auto minimum = n::minimumImages({{9, 2, 1}, {8, 1, 2}, {7, 2, 1}}, {2, 1});
  require(minimum.minimum == P({1, 2}) && minimum.survivorIndices == std::vector<std::size_t>({0, 2}), "minimum images preserves ties");
  require(n::sortUniqueRows({{2}, {1}, {2}}) == n::IntegerMatrix({{1}, {2}}), "unique row sort");
  invalid([] { n::minimumImages({{1}}, {1}); }, "column boundary checked");
  invalid([] { n::validateLabelGroups({{n::LabelGroupType::Repeated, {1, 1}}}, 2); }, "duplicate label rejected");
  invalid([] { n::validateLabelGroups({{n::LabelGroupType::Dummy, {1}}}, 1); }, "unpaired label rejected");
}

void stagedSearchRegressions() {
  n::SearchConfiguration configuration{identity(3), {3, 1, 2}, 1, {42}, {}};
  n::SearchLevelInput explicitLevel;
  explicitLevel.configurations = {configuration};
  explicitLevel.slotTransversals = {{2, 1, 3}};
  explicitLevel.labelTransversals = {{1, 3, 2}};
  explicitLevel.parentIndices = {0};
  const auto expanded = n::expandConfigurations(explicitLevel);
  require(expanded.size() == 1 && expanded[0].labelPermutation == P({1, 2, 3}),
          "stage 2 must apply both slot and label actions to configuration");
  require(expanded[0].slotPermutation == P({2, 1, 3}) && expanded[0].metadata == P({42}),
          "stage 2 preserves provenance");
  auto other = configuration;
  other.fixedLabels = {1};
  const auto deduplicated = n::filterMinimumAndDeduplicate(
      {configuration, other, configuration}, {0, 1, 2}, 1);
  require(deduplicated.configurations.size() == 2, "different fixed-label sets remain distinct");
  invalid([&] {
    explicitLevel.parentIndices = {1};
    n::expandConfigurations(explicitLevel);
  }, "stage 2 rejects invalid parent");

  const auto single = n::advanceGroupSearchLevel({{configuration}, {}, {{2, 1, 3}}, 1});
  require(single.minimumImage == 1 && single.configurations.size() == 1 &&
          single.configurations[0].labelPermutation == P({1, 3, 2}),
          "stage 3 must apply slot representative to configuration");
  const auto persistent = n::runPersistentGroupSearch(
      {{configuration}, {}, {{2, 1, 3}, {1, 3, 2}}, {1, 2, 3}});
  require(persistent.configurations.size() == 1 &&
          persistent.configurations[0].labelPermutation == identity(3),
          "stage 4 must retain minimized images in later levels");
  require(persistent.levels.size() == 3 && persistent.levels[0].minimumImage == 1 &&
          persistent.levels[1].minimumImage == 2 && persistent.levels[2].minimumImage == 3,
          "stage 4 minimum image summaries");
  n::LabelGroupSearchInput labelSearch;
  labelSearch.configurations = {configuration};
  labelSearch.slotGenerators = {{2, 1, 3}, {1, 3, 2}};
  labelSearch.selectedSlots = {1, 2, 3};
  labelSearch.labelGroups = {{n::LabelGroupType::Free, {1, 2, 3}}};
  const auto labeled = n::runLabelGroupSearch(labelSearch);
  require(labeled.configurations.size() == 1 &&
          labeled.configurations[0].labelPermutation == identity(3),
          "stage 5 free-label search must apply slot action");

  const std::vector<n::LabelGroup> reversedPairs{
      {n::LabelGroupType::AntisymmetricMetric, {2, 1, 4, 3}}};
  const auto candidates = n::canonicalizeLabel(1, {}, reversedPairs, 4);
  require(candidates.size() == 1 && candidates[0].canonicalLabel == 1 &&
          candidates[0].sign == 1 && candidates[0].representative == identity(4),
          "stage 5 metric label minimum is independent of pair orientation");
}

void bsgsRegressions() {
  for (const std::vector<P> generators :
       {std::vector<P>{identity(3)}, std::vector<P>{identity(3), identity(3), identity(3)}}) {
    const auto group = n::makeBSGS({}, generators);
    require(std::all_of(group.base.begin(), group.base.end(), [](int point) {
      return point >= 1 && point <= 3;
    }), "identity generators must not append zero to the base");
    require(closure(3, group.strongGenerators) == std::vector<P>{identity(3)},
            "identity generators preserve the trivial group");
  }
  const std::vector<P> generators{
      {2, 3, 4, 1, 5, 6}, swap(6, 1, 2), {1, 2, 3, 4, 6, 5}};
  auto allElements = closure(6, generators);
  auto redundant = allElements;
  redundant.push_back(swap(6, 3, 2, -1));
  redundant.push_back(swap(6, 2, 1, -1));
  const auto group = n::makeBSGS({1, 2, 3, 4}, redundant);
  auto generated = closure(6, group.strongGenerators);
  std::sort(generated.begin(), generated.end());
  std::sort(allElements.begin(), allElements.end());
  require(generated == allElements && generated.size() == 48,
          "redundant signed S4 generators retain the group without hanging");
  require(std::all_of(group.base.begin(), group.base.end(), [](int point) {
    return point >= 1 && point <= 6;
  }), "signed group base points stay within degree");
  const auto orbit = n::basicOrbit(group, 0);
  require(orbit.points.size() == 4, "signed S4 first orbit has four real slots");
  for (int point : orbit.points) {
    const auto representative = n::traceRepresentative(orbit, point, 6);
    require(representative[orbit.root - 1] == point &&
            std::binary_search(allElements.begin(), allElements.end(), representative),
            "Schreier representative has requested image and belongs to group");
  }
  const auto stabilizer = closure(6, n::stabilizerOfBasePrefix(group, 1));
  require(stabilizer.size() == 12 && std::all_of(stabilizer.begin(), stabilizer.end(),
      [](const P &element) { return element[0] == 1; }),
      "first base stabilizer fixes its point and has expected order");
}

void inputValidationRegressions() {
  auto checkInvalid = [](const Input &input, const std::string &reason) {
    invalid([&] { n::canonicalizeLegacyInput(input); }, reason);
  };
  Input input = freeInput(2);
  input.permutation = {3, 2, 1, 4};
  checkInvalid(input, "sign point cannot occur in a real slot");
  input = freeInput(2);
  input.slotGenerators = {{1, 3, 2, 4}};
  checkInvalid(input, "slot generator cannot mix sign and real points");
  input = freeInput(2);
  input.base = {1, 1};
  checkInvalid(input, "duplicate base rejected");
  input.base = {0};
  checkInvalid(input, "base zero rejected");
  input.base = {5};
  checkInvalid(input, "base beyond degree rejected");
  input = freeInput(2);
  input.freeLabels = {1, 3};
  checkInvalid(input, "sign points cannot be free labels");
  input = freeInput(2);
  input.freeLabels.clear();
  input.dummyLabels = {1, 2};
  input.dummySetLengths = {4, -2};
  input.metricSymmetries = {1, 1};
  checkInvalid(input, "negative length cannot cancel oversized length");
  input.dummySetLengths = {2147483646, 2147483646, 6};
  input.metricSymmetries = {1, 1, 1};
  checkInvalid(input, "length accumulation cannot wrap around");
  input.dummySetLengths = {2};
  input.metricSymmetries = {2};
  checkInvalid(input, "invalid metric symmetry rejected");
  input = freeInput(2);
  input.freeLabels.clear();
  input.repeatedSetLengths = {3, -1};
  input.repeatedLabels = {1, 2};
  checkInvalid(input, "negative repeated-set length rejected");
  invalid([] { n::makeBSGS({0}, {{1, 2}}); }, "BSGS invalid base rejected");
  invalid([] { n::makeBSGS({1, 1}, {{1, 2}}); }, "BSGS duplicate base rejected");
  invalid([] { n::initializeStabilizerChain({}, {{1, 2}}, {1, 1}); },
          "duplicate selected slots rejected");
  invalid([] { n::canonicalizeExtendedInput({freeInput(2), {{{1, 3}, 1}}}); },
          "subset containing sign point rejected");
  invalid([] { n::canonicalizeExtendedInput({freeInput(2), {{{1, 2}, 0}}}); },
          "invalid subset sign rejected");
  invalid([] { n::canonicalizeExtendedInput({freeInput(3), {{{1, 2}, 1}, {{2, 3}, 1}}}); },
          "overlapping subset declarations rejected");
}

void labelCandidateOracleCases() {
  for (int realDegree = 1; realDegree <= 4; ++realDegree) {
    for (const auto &input : labelCases(realDegree)) {
      const auto groups = n::makeLegacyLabelGroups(input);
      auto labelGroup = closure(input.degree, labelGenerators(input));
      std::sort(labelGroup.begin(), labelGroup.end());
      for (unsigned mask = 0; mask < (1U << realDegree); ++mask) {
        P fixed;
        for (int label = 1; label <= realDegree; ++label)
          if (mask & (1U << (label - 1)))
            fixed.push_back(label);
        for (int source = 1; source <= realDegree; ++source) {
          int minimum = source;
          for (const auto &element : labelGroup) {
            if (std::all_of(fixed.begin(), fixed.end(), [&](int label) {
                  return element[label - 1] == label;
                }))
              minimum = std::min(minimum, element[source - 1]);
          }
          const auto candidates = n::canonicalizeLabel(source, fixed, groups, input.degree);
          const std::string context = " (source=" + std::to_string(source) +
              ", fixed=" + list(fixed) + ", dummies=" + list(input.dummyLabels) +
              ", metric=" + list(input.metricSymmetries) + ")";
          require(!candidates.empty(), "label oracle: a candidate always exists" + context);
          for (const auto &candidate : candidates) {
            require(candidate.canonicalLabel == minimum &&
                    candidate.representative[source - 1] == minimum,
                    "label oracle: minimum in fixed-label stabilizer" + context);
            require(std::all_of(fixed.begin(), fixed.end(), [&](int label) {
                      return candidate.representative[label - 1] == label;
                    }), "label oracle: representative preserves fixed labels" + context);
            P signedRepresentative = candidate.representative;
            if (candidate.sign < 0)
              std::swap(signedRepresentative[input.degree - 2],
                        signedRepresentative[input.degree - 1]);
            require(std::binary_search(labelGroup.begin(), labelGroup.end(), signedRepresentative),
                    "label oracle: representative and sign belong to declared group" + context);
          }
        }
      }
    }
  }
}

void exhaustiveCases() {
  for (int realDegree = 1; realDegree <= 4; ++realDegree) {
    const int degree = realDegree + 2;
    std::vector<std::vector<P>> groups{{}};
    if (realDegree >= 2) {
      groups.push_back({swap(degree, 1, 2)});
      groups.push_back({swap(degree, 1, 2, -1)});
      P cycle = identity(degree);
      std::rotate(cycle.begin(), cycle.begin() + 1, cycle.begin() + realDegree);
      groups.push_back({cycle});
      groups.push_back({cycle, swap(degree, 1, 2)});
    }
    for (auto input : labelCases(realDegree)) {
      P arrangement = identity(realDegree);
      do {
        for (const auto &generators : groups) {
          input.permutation = identity(degree);
          std::copy(arrangement.begin(), arrangement.end(), input.permutation.begin());
          input.slotGenerators = generators;
          compare(input);
          std::swap(input.permutation[degree - 2], input.permutation[degree - 1]);
          compare(input);
        }
      } while (std::next_permutation(arrangement.begin(), arrangement.end()));
    }
  }
}

void deterministicRandomCases() {
  std::mt19937 random(0x4e494548U);
  for (int iteration = 0; iteration < 300; ++iteration) {
    const int realDegree = 2 + static_cast<int>(random() % 4);
    auto cases = labelCases(realDegree);
    Input input = cases[random() % cases.size()];
    std::shuffle(input.permutation.begin(), input.permutation.begin() + realDegree, random);
    if (random() % 2)
      std::swap(input.permutation[input.degree - 2], input.permutation[input.degree - 1]);
    input.base = identity(realDegree);
    std::shuffle(input.base.begin(), input.base.end(), random);
    input.base.resize(random() % (realDegree + 1));
    for (unsigned j = 0, count = random() % 3; j < count; ++j) {
      P generator = identity(input.degree);
      std::shuffle(generator.begin(), generator.begin() + realDegree, random);
      if (random() % 2)
        std::swap(generator[input.degree - 2], generator[input.degree - 1]);
      input.slotGenerators.push_back(std::move(generator));
    }
    if (iteration % 4 == 0) {
      // The whole group is certainly a strong generating set for a full base.
      input.slotGenerators = closure(input.degree, input.slotGenerators);
      input.slotGeneratorsAreStrong = true;
      input.base = identity(input.degree);
      std::shuffle(input.base.begin(), input.base.end(), random);
    }
    compare(input);
    if (iteration % 3 == 0) {
      P subset = identity(realDegree);
      std::shuffle(subset.begin(), subset.end(), random);
      subset.resize(2 + random() % (realDegree - 1));
      compare(input, {{subset, random() % 2 ? 1 : -1}});
    }
  }
}

void regressions() {
  Input scalar = freeInput(0);
  scalar.slotGeneratorsAreStrong = true;
  compare(scalar);
  scalar.permutation = {2, 1};
  compare(scalar);
  scalar.slotGenerators = {{2, 1}};
  compare(scalar);
  Input negativeIdentity = freeInput(1);
  P sign = identity(negativeIdentity.degree);
  std::swap(sign[1], sign[2]);
  negativeIdentity.slotGenerators = {sign};
  compare(negativeIdentity);

  Input reversedBase = freeInput(3);
  reversedBase.base = {3, 1, 2};
  reversedBase.permutation = {2, 3, 1, 4, 5};
  reversedBase.slotGenerators = {{2, 3, 1, 4, 5}};
  compare(reversedBase);

  Input combined = freeInput(4);
  combined.freeLabels.clear();
  combined.dummySetLengths = {4};
  combined.dummyLabels = {1, 2, 3, 4};
  combined.metricSymmetries = {1};
  combined.permutation = {1, 3, 4, 2, 5, 6};
  compare(combined, {{{1, 2, 3}, 1}});
  compare(combined, {{{3, 1, 2}, -1}});
  compare(combined, {{{1, 2}, 1}, {{3, 4}, -1}});

  // A pair exchange and a slot transposition generate extra slot motions;
  // subgroup hints must participate in the group, even when not normal in S.
  Input nonnormal = freeInput(4);
  nonnormal.permutation = {4, 3, 2, 1, 5, 6};
  nonnormal.slotGenerators = {{3, 4, 1, 2, 5, 6}};
  compare(nonnormal, {{{1, 2}, 1}});
}
} // namespace

int main() {
  try {
    standaloneHelpers();
    stagedSearchRegressions();
    bsgsRegressions();
    inputValidationRegressions();
    labelCandidateOracleCases();
    regressions();
    exhaustiveCases();
    deterministicRandomCases();
    std::cout << checks << " checks, " << failures << " oracle mismatches\n";
    return failures ? 1 : 0;
  } catch (const std::exception &exception) {
    std::cerr << "Test aborted after " << checks << " checks: " << exception.what() << '\n';
    return 1;
  }
}
