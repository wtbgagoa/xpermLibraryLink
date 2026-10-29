// Compare total-subset inference with independent enumeration of signed groups.
#include "TotalSymmetry.hpp"
#include "xperm.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace n = xperm::niehoff;
using P = n::Permutation;
namespace {
std::size_t checks = 0;
void require(bool condition, const std::string &message) {
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}
P identity(int degree) {
  P result(static_cast<std::size_t>(degree));
  std::iota(result.begin(), result.end(), 1);
  return result;
}
std::set<P> closure(int degree, const n::PermutationBatch &generators) {
  std::set<P> seen{identity(degree)};
  std::vector<P> elements(seen.begin(), seen.end());
  for (std::size_t cursor = 0; cursor < elements.size(); ++cursor)
    for (const auto &generator : generators) {
      P next = elements[cursor];
      for (int &image : next)
        image = generator[image - 1];
      if (seen.insert(next).second) {
        elements.push_back(std::move(next));
        if (elements.size() > 20000)
          throw std::runtime_error("small-group inference oracle exceeded its budget");
      }
    }
  return seen;
}
void compare(int realDegree, const n::PermutationBatch &generators,
             const std::string &caseName) {
  const int degree = realDegree + 2;
  const auto all = closure(degree, generators);
  const auto chain = n::initializeStabilizerChain({}, generators, identity(realDegree));
  const auto actual = n::inferTotalSymmetrySubsets(chain);
  P negativeIdentity = identity(degree);
  std::swap(negativeIdentity[realDegree], negativeIdentity[realDegree + 1]);
  const bool zero = all.count(negativeIdentity) != 0;
  require(actual.zero == zero, "negative-identity inference: " + caseName);
  if (zero)
    return;
  require(actual.subsetOfSlot.size() == static_cast<std::size_t>(realDegree + 1),
          "one-based subset lookup: " + caseName);
  for (int first = 0; first < realDegree; ++first)
    for (int second = first + 1; second < realDegree; ++second) {
      P swap = identity(degree);
      std::swap(swap[first], swap[second]);
      int expected = all.count(swap) ? 1 : 0;
      std::swap(swap[realDegree], swap[realDegree + 1]);
      if (all.count(swap))
        expected = -1;
      const int a = actual.subsetOfSlot[first + 1];
      const int b = actual.subsetOfSlot[second + 1];
      const int inferred = a == b ? (a > 0 ? 1 : (a < 0 ? -1 : 0)) : 0;
      require(inferred == expected, "signed transposition inference: " + caseName +
          ", pair=" + std::to_string(first + 1) + "," + std::to_string(second + 1));
    }
  std::set<int> included;
  for (const auto &subset : actual.subsets) {
    require(subset.slots.size() >= 2 && (subset.sign == 1 || subset.sign == -1),
            "invalid inferred subset: " + caseName);
    const int identifier = actual.subsetOfSlot[subset.slots.front()];
    for (int slot : subset.slots) {
      require(slot >= 1 && slot <= realDegree && included.insert(slot).second,
              "overlapping inferred subsets: " + caseName);
      require(actual.subsetOfSlot[slot] == identifier &&
                  (identifier < 0 ? -1 : 1) == subset.sign,
              "inconsistent inferred lookup: " + caseName);
    }
  }
  for (int slot = 1; slot <= realDegree; ++slot)
    require((included.count(slot) != 0) == (actual.subsetOfSlot[slot] != 0),
            "subset missing from lookup: " + caseName);
}
void hiddenSwaps() {
  // A 4-cycle and a 3-cycle generate S4 but neither input generator is a
  // transposition. This includes the sign character of A_[abcd].
  for (int sign : {-1, 1}) {
    P fourCycle{2, 3, 4, 1, 5, 6};
    if (sign < 0)
      std::swap(fourCycle[4], fourCycle[5]);
    P threeCycle{2, 3, 1, 4, 5, 6};
    compare(4, {fourCycle, threeCycle}, "hidden S4 character " + std::to_string(sign));
  }
  // A4 is transitive, yet has no individual swap and no total-symmetry subset.
  compare(4, {{2, 3, 1, 4, 5, 6}, {1, 3, 4, 2, 5, 6}}, "alternating group A4");
  // Two unsigned pair exchanges do not imply internal pair transpositions.
  compare(6, {{3, 4, 1, 2, 5, 6, 7, 8}, {1, 2, 5, 6, 3, 4, 7, 8}},
          "pure pairwise tensor symmetry");
  // Unequal signs on overlapping transpositions imply -identity as a product.
  compare(3, {{2, 1, 3, 4, 5}, {1, 3, 2, 5, 4}}, "hidden negative identity");
  // A total subset need not occupy consecutive slot positions.
  compare(6, {{3, 2, 1, 4, 5, 6, 8, 7}, {1, 2, 5, 4, 3, 6, 8, 7}},
          "interleaved negative subset");
}
void randomizedGroups() {
  std::mt19937 random(76543);
  for (int size = 2; size <= 7; ++size)
    for (int trial = 0; trial < 100; ++trial) {
      n::PermutationBatch generators;
      const int count = 1 + static_cast<int>(random() % 4);
      for (int j = 0; j < count; ++j) {
        P generator = identity(size + 2);
        std::shuffle(generator.begin(), generator.begin() + size, random);
        if (random() % 2)
          std::swap(generator[size], generator[size + 1]);
        generators.push_back(std::move(generator));
      }
      compare(size, generators, "random degree " + std::to_string(size) +
                                    ", trial " + std::to_string(trial));
    }
}
void manyInterchangeablePairs(int factors) {
  // This SGS has a single point orbit, but factors separate total subsets.
  // Use its known complete SGS so this check measures inference rather than
  // asking Schreier-Sims to rebuild a large wreath-product group from scratch.
  const int realDegree = 2 * factors;
  n::StabilizerChainState chain;
  chain.degree = realDegree + 2;
  chain.base = identity(realDegree);
  for (int j = 0; j < factors; ++j) {
    P generator = identity(chain.degree);
    std::swap(generator[2 * j], generator[2 * j + 1]);
    std::swap(generator[realDegree], generator[realDegree + 1]);
    chain.strongGenerators.push_back(std::move(generator));
  }
  for (int j = 0; j + 1 < factors; ++j) {
    P generator = identity(chain.degree);
    std::swap(generator[2 * j], generator[2 * j + 2]);
    std::swap(generator[2 * j + 1], generator[2 * j + 3]);
    chain.strongGenerators.push_back(std::move(generator));
  }
  n::IntegerRow flat;
  for (const auto &generator : chain.strongGenerators)
    flat.insert(flat.end(), generator.begin(), generator.end());
  StabilizerChain positions;
  ::stab_chain(chain.base.data(), realDegree, flat.data(),
               static_cast<int>(chain.strongGenerators.size()), chain.degree, positions);
  chain.generatorPositions.assign(positions.begin(), positions.end());
  const auto start = std::chrono::steady_clock::now();
  const auto inferred = n::inferTotalSymmetrySubsets(chain);
  const auto end = std::chrono::steady_clock::now();
  require(!inferred.zero && inferred.subsets.size() == static_cast<std::size_t>(factors),
          "interchangeable factors must retain independent antisymmetric subsets");
  for (int j = 0; j < factors; ++j) {
    const int entry = inferred.subsetOfSlot[2 * j + 1];
    require(entry < 0 && entry == inferred.subsetOfSlot[2 * j + 2],
            "internal pair must be one negative subset");
    if (j)
      require(entry != inferred.subsetOfSlot[2 * j], "adjacent factors must not merge");
  }
  std::cout << "inference factors=" << factors << " subsets=" << inferred.subsets.size()
            << " seconds=" << std::chrono::duration<double>(end - start).count() << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    hiddenSwaps();
    randomizedGroups();
    manyInterchangeablePairs(125);
    if (argc > 1 && std::string(argv[1]) == "--benchmark") {
      manyInterchangeablePairs(250);
      manyInterchangeablePairs(500);
    }
    std::cout << "Total-subset inference: " << checks << " checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << "After " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
