/*********************************************************************
*********************************************************************
*********************************************************************
*
*   Improved CanonicalPerm based on the algoritm from Ben Niehoff.
*   C++ code translated from
*   https://github.com/bniehoff/tensor-canonicalizer/tree/master
*   by ChatGPT 5.6 Sol under prompting by Thomas Bäckdahl October 2026.
*
*********************************************************************
*********************************************************************
*********************************************************************/

#include "NiehoffCanonicalizer.hpp"
#include "xperm.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
namespace xperm::niehoff {
namespace {
void validateBatch(const PermutationBatch &p) {
  const auto n = p.empty() ? 0 : p.front().size();
  for (const auto &x : p)
    if (x.size() != n || !isPermutation(x))
      throw std::invalid_argument("invalid permutation batch");
}
void sameShape(const IntegerMatrix &a, const IntegerMatrix &b) {
  if (a.size() != b.size())
    throw std::invalid_argument("row counts differ");
  const auto n = a.empty() ? 0 : a.front().size();
  for (const auto &r : a)
    if (r.size() != n)
      throw std::invalid_argument("matrix not rectangular");
  for (const auto &r : b)
    if (r.size() != n)
      throw std::invalid_argument("matrix shapes differ");
}
} // namespace
bool isPermutation(const Permutation &p) {
  std::vector<bool> seen(p.size());
  for (int x : p) {
    if (x < 1 || static_cast<std::size_t>(x) > p.size() || seen[x - 1])
      return false;
    seen[x - 1] = true;
  }
  return true;
}
Permutation product(const Permutation &a, const Permutation &b) {
  if (a.size() != b.size() || !isPermutation(a) || !isPermutation(b))
    throw std::invalid_argument("invalid product operands");
  Permutation r(a.size());
  ::product(const_cast<int *>(a.data()), const_cast<int *>(b.data()), r.data(),
            static_cast<int>(r.size()));
  return r;
}
Permutation inverse(const Permutation &p) {
  if (!isPermutation(p))
    throw std::invalid_argument("invalid permutation");
  Permutation r(p.size());
  ::inverse(const_cast<int *>(p.data()), r.data(), static_cast<int>(r.size()));
  return r;
}
int onPoint(int point, const Permutation &p) {
  if (!isPermutation(p) || point < 1 ||
      static_cast<std::size_t>(point) > p.size())
    throw std::invalid_argument("invalid point");
  return ::onpoints(point, const_cast<int *>(p.data()),
                    static_cast<int>(p.size()));
}
PermutationBatch batchProduct(const PermutationBatch &a,
                              const PermutationBatch &b) {
  sameShape(a, b);
  validateBatch(a);
  validateBatch(b);
  PermutationBatch r;
  r.reserve(a.size());
  for (std::size_t i = 0; i < a.size(); ++i)
    r.push_back(product(a[i], b[i]));
  return r;
}
PermutationBatch batchInverse(const PermutationBatch &p) {
  validateBatch(p);
  PermutationBatch r;
  r.reserve(p.size());
  for (const auto &x : p)
    r.push_back(inverse(x));
  return r;
}
std::vector<int> batchOnPoints(const std::vector<int> &points,
                               const PermutationBatch &p) {
  validateBatch(p);
  if (points.size() != p.size())
    throw std::invalid_argument("counts differ");
  std::vector<int> r;
  for (std::size_t i = 0; i < p.size(); ++i)
    r.push_back(onPoint(points[i], p[i]));
  return r;
}
IntegerMatrix batchApply(const IntegerMatrix &v, const PermutationBatch &p) {
  sameShape(v, p);
  validateBatch(p);
  IntegerMatrix r(v.size());
  for (std::size_t i = 0; i < v.size(); ++i) {
    r[i].resize(v[i].size());
    for (std::size_t j = 0; j < v[i].size(); ++j)
      r[i][j] = v[i][p[i][j] - 1];
  }
  return r;
}
MinimumImagesResult minimumImages(const IntegerMatrix &rows,
                                  const std::vector<std::size_t> &cols) {
  if (rows.empty() || cols.empty())
    throw std::invalid_argument("empty input");
  auto n = rows.front().size();
  for (const auto &r : rows)
    if (r.size() != n)
      throw std::invalid_argument("not rectangular");
  for (auto c : cols)
    if (c >= n)
      throw std::invalid_argument("column out of range");
  auto less = [&](const auto &a, const auto &b) {
    for (auto c : cols) {
      if (a[c] != b[c])
        return a[c] < b[c];
    }
    return false;
  };
  auto best = std::min_element(rows.begin(), rows.end(), less);
  MinimumImagesResult out;
  for (auto c : cols)
    out.minimum.push_back((*best)[c]);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    bool equal = true;
    for (std::size_t j = 0; j < cols.size(); ++j)
      if (rows[i][cols[j]] != out.minimum[j])
        equal = false;
    if (equal)
      out.survivorIndices.push_back(i);
  }
  return out;
}
IntegerMatrix sortUniqueRows(IntegerMatrix rows) {
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
  return rows;
}
} // namespace xperm::niehoff

namespace xperm::niehoff {
namespace {
bool lessConfigStage2(const SearchConfiguration &a,
                      const SearchConfiguration &b) {
  return std::tie(a.slotPermutation, a.labelPermutation, a.sign, a.metadata) <
         std::tie(b.slotPermutation, b.labelPermutation, b.sign, b.metadata);
}
bool sameConfigStage2(const SearchConfiguration &a,
                      const SearchConfiguration &b) {
  return a.slotPermutation == b.slotPermutation &&
         a.labelPermutation == b.labelPermutation && a.sign == b.sign &&
         a.metadata == b.metadata && a.fixedLabels == b.fixedLabels;
}
} // namespace
std::vector<SearchConfiguration>
expandConfigurations(const SearchLevelInput &in) {
  if (in.parentIndices.size() != in.slotTransversals.size() ||
      in.parentIndices.size() != in.labelTransversals.size())
    throw std::invalid_argument("candidate counts differ");
  std::vector<SearchConfiguration> out;
  for (std::size_t i = 0; i < in.parentIndices.size(); ++i) {
    if (in.parentIndices[i] >= in.configurations.size())
      throw std::invalid_argument("parent out of range");
    auto c = in.configurations[in.parentIndices[i]];
    c.slotPermutation = product(c.slotPermutation, in.slotTransversals[i]);
    c.labelPermutation = product(c.labelPermutation, in.labelTransversals[i]);
    out.push_back(std::move(c));
  }
  return out;
}
std::vector<int> candidatePointImages(const std::vector<SearchConfiguration> &c,
                                      int point) {
  std::vector<int> out;
  for (const auto &x : c)
    out.push_back(onPoint(point, x.labelPermutation));
  return out;
}
SearchLevelResult
filterMinimumAndDeduplicate(std::vector<SearchConfiguration> c,
                            std::vector<std::size_t> parents, int point) {
  if (c.size() != parents.size())
    throw std::invalid_argument("counts differ");
  SearchLevelResult out;
  if (c.empty())
    return out;
  auto images = candidatePointImages(c, point);
  out.minimumImage = *std::min_element(images.begin(), images.end());
  struct X {
    SearchConfiguration c;
    std::size_t p;
  };
  std::vector<X> s;
  for (std::size_t i = 0; i < c.size(); ++i)
    if (images[i] == out.minimumImage)
      s.push_back({std::move(c[i]), parents[i]});
  std::stable_sort(s.begin(), s.end(), [](const X &a, const X &b) {
    return lessConfigStage2(a.c, b.c);
  });
  s.erase(std::unique(s.begin(), s.end(),
                      [](const X &a, const X &b) {
                        return sameConfigStage2(a.c, b.c);
                      }),
          s.end());
  for (auto &x : s) {
    out.configurations.push_back(std::move(x.c));
    out.parentIndices.push_back(x.p);
  }
  return out;
}
SearchLevelResult advanceSearchLevel(const SearchLevelInput &in) {
  return filterMinimumAndDeduplicate(expandConfigurations(in), in.parentIndices,
                                     in.selectedPoint);
}
} // namespace xperm::niehoff

namespace xperm::niehoff {
namespace {
IntegerRow flat(const PermutationBatch &p) {
  IntegerRow r;
  for (const auto &x : p) {
    if (!isPermutation(x))
      throw std::invalid_argument("invalid generator");
    r.insert(r.end(), x.begin(), x.end());
  }
  return r;
}
PermutationBatch rows(const int *p, int m, int n) {
  PermutationBatch r;
  for (int i = 0; i < m; ++i)
    r.emplace_back(p + i * n, p + (i + 1) * n);
  return r;
}
bool lessC(const SearchConfiguration &a, const SearchConfiguration &b) {
  return std::tie(a.slotPermutation, a.labelPermutation, a.sign, a.metadata) <
         std::tie(b.slotPermutation, b.labelPermutation, b.sign, b.metadata);
}
bool sameC(const SearchConfiguration &a, const SearchConfiguration &b) {
  return a.slotPermutation == b.slotPermutation &&
         a.labelPermutation == b.labelPermutation && a.sign == b.sign &&
         a.metadata == b.metadata && a.fixedLabels == b.fixedLabels;
}
} // namespace
BSGS makeBSGS(const IntegerRow &base, const PermutationBatch &generators) {
  if (generators.empty())
    throw std::invalid_argument("empty generators");
  int n = generators.front().size();
  auto f = flat(generators);
  IntegerRow nb(n);
  int nbl = 0, nm = 0, num = 0;
  std::vector<int> sg;
  ::schreier_sims(const_cast<int *>(base.data()), static_cast<int>(base.size()),
                  f.data(), static_cast<int>(generators.size()), n, nb.data(),
                  &nbl, sg, &nm, &num);
  return {n, IntegerRow(nb.begin(), nb.begin() + nbl), rows(sg.data(), nm, n)};
}
PermutationBatch stabilizerOfBasePrefix(const BSGS &g, std::size_t prefix) {
  auto f = flat(g.strongGenerators);
  IntegerRow out(f.size());
  int m = 0;
  ::stabilizer(const_cast<int *>(g.base.data()), prefix, f.data(),
               g.strongGenerators.size(), g.degree, out.data(), &m);
  return rows(out.data(), m, g.degree);
}
BasicOrbit basicOrbit(const BSGS &g, std::size_t level) {
  auto gens = stabilizerOfBasePrefix(g, level);
  auto f = flat(gens);
  BasicOrbit o;
  o.root = g.base.at(level);
  o.points.resize(g.degree);
  o.nu.resize(g.degree * g.degree);
  o.w.resize(g.degree);
  int ol = 0;
  IntegerRow positions(gens.size());
  std::iota(positions.begin(), positions.end(), 0);
  ::one_schreier_orbit_chain(o.root, f.data(), positions.data(),
                             static_cast<int>(positions.size()), g.degree,
                             o.points.data(), &ol, o.nu.data(), o.w.data(), 1);
  o.points.resize(ol);
  return o;
}
Permutation traceRepresentative(const BasicOrbit &o, int point, int degree) {
  Permutation r(degree);
  ::trace_schreier(point, const_cast<int *>(o.nu.data()),
                   const_cast<int *>(o.w.data()), r.data(), degree);
  return r;
}
GroupSearchLevelResult
advanceGroupSearchLevel(const GroupSearchLevelInput &in) {
  IntegerRow b = in.tentativeBase;
  b.erase(std::remove(b.begin(), b.end(), in.selectedSlot), b.end());
  b.insert(b.begin(), in.selectedSlot);
  auto g = makeBSGS(b, in.slotGenerators);
  auto o = basicOrbit(g, 0);
  struct C {
    SearchConfiguration c;
    std::size_t p;
    int image;
  };
  std::vector<C> all;
  for (std::size_t p = 0; p < in.configurations.size(); ++p)
    for (int x : o.points) {
      auto c = in.configurations[p];
      c.slotPermutation =
          product(traceRepresentative(o, x, g.degree), c.slotPermutation);
      all.push_back(
          {std::move(c), p, onPoint(x, in.configurations[p].labelPermutation)});
    }
  int min = std::min_element(all.begin(), all.end(), [](auto &a, auto &b) {
              return a.image < b.image;
            })->image;
  all.erase(std::remove_if(all.begin(), all.end(),
                           [&](auto &x) { return x.image != min; }),
            all.end());
  std::sort(all.begin(), all.end(),
            [](auto &a, auto &b) { return lessC(a.c, b.c); });
  all.erase(std::unique(all.begin(), all.end(),
                        [](auto &a, auto &b) { return sameC(a.c, b.c); }),
            all.end());
  GroupSearchLevelResult out;
  out.minimumImage = min;
  out.group = g;
  out.orbit = o;
  out.nextStabilizerGenerators = stabilizerOfBasePrefix(g, 1);
  for (auto &x : all) {
    out.configurations.push_back(std::move(x.c));
    out.parentIndices.push_back(x.p);
  }
  return out;
}
} // namespace xperm::niehoff

namespace xperm::niehoff {
namespace {
IntegerRow flatten(const PermutationBatch &ps) {
  IntegerRow f;
  if (ps.empty())
    return f;
  auto n = ps.front().size();
  f.reserve(ps.size() * n);
  for (const auto &p : ps) {
    if (p.size() != n || !isPermutation(p))
      throw std::invalid_argument("invalid permutations");
    f.insert(f.end(), p.begin(), p.end());
  }
  return f;
}
PermutationBatch unflatten(const int *p, int m, int n) {
  PermutationBatch r;
  for (int i = 0; i < m; ++i)
    r.emplace_back(p + i * n, p + (i + 1) * n);
  return r;
}
IntegerRow orderedBase(const IntegerRow &initial, const IntegerRow &selected) {
  IntegerRow r;
  for (int x : selected)
    if (std::find(r.begin(), r.end(), x) == r.end())
      r.push_back(x);
  for (int x : initial)
    if (std::find(r.begin(), r.end(), x) == r.end())
      r.push_back(x);
  return r;
}
bool lessConfig(const SearchConfiguration &a, const SearchConfiguration &b) {
  return std::tie(a.slotPermutation, a.labelPermutation, a.sign, a.metadata) <
         std::tie(b.slotPermutation, b.labelPermutation, b.sign, b.metadata);
}
bool equalConfig(const SearchConfiguration &a, const SearchConfiguration &b) {
  return a.slotPermutation == b.slotPermutation &&
         a.labelPermutation == b.labelPermutation && a.sign == b.sign &&
         a.metadata == b.metadata && a.fixedLabels == b.fixedLabels;
}
} // namespace
StabilizerChainState
initializeStabilizerChain(const IntegerRow &tentativeBase,
                          const PermutationBatch &generators,
                          const IntegerRow &selectedSlots) {
  if (generators.empty()) {
    throw std::invalid_argument("empty generators");
  }
  int n = static_cast<int>(generators.front().size());
  IntegerRow base = orderedBase(tentativeBase, selectedSlots);
  IntegerRow flat = flatten(generators), newBase(static_cast<std::size_t>(n));
  int nbl = 0, nm = 0, num = 0;
  std::vector<int> strong;
  ::schreier_sims(base.data(), static_cast<int>(base.size()), flat.data(),
                  static_cast<int>(generators.size()), n, newBase.data(), &nbl,
                  strong, &nm, &num);
  StabilizerChainState state;
  state.degree = n;
  state.base.assign(newBase.begin(), newBase.begin() + nbl);
  state.strongGenerators = unflatten(strong.data(), nm, n);
  if (state.base.size() < selectedSlots.size() ||
      !std::equal(selectedSlots.begin(), selectedSlots.end(),
                  state.base.begin()))
    throw std::runtime_error("requested base prefix not retained");
  IntegerRow flatSGS = flatten(state.strongGenerators);
  StabilizerChain chain;
  ::stab_chain(state.base.data(), static_cast<int>(state.base.size()),
               flatSGS.data(), static_cast<int>(state.strongGenerators.size()),
               n, chain);
  state.generatorPositions.assign(chain.begin(), chain.end());
  return state;
}
BasicOrbit currentBasicOrbit(const StabilizerChainState &s) {
  if (s.level >= s.base.size())
    throw std::invalid_argument("level outside chain");
  IntegerRow flat = flatten(s.strongGenerators);
  BasicOrbit o;
  o.root = s.base[s.level];
  o.points.resize(s.degree);
  o.nu.resize(static_cast<std::size_t>(s.degree * s.degree));
  o.w.resize(s.degree);
  int ol = 0;
  const auto &positions = s.generatorPositions[s.level];
  ::one_schreier_orbit_chain(o.root, flat.data(),
                             const_cast<int *>(positions.data()),
                             static_cast<int>(positions.size()), s.degree,
                             o.points.data(), &ol, o.nu.data(), o.w.data(), 1);
  o.points.resize(ol);
  return o;
}
PersistentSearchResult
runPersistentGroupSearch(const PersistentSearchInput &in) {
  PersistentSearchResult out;
  if (in.configurations.empty())
    return out;
  out.configurations = in.configurations;
  out.group = initializeStabilizerChain(in.tentativeBase, in.slotGenerators,
                                        in.selectedSlots);
  for (int selected : in.selectedSlots) {
    if (out.group.base[out.group.level] != selected)
      throw std::runtime_error("chain/base mismatch");
    BasicOrbit orbit = currentBasicOrbit(out.group);
    struct Image {
      std::size_t parent;
      int point;
      int image;
    };
    std::vector<Image> images;
    for (std::size_t p = 0; p < out.configurations.size(); ++p)
      for (int point : orbit.points)
        images.push_back(
            {p, point, onPoint(point, out.configurations[p].labelPermutation)});
    int minimum = std::min_element(images.begin(), images.end(),
                                   [](const auto &a, const auto &b) {
                                     return a.image < b.image;
                                   })
                      ->image;
    std::unordered_map<int, Permutation> cache;
    std::vector<SearchConfiguration> survivors;
    for (const auto &c : images) {
      if (c.image != minimum)
        continue;
      auto [it, inserted] = cache.try_emplace(c.point);
      if (inserted)
        it->second = traceRepresentative(orbit, c.point, out.group.degree);
      auto expanded = out.configurations[c.parent];
      expanded.slotPermutation = product(it->second, expanded.slotPermutation);
      survivors.push_back(std::move(expanded));
    }
    std::stable_sort(survivors.begin(), survivors.end(), lessConfig);
    survivors.erase(
        std::unique(survivors.begin(), survivors.end(), equalConfig),
        survivors.end());
    out.configurations = std::move(survivors);
    out.levels.push_back(
        {selected, minimum, orbit.points, out.configurations.size()});
    ++out.group.level;
  }
  return out;
}

// -----------------------------------------------------------------------------
// Stage 5: structured label groups
// -----------------------------------------------------------------------------
namespace {

Permutation identityPermutation(int degree) {
  Permutation result(static_cast<std::size_t>(degree));
  std::iota(result.begin(), result.end(), 1);
  return result;
}

bool containsLabel(const IntegerRow &labels, int label) {
  return std::find(labels.begin(), labels.end(), label) != labels.end();
}

const LabelGroup &groupContainingLabel(const std::vector<LabelGroup> &groups,
                                       int label) {
  const auto position = std::find_if(
      groups.begin(), groups.end(), [label](const LabelGroup &group) {
        return containsLabel(group.labels, label);
      });
  if (position == groups.end())
    throw std::invalid_argument("label does not belong to a label group");
  return *position;
}

Permutation transpositionRepresentative(int first, int second, int degree) {
  Permutation representative = identityPermutation(degree);
  if (first != second)
    std::swap(representative[static_cast<std::size_t>(first - 1)],
              representative[static_cast<std::size_t>(second - 1)]);
  return representative;
}

Permutation pairRepresentative(int sourceFirst, int sourceSecond,
                               int targetFirst, int targetSecond, int degree) {
  Permutation representative = identityPermutation(degree);
  if (sourceFirst == targetFirst && sourceSecond == targetSecond)
    return representative;
  if (sourceFirst == targetSecond && sourceSecond == targetFirst) {
    std::swap(representative[static_cast<std::size_t>(sourceFirst - 1)],
              representative[static_cast<std::size_t>(sourceSecond - 1)]);
    return representative;
  }
  std::swap(representative[static_cast<std::size_t>(sourceFirst - 1)],
            representative[static_cast<std::size_t>(targetFirst - 1)]);
  std::swap(representative[static_cast<std::size_t>(sourceSecond - 1)],
            representative[static_cast<std::size_t>(targetSecond - 1)]);
  return representative;
}

IntegerRow mergedLabels(IntegerRow fixedLabels,
                        const IntegerRow &newlyFixedLabels) {
  fixedLabels.insert(fixedLabels.end(), newlyFixedLabels.begin(),
                     newlyFixedLabels.end());
  std::sort(fixedLabels.begin(), fixedLabels.end());
  fixedLabels.erase(std::unique(fixedLabels.begin(), fixedLabels.end()),
                    fixedLabels.end());
  return fixedLabels;
}

} // namespace

void validateLabelGroups(const std::vector<LabelGroup> &groups, int degree) {
  if (degree <= 0)
    throw std::invalid_argument("invalid label degree");
  std::vector<bool> seen(static_cast<std::size_t>(degree + 1), false);
  for (const auto &group : groups) {
    if (group.labels.empty())
      throw std::invalid_argument("empty label group");
    const bool paired = group.type == LabelGroupType::Dummy ||
                        group.type == LabelGroupType::SymmetricMetric ||
                        group.type == LabelGroupType::AntisymmetricMetric;
    if (paired && group.labels.size() % 2 != 0)
      throw std::invalid_argument("paired label group has odd length");
    for (const int label : group.labels) {
      if (label < 1 || label > degree || seen[static_cast<std::size_t>(label)])
        throw std::invalid_argument("invalid or repeated label in groups");
      seen[static_cast<std::size_t>(label)] = true;
    }
  }
  for (int label = 1; label <= degree; ++label)
    if (!seen[static_cast<std::size_t>(label)])
      throw std::invalid_argument("label groups do not cover the degree");
}

std::vector<LabelCandidate>
canonicalizeLabel(int sourceLabel, const IntegerRow &fixedLabels,
                  const std::vector<LabelGroup> &groups, int degree) {
  if (sourceLabel < 1 || sourceLabel > degree)
    throw std::invalid_argument("source label outside degree");
  const LabelGroup &group = groupContainingLabel(groups, sourceLabel);
  const Permutation identity = identityPermutation(degree);

  if (containsLabel(fixedLabels, sourceLabel))
    return {{sourceLabel, identity, {}, 1}};

  if (group.type == LabelGroupType::Fixed || group.type == LabelGroupType::Free)
    return {{sourceLabel, identity, {sourceLabel}, 1}};

  std::vector<LabelCandidate> candidates;
  if (group.type == LabelGroupType::Repeated) {
    for (const int target : group.labels) {
      if (containsLabel(fixedLabels, target))
        continue;
      candidates.push_back(
          {target,
           transpositionRepresentative(sourceLabel, target, degree),
           {target},
           1});
    }
  } else {
    std::size_t sourcePair = 0;
    bool sourceIsSecond = false;
    bool foundSource = false;
    for (std::size_t pair = 0; pair < group.labels.size() / 2; ++pair) {
      if (group.labels[2 * pair] == sourceLabel ||
          group.labels[2 * pair + 1] == sourceLabel) {
        sourcePair = pair;
        sourceIsSecond = group.labels[2 * pair + 1] == sourceLabel;
        foundSource = true;
        break;
      }
    }
    if (!foundSource)
      throw std::logic_error("paired source label was not found");

    const int sourceFirst = group.labels[2 * sourcePair];
    const int sourceSecond = group.labels[2 * sourcePair + 1];
    for (std::size_t pair = 0; pair < group.labels.size() / 2; ++pair) {
      const int targetFirst = group.labels[2 * pair];
      const int targetSecond = group.labels[2 * pair + 1];
      if (containsLabel(fixedLabels, targetFirst) ||
          containsLabel(fixedLabels, targetSecond))
        continue;

      bool reverse = sourceIsSecond;
      if (group.type == LabelGroupType::Dummy)
        reverse = false;
      const int mappedFirst = reverse ? targetSecond : targetFirst;
      const int mappedSecond = reverse ? targetFirst : targetSecond;
      const int canonicalLabel = sourceIsSecond ? mappedSecond : mappedFirst;
      const int sign =
          group.type == LabelGroupType::AntisymmetricMetric && reverse ? -1 : 1;
      candidates.push_back(
          {canonicalLabel,
           pairRepresentative(sourceFirst, sourceSecond, mappedFirst,
                              mappedSecond, degree),
           {targetFirst, targetSecond},
           sign});
    }
  }

  if (candidates.empty())
    throw std::runtime_error("no admissible label target remains");
  const int minimum =
      std::min_element(
          candidates.begin(), candidates.end(),
          [](const LabelCandidate &first, const LabelCandidate &second) {
            return first.canonicalLabel < second.canonicalLabel;
          })
          ->canonicalLabel;
  candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                  [minimum](const LabelCandidate &candidate) {
                                    return candidate.canonicalLabel != minimum;
                                  }),
                   candidates.end());
  return candidates;
}

LabelGroupSearchResult runLabelGroupSearch(const LabelGroupSearchInput &input) {
  LabelGroupSearchResult result;
  if (input.configurations.empty())
    return result;
  const int degree =
      static_cast<int>(input.configurations.front().slotPermutation.size());
  validateLabelGroups(input.labelGroups, degree);

  result.configurations = input.configurations;
  result.group = initializeStabilizerChain(
      input.tentativeBase, input.slotGenerators, input.selectedSlots);

  for (const int selectedSlot : input.selectedSlots) {
    if (result.group.level >= result.group.base.size() ||
        result.group.base[result.group.level] != selectedSlot)
      throw std::runtime_error("label search/base mismatch");
    const BasicOrbit orbit = currentBasicOrbit(result.group);

    struct CandidateDescription {
      std::size_t parent = 0;
      int orbitPoint = 0;
      LabelCandidate label;
    };
    std::vector<CandidateDescription> candidates;
    int minimumLabel = degree + 1;
    for (std::size_t parent = 0; parent < result.configurations.size();
         ++parent) {
      const auto &configuration = result.configurations[parent];
      for (const int orbitPoint : orbit.points) {
        const int sourceLabel =
            onPoint(orbitPoint, configuration.labelPermutation);
        for (auto label :
             canonicalizeLabel(sourceLabel, configuration.fixedLabels,
                               input.labelGroups, degree)) {
          minimumLabel = std::min(minimumLabel, label.canonicalLabel);
          candidates.push_back({parent, orbitPoint, std::move(label)});
        }
      }
    }

    std::unordered_map<int, Permutation> representativeCache;
    std::vector<SearchConfiguration> survivors;
    for (const auto &candidate : candidates) {
      if (candidate.label.canonicalLabel != minimumLabel)
        continue;
      auto [position, inserted] =
          representativeCache.try_emplace(candidate.orbitPoint);
      if (inserted)
        position->second =
            traceRepresentative(orbit, candidate.orbitPoint, degree);

      SearchConfiguration expanded = result.configurations[candidate.parent];
      expanded.slotPermutation =
          product(position->second, expanded.slotPermutation);
      expanded.labelPermutation =
          product(expanded.labelPermutation, candidate.label.representative);
      expanded.sign *= candidate.label.sign;
      expanded.fixedLabels = mergedLabels(std::move(expanded.fixedLabels),
                                          candidate.label.newlyFixedLabels);
      survivors.push_back(std::move(expanded));
    }

    std::stable_sort(survivors.begin(), survivors.end(), lessConfig);
    survivors.erase(
        std::unique(survivors.begin(), survivors.end(), equalConfig),
        survivors.end());
    result.configurations = std::move(survivors);
    result.levels.push_back({selectedSlot, minimumLabel, orbit.points,
                             result.configurations.size()});
    ++result.group.level;
  }
  return result;
}

// -----------------------------------------------------------------------------
// Stage 6: propagated signed symmetries
// -----------------------------------------------------------------------------
namespace {

struct ConfigurationKey {
  Permutation slotPermutation;
  Permutation labelPermutation;
  IntegerRow metadata;
  IntegerRow fixedLabels;

  bool operator<(const ConfigurationKey &other) const {
    return std::tie(slotPermutation, labelPermutation, metadata, fixedLabels) <
           std::tie(other.slotPermutation, other.labelPermutation,
                    other.metadata, other.fixedLabels);
  }
};

struct SignedOrbitElement {
  Permutation permutation;
  int sign = 1;
};

struct SignedPermutationKey {
  Permutation permutation;
  int sign = 1;

  bool operator<(const SignedPermutationKey &other) const {
    return std::tie(permutation, sign) <
           std::tie(other.permutation, other.sign);
  }
};

std::vector<SignedOrbitElement>
signedGroupClosure(const std::vector<SignedPermutation> &generators, int degree,
                   std::size_t maximumOrbitSize) {
  const SignedOrbitElement identity{identityPermutation(degree), 1};
  std::queue<SignedOrbitElement> pending;
  std::set<SignedPermutationKey> visited;
  std::vector<SignedOrbitElement> result;
  pending.push(identity);
  visited.insert({identity.permutation, identity.sign});

  while (!pending.empty()) {
    SignedOrbitElement current = std::move(pending.front());
    pending.pop();
    result.push_back(current);
    if (result.size() > maximumOrbitSize)
      throw std::runtime_error(
          "propagated-symmetry orbit exceeded the configured limit");

    for (const auto &generator : generators) {
      SignedOrbitElement next;
      next.permutation = product(current.permutation, generator.permutation);
      next.sign = current.sign * generator.sign;
      if (visited.insert({next.permutation, next.sign}).second)
        pending.push(std::move(next));
    }
  }
  return result;
}

ConfigurationKey configurationKey(const SearchConfiguration &configuration) {
  return {configuration.slotPermutation, configuration.labelPermutation,
          configuration.metadata, configuration.fixedLabels};
}

} // namespace

void validatePropagatedSymmetries(
    const std::vector<SignedPermutation> &generators, int degree) {
  if (degree <= 0)
    throw std::invalid_argument("invalid propagated-symmetry degree");
  for (const auto &generator : generators) {
    if (static_cast<int>(generator.permutation.size()) != degree ||
        !isPermutation(generator.permutation))
      throw std::invalid_argument("invalid propagated-symmetry permutation");
    if (generator.sign != 1 && generator.sign != -1)
      throw std::invalid_argument("propagated-symmetry sign must be +1 or -1");
  }
}

PropagatedSymmetryResult
reduceByPropagatedSymmetries(std::vector<SearchConfiguration> configurations,
                             const std::vector<SignedPermutation> &generators,
                             std::size_t maximumOrbitSize) {
  PropagatedSymmetryResult result;
  if (configurations.empty())
    return result;
  const int degree =
      static_cast<int>(configurations.front().slotPermutation.size());
  validatePropagatedSymmetries(generators, degree);
  if (generators.empty()) {
    result.configurations = std::move(configurations);
    return result;
  }

  const auto closure = signedGroupClosure(generators, degree, maximumOrbitSize);
  std::map<ConfigurationKey, SearchConfiguration> representatives;
  std::map<ConfigurationKey, int> representativeSigns;

  for (const auto &configuration : configurations) {
    ConfigurationKey bestKey;
    SearchConfiguration bestConfiguration;
    int bestSign = 0;
    bool initialized = false;
    std::map<ConfigurationKey, std::set<int>> orbitSigns;

    for (const auto &symmetry : closure) {
      SearchConfiguration transformed = configuration;
      transformed.slotPermutation =
          product(symmetry.permutation, configuration.slotPermutation);
      transformed.sign = configuration.sign * symmetry.sign;
      const ConfigurationKey key = configurationKey(transformed);
      orbitSigns[key].insert(transformed.sign);
      if (!initialized || key < bestKey) {
        bestKey = key;
        bestConfiguration = std::move(transformed);
        bestSign = bestConfiguration.sign;
        initialized = true;
      } else if (!(bestKey < key) && !(key < bestKey) &&
                 transformed.sign < bestSign) {
        bestConfiguration = std::move(transformed);
        bestSign = bestConfiguration.sign;
      }
    }

    for (const auto &[key, signs] : orbitSigns) {
      if (signs.size() > 1) {
        result.zero = true;
        result.cancelledConfigurationCount += 1;
        result.configurations.clear();
        return result;
      }
    }

    const auto existing = representatives.find(bestKey);
    if (existing == representatives.end()) {
      representativeSigns.emplace(bestKey, bestConfiguration.sign);
      representatives.emplace(std::move(bestKey), std::move(bestConfiguration));
    } else if (representativeSigns.at(bestKey) == bestConfiguration.sign) {
      result.mergedConfigurationCount += 1;
    } else {
      result.zero = true;
      result.cancelledConfigurationCount += 2;
      result.configurations.clear();
      return result;
    }
  }

  result.configurations.reserve(representatives.size());
  for (auto &[key, configuration] : representatives)
    result.configurations.push_back(std::move(configuration));
  return result;
}

PropagatedSymmetryResult
runPropagatedSymmetrySearch(const PropagatedSymmetryInput &input) {
  const auto labelResult = runLabelGroupSearch(input.labelSearch);
  auto result = reduceByPropagatedSymmetries(
      labelResult.configurations, input.generators, input.maximumOrbitSize);
  result.group = labelResult.group;
  result.levels = labelResult.levels;
  return result;
}

// -----------------------------------------------------------------------------
// Stage 7: legacy LL_canonical_perm adapter
// -----------------------------------------------------------------------------
namespace {

void validateLegacyInput(const LegacyCanonicalPermInput &input) {
  if (input.degree < 3 ||
      static_cast<int>(input.permutation.size()) != input.degree ||
      !isPermutation(input.permutation))
    throw std::invalid_argument("invalid signed input permutation");
  if (input.dummySetLengths.size() != input.metricSymmetries.size())
    throw std::invalid_argument("dummy-set and metric counts differ");
  const auto dummyCount = std::accumulate(input.dummySetLengths.begin(),
                                          input.dummySetLengths.end(), 0);
  const auto repeatedCount = std::accumulate(input.repeatedSetLengths.begin(),
                                             input.repeatedSetLengths.end(), 0);
  if (dummyCount != static_cast<int>(input.dummyLabels.size()) ||
      repeatedCount != static_cast<int>(input.repeatedLabels.size()))
    throw std::invalid_argument("legacy set lengths do not match their data");
  for (const auto &generator : input.slotGenerators)
    if (static_cast<int>(generator.size()) != input.degree ||
        !isPermutation(generator))
      throw std::invalid_argument("invalid slot generator");
}

IntegerRow realSearchBase(const LegacyCanonicalPermInput &input) {
  const int realDegree = input.degree - 2;
  IntegerRow result;
  for (const int point : input.base)
    if (point >= 1 && point <= realDegree &&
        std::find(result.begin(), result.end(), point) == result.end())
      result.push_back(point);
  for (int point = 1; point <= realDegree; ++point)
    if (std::find(result.begin(), result.end(), point) == result.end())
      result.push_back(point);
  return result;
}

} // namespace

std::vector<LabelGroup>
makeLegacyLabelGroups(const LegacyCanonicalPermInput &input) {
  validateLegacyInput(input);
  std::vector<LabelGroup> groups;
  std::vector<bool> covered(static_cast<std::size_t>(input.degree + 1), false);

  for (const int label : input.freeLabels) {
    if (label < 1 || label > input.degree || covered[label])
      throw std::invalid_argument("invalid or repeated free label");
    groups.push_back({LabelGroupType::Free, {label}});
    covered[label] = true;
  }

  std::size_t offset = 0;
  for (std::size_t set = 0; set < input.dummySetLengths.size(); ++set) {
    const int length = input.dummySetLengths[set];
    if (length <= 0 || length % 2 != 0)
      throw std::invalid_argument("dummy-set length must be positive and even");
    LabelGroupType type = LabelGroupType::Dummy;
    if (input.metricSymmetries[set] == 1)
      type = LabelGroupType::SymmetricMetric;
    else if (input.metricSymmetries[set] == -1)
      type = LabelGroupType::AntisymmetricMetric;
    else if (input.metricSymmetries[set] != 0)
      throw std::invalid_argument("invalid metric symmetry");

    IntegerRow labels(input.dummyLabels.begin() +
                          static_cast<std::ptrdiff_t>(offset),
                      input.dummyLabels.begin() +
                          static_cast<std::ptrdiff_t>(offset + length));
    for (const int label : labels) {
      if (label < 1 || label > input.degree || covered[label])
        throw std::invalid_argument("invalid or repeated dummy label");
      covered[label] = true;
    }
    groups.push_back({type, std::move(labels)});
    offset += static_cast<std::size_t>(length);
  }

  offset = 0;
  for (const int length : input.repeatedSetLengths) {
    if (length <= 0)
      throw std::invalid_argument("repeated-set length must be positive");
    IntegerRow labels(input.repeatedLabels.begin() +
                          static_cast<std::ptrdiff_t>(offset),
                      input.repeatedLabels.begin() +
                          static_cast<std::ptrdiff_t>(offset + length));
    for (const int label : labels) {
      if (label < 1 || label > input.degree || covered[label])
        throw std::invalid_argument("invalid or repeated repeated-set label");
      covered[label] = true;
    }
    groups.push_back({LabelGroupType::Repeated, std::move(labels)});
    offset += static_cast<std::size_t>(length);
  }

  for (int label = 1; label <= input.degree; ++label)
    if (!covered[label])
      groups.push_back({LabelGroupType::Fixed, {label}});
  validateLabelGroups(groups, input.degree);
  return groups;
}

namespace {

int signOf(const Permutation &permutation) {
  const int degree = static_cast<int>(permutation.size());
  if (permutation[degree - 2] == degree - 1 &&
      permutation[degree - 1] == degree)
    return 1;
  if (permutation[degree - 2] == degree &&
      permutation[degree - 1] == degree - 1)
    return -1;
  throw std::invalid_argument("invalid xPerm sign points");
}

void multiplySign(Permutation &permutation, int sign) {
  if (sign == 1)
    return;
  const int degree = static_cast<int>(permutation.size());
  for (int &image : permutation) {
    if (image == degree - 1)
      image = degree;
    else if (image == degree)
      image = degree - 1;
  }
}

Permutation normalizeLabelGroups(const Permutation &configuration,
                                 const std::vector<LabelGroup> &groups,
                                 int realDegree) {
  const int degree = static_cast<int>(configuration.size());
  Permutation labelMap = identityPermutation(degree);
  int extraSign = 1;
  IntegerRow position(static_cast<std::size_t>(degree + 1), 0);
  for (int slot = 1; slot <= realDegree; ++slot)
    position[configuration[slot - 1]] = slot;

  for (const auto &group : groups) {
    if (group.type == LabelGroupType::Fixed ||
        group.type == LabelGroupType::Free)
      continue;
    if (group.type == LabelGroupType::Repeated) {
      IntegerRow source = group.labels;
      std::stable_sort(source.begin(), source.end(),
                       [&](int first, int second) {
                         return position[first] < position[second];
                       });
      for (std::size_t index = 0; index < source.size(); ++index)
        labelMap[source[index] - 1] = group.labels[index];
      continue;
    }

    struct Occurrence {
      std::size_t pair;
      int firstPosition;
      bool reversed;
    };
    std::vector<Occurrence> occurrences;
    const std::size_t pairs = group.labels.size() / 2;
    occurrences.reserve(pairs);
    for (std::size_t pair = 0; pair < pairs; ++pair) {
      const int first = group.labels[2 * pair];
      const int second = group.labels[2 * pair + 1];
      if (position[first] == 0 || position[second] == 0)
        throw std::runtime_error("dummy label missing from configuration");
      occurrences.push_back({pair, std::min(position[first], position[second]),
                             position[second] < position[first]});
    }
    std::stable_sort(occurrences.begin(), occurrences.end(),
                     [](const Occurrence &first, const Occurrence &second) {
                       return first.firstPosition < second.firstPosition;
                     });
    for (std::size_t target = 0; target < pairs; ++target) {
      const auto &occurrence = occurrences[target];
      const int sourceFirst = group.labels[2 * occurrence.pair];
      const int sourceSecond = group.labels[2 * occurrence.pair + 1];
      const int targetFirst = group.labels[2 * target];
      const int targetSecond = group.labels[2 * target + 1];
      const bool hasMetric = group.type == LabelGroupType::SymmetricMetric ||
                             group.type == LabelGroupType::AntisymmetricMetric;
      const bool reverse = hasMetric && occurrence.reversed;
      labelMap[sourceFirst - 1] = reverse ? targetSecond : targetFirst;
      labelMap[sourceSecond - 1] = reverse ? targetFirst : targetSecond;
      if (reverse && group.type == LabelGroupType::AntisymmetricMetric)
        extraSign = -extraSign;
    }
  }

  Permutation result(configuration.size());
  for (std::size_t slot = 0; slot < configuration.size(); ++slot)
    result[slot] = labelMap[configuration[slot] - 1];
  multiplySign(result, extraSign);
  return result;
}

void validateTotalSubsets(const std::vector<TotalSymmetrySubset> &subsets,
                          int realDegree) {
  std::vector<bool> used(static_cast<std::size_t>(realDegree + 1), false);
  for (const auto &subset : subsets) {
    if (subset.slots.size() < 2 || (subset.sign != 1 && subset.sign != -1))
      throw std::invalid_argument("invalid total-symmetry subset");
    for (const int slot : subset.slots) {
      if (slot < 1 || slot > realDegree || used[slot])
        throw std::invalid_argument("overlapping or invalid symmetry subset");
      used[slot] = true;
    }
  }
}

int inversionParity(const IntegerRow &values) {
  int parity = 0;
  for (std::size_t first = 0; first < values.size(); ++first)
    for (std::size_t second = first + 1; second < values.size(); ++second)
      parity ^= values[first] > values[second];
  return parity;
}

Permutation
normalizeTotalSubsets(Permutation configuration,
                      const std::vector<TotalSymmetrySubset> &subsets) {
  int extraSign = 1;
  for (const auto &subset : subsets) {
    IntegerRow values;
    values.reserve(subset.slots.size());
    for (const int slot : subset.slots)
      values.push_back(configuration[slot - 1]);
    if (subset.sign == -1 && inversionParity(values))
      extraSign = -extraSign;
    std::sort(values.begin(), values.end());
    for (std::size_t index = 0; index < subset.slots.size(); ++index)
      configuration[subset.slots[index] - 1] = values[index];
  }
  multiplySign(configuration, extraSign);
  return configuration;
}

Permutation normalizeConfiguration(
    const Permutation &configuration, const std::vector<LabelGroup> &groups,
    const std::vector<TotalSymmetrySubset> &subsets, int realDegree) {
  return normalizeTotalSubsets(
      normalizeLabelGroups(configuration, groups, realDegree), subsets);
}

Permutation unsignedKey(Permutation permutation) {
  if (signOf(permutation) < 0)
    multiplySign(permutation, -1);
  return permutation;
}

bool oppositeSigns(const std::vector<Permutation> &configurations) {
  std::map<Permutation, int> signs;
  for (const auto &configuration : configurations) {
    const auto [position, inserted] =
        signs.emplace(unsignedKey(configuration), signOf(configuration));
    if (!inserted && position->second != signOf(configuration))
      return true;
  }
  return false;
}

StabilizerChainState legacyChain(const LegacyCanonicalPermInput &input,
                                 const IntegerRow &selectedSlots) {
  PermutationBatch generators = input.slotGenerators;
  if (generators.empty())
    generators.push_back(identityPermutation(input.degree));
  StabilizerChainState chain;
  if (input.slotGeneratorsAreStrong) {
    chain.degree = input.degree;
    chain.base = input.base;
    chain.strongGenerators = generators;
    IntegerRow flatGenerators = flatten(chain.strongGenerators);
    StabilizerChain xpermChain;
    ::stab_chain(chain.base.data(), static_cast<int>(chain.base.size()),
                 flatGenerators.data(),
                 static_cast<int>(chain.strongGenerators.size()), input.degree,
                 xpermChain);
    ::basechange_chain(chain.base, flatGenerators, input.degree, xpermChain,
                       selectedSlots.data(),
                       static_cast<int>(selectedSlots.size()));
    chain.strongGenerators = rows(
        flatGenerators.data(),
        static_cast<int>(flatGenerators.size() / input.degree), input.degree);
    chain.generatorPositions.assign(xpermChain.begin(), xpermChain.end());
  } else {
    const BSGS group = makeBSGS(selectedSlots, generators);
    chain.degree = group.degree;
    chain.base = group.base;
    chain.strongGenerators = group.strongGenerators;
    IntegerRow flatGenerators = flatten(chain.strongGenerators);
    StabilizerChain xpermChain;
    ::stab_chain(chain.base.data(), static_cast<int>(chain.base.size()),
                 flatGenerators.data(),
                 static_cast<int>(chain.strongGenerators.size()), input.degree,
                 xpermChain);
    chain.generatorPositions.assign(xpermChain.begin(), xpermChain.end());
  }
  return chain;
}

LegacyCanonicalPermResult
canonicalizeCore(const LegacyCanonicalPermInput &input,
                 const std::vector<TotalSymmetrySubset> &subsets) {
  validateLegacyInput(input);
  const int realDegree = input.degree - 2;
  validateTotalSubsets(subsets, realDegree);
  const IntegerRow selectedSlots = realSearchBase(input);
  const auto groups = makeLegacyLabelGroups(input);
  StabilizerChainState chain = legacyChain(input, selectedSlots);
  std::vector<Permutation> configurations{
      normalizeConfiguration(input.permutation, groups, subsets, realDegree)};

  for (std::size_t level = 0; level < selectedSlots.size(); ++level) {
    chain.level = level;
    const BasicOrbit orbit = currentBasicOrbit(chain);
    std::unordered_map<int, Permutation> representatives;
    std::vector<Permutation> candidates;
    int minimum = input.degree + 1;
    const int selected = selectedSlots[level];
    for (const auto &configuration : configurations) {
      for (const int orbitPoint : orbit.points) {
        if (orbitPoint > realDegree)
          continue;
        auto [position, inserted] = representatives.try_emplace(orbitPoint);
        if (inserted)
          position->second =
              traceRepresentative(orbit, orbitPoint, input.degree);
        Permutation candidate =
            normalizeConfiguration(product(position->second, configuration),
                                   groups, subsets, realDegree);
        minimum = std::min(minimum, candidate[selected - 1]);
        candidates.push_back(std::move(candidate));
      }
    }
    configurations.clear();
    for (auto &candidate : candidates)
      if (candidate[selected - 1] == minimum)
        configurations.push_back(std::move(candidate));
    std::sort(configurations.begin(), configurations.end());
    configurations.erase(
        std::unique(configurations.begin(), configurations.end()),
        configurations.end());
    if (oppositeSigns(configurations))
      return {true, {}};
  }
  if (configurations.empty())
    throw std::runtime_error("canonical search produced no configuration");
  if (oppositeSigns(configurations))
    return {true, {}};
  return {false,
          *std::min_element(configurations.begin(), configurations.end())};
}

} // namespace

LegacyCanonicalPermResult
canonicalizeLegacyInput(const LegacyCanonicalPermInput &input) {
  return canonicalizeCore(input, {});
}

LegacyCanonicalPermResult
canonicalizeExtendedInput(const ExtendedCanonicalPermInput &input) {
  return canonicalizeCore(input.legacy, input.totalSymmetrySubsets);
}

} // namespace xperm::niehoff
