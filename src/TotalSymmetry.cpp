#include "TotalSymmetry.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace xperm::niehoff {
namespace {

class SignedComponents {
public:
  explicit SignedComponents(int size) : parent(size), rank(size), signs(size) {
    std::iota(parent.begin(), parent.end(), 0);
  }

  int find(int point) {
    if (parent[point] != point)
      parent[point] = find(parent[point]);
    return parent[point];
  }

  int knownSign(int first, int second) {
    const int root = find(first);
    return root == find(second) ? signs[root] : 0;
  }

  // All transpositions in a connected total-symmetry subset are conjugate.
  // Consequently their signs must agree, or the signed group contains -id.
  bool join(int first, int second, int sign) {
    int left = find(first), right = find(second);
    if ((signs[left] && signs[left] != sign) ||
        (signs[right] && signs[right] != sign))
      return false;
    if (left != right) {
      if (rank[left] < rank[right])
        std::swap(left, right);
      parent[right] = left;
      if (rank[left] == rank[right])
        ++rank[left];
    }
    signs[left] = sign;
    return true;
  }

private:
  IntegerRow parent, rank, signs;
};

// Membership sifting needs Schreier trees, not an enumerated group or a full
// permutation for every transversal. Build each tree only if a query uses it.
class TranspositionMembership {
public:
  explicit TranspositionMembership(const StabilizerChainState &group)
      : group(group), trees(group.base.size()), inverseGenerators(
            group.strongGenerators.size(), Permutation(group.degree)) {
    for (std::size_t index = 0; index < group.strongGenerators.size(); ++index)
      for (int point = 0; point < group.degree; ++point)
        inverseGenerators[index][group.strongGenerators[index][point] - 1] =
            point + 1;
  }

  // Return the unique allowed sign, or zero if neither signed transposition
  // belongs to the group. The caller has already excluded -identity.
  int sign(int first, int second) {
    Permutation residue(group.degree);
    std::iota(residue.begin(), residue.end(), 1);
    std::swap(residue[first], residue[second]);
    for (std::size_t level = 0; level < group.base.size(); ++level) {
      const int root = group.base[level] - 1;
      // The sign is determined after sifting the action on all real slots.
      if (root >= group.degree - 2)
        continue;
      int point = residue[root] - 1;
      if (point == root)
        continue;
      const Tree &tree = getTree(level);
      if (tree.parent[point] == -1)
        return 0;
      // Following the tree backwards multiplies by the inverse transversal.
      while (point != root) {
        const auto &inverse = inverseGenerators[tree.generator[point]];
        for (int &image : residue)
          image = inverse[image - 1];
        point = tree.parent[point];
      }
    }
    for (int point = 0; point < group.degree - 2; ++point)
      if (residue[point] != point + 1)
        return 0;
    return residue[group.degree - 2] == group.degree - 1 ? 1 : -1;
  }

private:
  struct Tree {
    IntegerRow parent;
    IntegerRow generator;
  };
  const StabilizerChainState &group;
  std::vector<Tree> trees;
  PermutationBatch inverseGenerators;

  const Tree &getTree(std::size_t level) {
    Tree &tree = trees[level];
    if (!tree.parent.empty())
      return tree;
    tree.parent.assign(group.degree, -1);
    tree.generator.assign(group.degree, -1);
    const int root = group.base[level] - 1;
    tree.parent[root] = root;
    IntegerRow orbit{root};
    for (std::size_t cursor = 0; cursor < orbit.size(); ++cursor) {
      const int point = orbit[cursor];
      for (const int index : group.generatorPositions[level]) {
        const int image = group.strongGenerators[index][point] - 1;
        if (tree.parent[image] == -1) {
          tree.parent[image] = point;
          tree.generator[image] = index;
          orbit.push_back(image);
        }
      }
    }
    return tree;
  }
};

} // namespace

TotalSymmetryAnalysis
inferTotalSymmetrySubsets(const StabilizerChainState &chain) {
  if (chain.degree < 2 || chain.generatorPositions.size() < chain.base.size())
    throw std::invalid_argument("invalid signed stabilizer chain");
  const int size = chain.degree - 2;
  bool signPointSeen = false;
  for (const int point : chain.base) {
    if (point < 1 || point > chain.degree || (signPointSeen && point <= size))
      throw std::invalid_argument("real slots must precede sign points in base");
    signPointSeen = signPointSeen || point > size;
  }
  TotalSymmetryAnalysis result;
  result.subsetOfSlot.assign(static_cast<std::size_t>(size) + 1, 0);
  SignedComponents components(size);

  // Direct generators supply the usual tensor symmetries without sifting.
  for (const auto &generator : chain.strongGenerators) {
    int first = -1, second = -1, moved = 0;
    for (int point = 0; point < size; ++point) {
      if (generator[point] != point + 1) {
        if (moved == 0)
          first = point;
        else if (moved == 1)
          second = point;
        if (++moved > 2)
          break;
      }
    }
    const int sign = generator[size] == size + 1 ? 1 : -1;
    if ((moved == 0 && sign < 0) ||
        (moved == 2 && !components.join(first, second, sign))) {
      result.zero = true;
      return result;
    }
  }
  if (size < 2)
    return result;

  SignedComponents pointOrbits(size);
  for (const auto &generator : chain.strongGenerators)
    for (int point = 0; point < size; ++point)
      pointOrbits.join(point, generator[point] - 1, 1);

  // Conjugation preserves membership and sign of a transposition. Testing one
  // representative per orbit on unordered pairs makes inference independent
  // of the particular SGS: its generators can hide swaps in longer products.
  // For a product of interchangeable rank-two tensors there are only two pair
  // orbits, although the ordinary point orbit includes every tensor slot.
  const auto pairIndex = [size](int first, int second) {
    if (second < first)
      std::swap(first, second);
    const std::size_t a = static_cast<std::size_t>(first);
    return a * (2 * static_cast<std::size_t>(size) - a - 1) / 2 +
           static_cast<std::size_t>(second - first - 1);
  };
  std::vector<unsigned char> visited(
      static_cast<std::size_t>(size) * (size - 1) / 2, 0);
  TranspositionMembership membership(chain);
  std::vector<std::pair<int, int>> orbit;
  for (int first = 0; first < size; ++first) {
    for (int second = first + 1; second < size; ++second) {
      // A transposition cannot connect two distinct point orbits.
      if (pointOrbits.find(first) != pointOrbits.find(second))
        continue;
      if (visited[pairIndex(first, second)])
        continue;
      int sign = components.knownSign(first, second);
      if (sign == 0)
        sign = membership.sign(first, second);
      orbit.clear();
      orbit.emplace_back(first, second);
      visited[pairIndex(first, second)] = 1;
      for (std::size_t cursor = 0; cursor < orbit.size(); ++cursor) {
        const auto [left, right] = orbit[cursor];
        if (sign && !components.join(left, right, sign)) {
          result.zero = true;
          return result;
        }
        for (const auto &generator : chain.strongGenerators) {
          const int imageLeft = generator[left] - 1;
          const int imageRight = generator[right] - 1;
          const auto index = pairIndex(imageLeft, imageRight);
          if (!visited[index]) {
            visited[index] = 1;
            orbit.emplace_back(imageLeft, imageRight);
          }
        }
      }
    }
  }

  IntegerRow subsetIndex(size, -1);
  for (int point = 0; point < size; ++point) {
    const int root = components.find(point);
    const int sign = components.knownSign(root, point);
    if (sign == 0)
      continue;
    if (subsetIndex[root] == -1) {
      subsetIndex[root] = static_cast<int>(result.subsets.size());
      result.subsets.push_back({{}, sign});
    }
    const int index = subsetIndex[root];
    result.subsets[index].slots.push_back(point + 1);
    result.subsetOfSlot[point + 1] = sign * (index + 1);
  }
  return result;
}

} // namespace xperm::niehoff
