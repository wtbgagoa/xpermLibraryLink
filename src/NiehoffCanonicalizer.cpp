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

/**
 * @file NiehoffCanonicalizer.cpp
 * @brief Shared search utilities and the production Niehoff canonicalizer.
 *
 * The production route is canonicalizeCore() -> inferTotalSymmetrySubsets() ->
 * incrementalSearch(). It reuses xPerm's Schreier–Sims and orbit machinery but
 * performs its own signed double-coset search following Niehoff's
 * Algorithm 4.1, with the pruning of Algorithm 4.2 and bookkeeping of B.1–B.4.
 * The earlier staged search interfaces in this file remain independently
 * callable; Stage 6's explicit group closure is not part of that route. See
 * @ref algorithm_overview and @ref niehoff_algorithm.
 *
 * Array convention: a real point i has C++ index i-1, whereas label lookup
 * tables have an unused entry 0 and are indexed directly by the one-based
 * label. The two final permutation points encode sign. See @ref
 * representations.
 */

#include "NiehoffCanonicalizer.hpp"
#include "TotalSymmetry.hpp"
#include "xperm.h"

#include <algorithm>
#include <cassert>
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
void validateConfigurations(const std::vector<SearchConfiguration> &configs) {
	if (configs.empty())
		return;
	const auto degree = configs.front().slotPermutation.size();
	if (degree == 0)
		throw std::invalid_argument("empty configuration permutation");
	for (const auto &config : configs) {
		if (config.slotPermutation.size() != degree ||
				config.labelPermutation.size() != degree ||
				!isPermutation(config.slotPermutation) ||
				!isPermutation(config.labelPermutation) ||
				(config.sign != 1 && config.sign != -1))
			throw std::invalid_argument("invalid search configuration");
		std::set<int> fixed;
		for (int label : config.fixedLabels)
			if (label < 1 || static_cast<std::size_t>(label) > degree ||
					!fixed.insert(label).second)
				throw std::invalid_argument("invalid or repeated fixed label");
	}
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
	return std::tie(a.slotPermutation, a.labelPermutation, a.sign, a.metadata,
									a.fixedLabels) < std::tie(b.slotPermutation,
																						b.labelPermutation, b.sign,
																						b.metadata, b.fixedLabels);
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
	validateConfigurations(in.configurations);
	if (in.parentIndices.size() != in.slotTransversals.size() ||
			in.parentIndices.size() != in.labelTransversals.size())
		throw std::invalid_argument("candidate counts differ");
	std::vector<SearchConfiguration> out;
	for (std::size_t i = 0; i < in.parentIndices.size(); ++i) {
		if (in.parentIndices[i] >= in.configurations.size())
			throw std::invalid_argument("parent out of range");
		auto c = in.configurations[in.parentIndices[i]];
		c.slotPermutation = product(in.slotTransversals[i], c.slotPermutation);
		c.labelPermutation =
				product(product(in.slotTransversals[i], c.labelPermutation),
								in.labelTransversals[i]);
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
	validateConfigurations(c);
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
	validateBatch(p);
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
	return lessConfigStage2(a, b);
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
	if (n <= 0)
		throw std::invalid_argument("empty generator permutation");
	std::set<int> seenBase;
	for (int point : base)
		if (point < 1 || point > n || !seenBase.insert(point).second)
			throw std::invalid_argument("invalid or repeated base point");
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
	if (prefix > g.base.size())
		throw std::invalid_argument("prefix outside base");
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
	validateConfigurations(in.configurations);
	if (in.configurations.empty())
		return {};
	IntegerRow b = in.tentativeBase;
	b.erase(std::remove(b.begin(), b.end(), in.selectedSlot), b.end());
	b.insert(b.begin(), in.selectedSlot);
	auto g = makeBSGS(b, in.slotGenerators);
	if (in.configurations.front().slotPermutation.size() !=
			static_cast<std::size_t>(g.degree))
		throw std::invalid_argument("configuration and group degrees differ");
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
			const auto transversal = traceRepresentative(o, x, g.degree);
			c.slotPermutation = product(transversal, c.slotPermutation);
			c.labelPermutation = product(transversal, c.labelPermutation);
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
	return lessConfigStage2(a, b);
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
	std::set<int> selected;
	for (int point : selectedSlots)
		if (point < 1 || point > n || !selected.insert(point).second)
			throw std::invalid_argument("invalid or repeated selected slot");
	IntegerRow base = orderedBase(tentativeBase, selectedSlots);
	const auto group = makeBSGS(base, generators);
	StabilizerChainState state;
	state.degree = n;
	state.base = group.base;
	state.strongGenerators = group.strongGenerators;
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
	validateConfigurations(in.configurations);
	PersistentSearchResult out;
	if (in.configurations.empty())
		return out;
	out.configurations = in.configurations;
	out.group = initializeStabilizerChain(in.tentativeBase, in.slotGenerators,
																				in.selectedSlots);
	if (in.configurations.front().slotPermutation.size() !=
			static_cast<std::size_t>(out.group.degree))
		throw std::invalid_argument("configuration and group degrees differ");
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
			expanded.labelPermutation =
					product(it->second, expanded.labelPermutation);
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

/** @brief Exchange dummy pairs, optionally reversing the source orientation.
 *
 * A reversal of one pair followed by a pair exchange can be a four-cycle;
 * two cross-transpositions would reverse both pairs and miscount the character
 * of an antisymmetric metric. This helper constructs the unsigned label action.
 * Stage 5 records that character separately; production embeds it in sign
 * points via multiplySign(). See @ref design_choices.
 */
Permutation pairRepresentative(int sourceFirst, int sourceSecond,
															 int targetFirst, int targetSecond, bool reverse,
															 int degree) {
	Permutation representative = identityPermutation(degree);
	if (sourceFirst == targetFirst && sourceSecond == targetSecond) {
		if (reverse)
			std::swap(representative[sourceFirst - 1],
								representative[sourceSecond - 1]);
		return representative;
	}
	// Exchange the pairs, reversing ONLY the source pair when requested. Two
	// cross-transpositions would reverse both pairs and give the wrong metric
	// sign. A one-pair reversal combined with exchange is a four-cycle.
	representative[sourceFirst - 1] = reverse ? targetSecond : targetFirst;
	representative[sourceSecond - 1] = reverse ? targetFirst : targetSecond;
	representative[targetFirst - 1] = sourceFirst;
	representative[targetSecond - 1] = sourceSecond;
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
		if (group.type < LabelGroupType::Fixed ||
				group.type > LabelGroupType::Repeated)
			throw std::invalid_argument("invalid label group type");
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
	validateLabelGroups(groups, degree);
	std::set<int> fixed;
	for (int label : fixedLabels)
		if (label < 1 || label > degree || !fixed.insert(label).second)
			throw std::invalid_argument("invalid or repeated fixed label");
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
		if (containsLabel(fixedLabels, sourceFirst) ||
				containsLabel(fixedLabels, sourceSecond))
			return {{sourceLabel, identity, {sourceLabel}, 1}};
		for (std::size_t pair = 0; pair < group.labels.size() / 2; ++pair) {
			const int targetFirst = group.labels[2 * pair];
			const int targetSecond = group.labels[2 * pair + 1];
			if (containsLabel(fixedLabels, targetFirst) ||
					containsLabel(fixedLabels, targetSecond))
				continue;

			const bool targetIsSecond = group.type == LabelGroupType::Dummy
																			? sourceIsSecond
																			: targetSecond < targetFirst;
			const bool reverse = sourceIsSecond != targetIsSecond;
			const int mappedFirst = reverse ? targetSecond : targetFirst;
			const int mappedSecond = reverse ? targetFirst : targetSecond;
			const int canonicalLabel = sourceIsSecond ? mappedSecond : mappedFirst;
			const int sign =
					group.type == LabelGroupType::AntisymmetricMetric && reverse ? -1 : 1;
			candidates.push_back(
					{canonicalLabel,
					 pairRepresentative(sourceFirst, sourceSecond, targetFirst,
															targetSecond, reverse, degree),
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
	validateConfigurations(input.configurations);
	LabelGroupSearchResult result;
	if (input.configurations.empty())
		return result;
	const int degree =
			static_cast<int>(input.configurations.front().slotPermutation.size());
	validateLabelGroups(input.labelGroups, degree);

	result.configurations = input.configurations;
	result.group = initializeStabilizerChain(
			input.tentativeBase, input.slotGenerators, input.selectedSlots);
	if (result.group.degree != degree)
		throw std::invalid_argument("configuration and group degrees differ");

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
					product(product(position->second, expanded.labelPermutation),
									candidate.label.representative);
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
// Stage 6: bounded explicit signed-group reduction (not production propagation)
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

/** @brief Enumerate a supplied signed group for the standalone Stage 6 utility.
 *
 * Breadth-first closure is deliberately bounded by maximumOrbitSize; crossing
 * the limit throws rather than returning an incomplete equivalence relation.
 * Production PropagatedSymmetries uses class identifiers instead of closure
 * and never calls this function. See @ref staged_helpers.
 */
std::vector<SignedOrbitElement>
signedGroupClosure(const std::vector<SignedPermutation> &generators, int degree,
									 std::size_t maximumOrbitSize) {
	if (maximumOrbitSize == 0)
		throw std::invalid_argument("signed-group closure limit must be positive");
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
			if (visited.insert({next.permutation, next.sign}).second) {
				if (visited.size() > maximumOrbitSize)
					throw std::runtime_error(
							"propagated-symmetry orbit exceeded the configured limit");
				pending.push(std::move(next));
			}
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
	validateConfigurations(configurations);
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
			transformed.labelPermutation =
					product(symmetry.permutation, configuration.labelPermutation);
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

/** @brief Check the signed layout before any low-level xPerm operation.
 *
 * Checks permutation validity, sign-point separation, base uniqueness and
 * flattened pool lengths. Label-pool disjointness and coverage are checked by
 * makeLegacyLabelGroups(). The caller's assertion that an SGS is strong is a
 * precondition, not something this validator proves.
 */
void validateLegacyInput(const LegacyCanonicalPermInput &input) {
	if (input.degree < 2 ||
			static_cast<int>(input.permutation.size()) != input.degree ||
			!isPermutation(input.permutation))
		throw std::invalid_argument("invalid signed input permutation");
	auto validSignPoints = [&](const Permutation &p) {
		return p[input.degree - 2] >= input.degree - 1 &&
					 p[input.degree - 1] >= input.degree - 1;
	};
	if (!validSignPoints(input.permutation))
		throw std::invalid_argument("input moves sign points into real slots");
	std::set<int> basePoints;
	for (int point : input.base)
		if (point < 1 || point > input.degree || !basePoints.insert(point).second)
			throw std::invalid_argument("invalid or repeated base point");
	if (input.dummySetLengths.size() != input.metricSymmetries.size())
		throw std::invalid_argument("dummy-set and metric counts differ");
	auto validateLengths = [](const IntegerRow &lengths, std::size_t size,
														bool paired) {
		std::size_t count = 0;
		for (int length : lengths) {
			if (length <= 0 || (paired && length % 2 != 0) ||
					static_cast<std::size_t>(length) > size - count)
				throw std::invalid_argument("invalid legacy set length");
			count += static_cast<std::size_t>(length);
		}
		if (count != size)
			throw std::invalid_argument("legacy set lengths do not match their data");
	};
	validateLengths(input.dummySetLengths, input.dummyLabels.size(), true);
	validateLengths(input.repeatedSetLengths, input.repeatedLabels.size(), false);
	for (const auto &generator : input.slotGenerators)
		if (static_cast<int>(generator.size()) != input.degree ||
				!isPermutation(generator) || !validSignPoints(generator))
			throw std::invalid_argument("invalid slot generator");
}

/** @brief Select real slots 1..r in canonical comparison order.
 *
 * A BSGS base is a group representation choice; it must not silently change the
 * output ordering. Production therefore rebases to natural slot order even if
 * the caller's valid SGS was supplied with another base. This is why comparison
 * with the old Butler–Portugal backend must allow different representatives of
 * the same signed double coset. See @ref representations.
 */
IntegerRow realSearchBase(const LegacyCanonicalPermInput &input) {
	const int realDegree = input.degree - 2;
	// The base is a group representation detail. The canonical ordering must
	// agree with the natural slot order used by label normalization.
	IntegerRow result(static_cast<std::size_t>(realDegree));
	std::iota(result.begin(), result.end(), 1);
	return result;
}

} // namespace

std::vector<LabelGroup>
makeLegacyLabelGroups(const LegacyCanonicalPermInput &input) {
	validateLegacyInput(input);
	std::vector<LabelGroup> groups;
	std::vector<bool> covered(static_cast<std::size_t>(input.degree + 1), false);

	for (const int label : input.freeLabels) {
		if (label < 1 || label > input.degree - 2 || covered[label])
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
			if (label < 1 || label > input.degree - 2 || covered[label])
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
			if (label < 1 || label > input.degree - 2 || covered[label])
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

/// Decode the final two points of a signed permutation; reject other layouts.
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

/** @brief Multiply a signed permutation's character without moving real labels.
 * @pre sign is +1 or -1 and permutation has valid separated sign points.
 */
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

/// Require disjoint subsets of at least two real slots, each with character ±1.
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

/** @brief Prepare the slot group's SGS in the production search order.
 * @details
 * @anchor niehoff_chain
 *
 * A trusted strong input is rebased with xPerm's basechange_chain; an arbitrary
 * generating set goes through Schreier–Sims. The identity supplies the trivial
 * group when the input generator list is empty. The returned chain contains
 * all requested real points before residual sign points, exposing negative
 * identity in the final real-point stabilizer when it is present.
 *
 * This is shared computational group theory with Butler–Portugal, not a second
 * independent implementation of Schreier–Sims. No full slot-group closure is
 * constructed. See @ref algorithm_overview.
 */
StabilizerChainState legacyChain(const LegacyCanonicalPermInput &input,
																 const IntegerRow &selectedSlots) {
	PermutationBatch generators = input.slotGenerators;
	if (generators.empty())
		generators.push_back(identityPermutation(input.degree));
	StabilizerChainState chain;
	if (input.slotGeneratorsAreStrong && !input.base.empty()) {
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

/**
 * @brief Production label stabilizer represented by Values and explicit pair
 * tables.
 * @ingroup niehoff_api
 * @details
 * @anchor niehoff_incremental_labels
 *
 * This realizes the Values/Label-Codes bookkeeping of Algorithm 4.1 and B.4,
 * without generating the label group. `value[a]` is the least label reachable
 * from a under the current label stabilizer. All surviving configurations share
 * the selected prefix, so one instance serves the whole search frontier.
 *
 * Unlike the paper's convenient consecutive-label coding, the public API
 * permits descending, sparse and interleaved pair entries. partner and second
 * preserve declared pairing and variance explicitly. Original type codes stay
 * immutable for later sign checks; the fixed mask records the changing
 * stabilizer instead of rewriting those codes. Fixing one endpoint fixes its
 * partner as well. See @ref representations and @ref design_choices.
 */
class IncrementalLabels {
	const std::vector<LabelGroup>
			&groups; ///< Valid disjoint pools covering the degree.
	int degree;  ///< Full signed permutation degree, including two sign points.
	std::vector<unsigned char> second,
			fixed; ///< Declared pair position and fixed-label mask.

public:
	IntegerRow value,
			partner; ///< Least targets and dummy partners, indexed by label.
	std::vector<LabelGroupType>
			type; ///< Immutable original pool type of each label.

	/// Compile pairing/variance lookup tables once and initialize the full label
	/// orbits.
	IncrementalLabels(const std::vector<LabelGroup> &input, int n)
			: groups(input), degree(n), second(n + 1), fixed(n + 1), value(n + 1),
				partner(n + 1), type(n + 1, LabelGroupType::Fixed) {
		for (std::size_t index = 0; index < groups.size(); ++index) {
			const auto &group = groups[index];
			for (int label : group.labels) {
				type[label] = group.type;
				fixed[label] = group.type == LabelGroupType::Fixed ||
											 group.type == LabelGroupType::Free;
			}
			if (group.type == LabelGroupType::Dummy ||
					group.type == LabelGroupType::SymmetricMetric ||
					group.type == LabelGroupType::AntisymmetricMetric) {
				for (std::size_t j = 0; j < group.labels.size(); j += 2) {
					const int a = group.labels[j], b = group.labels[j + 1];
					partner[a] = b;
					partner[b] = a;
					second[b] = 1;
				}
			}
		}
		updateValues();
	}

	/// Whether a label is still available for relabeling and new source
	/// propagation.
	bool active(int label) const { return !fixed[label]; }

	/** @brief Recompute Values from the remaining labels in each pool (B.4).
	 * @details
	 * @anchor niehoff_update_values
	 * No-metric pools have two variance orbits. Metric/repeated pools have one;
	 * already fixed labels retain their own values. The scan is linear in the
	 * total number of declared labels and avoids explicit label-group elements.
	 */
	void updateValues() {
		std::iota(value.begin(), value.end(), 0);
		for (const auto &group : groups) {
			int least[2] = {degree + 1, degree + 1};
			for (int label : group.labels)
				if (!fixed[label]) {
					const int variance =
							group.type == LabelGroupType::Dummy ? second[label] : 0;
					least[variance] = std::min(least[variance], label);
				}
			for (int label : group.labels)
				if (!fixed[label])
					value[label] =
							least[group.type == LabelGroupType::Dummy ? second[label] : 0];
		}
	}

	/** @brief Advance the label stabilizer after fixing one slot's common minimum
	 * (B.4).
	 * @details
	 * @anchor niehoff_freeze_label
	 */
	void freeze(int label) {
		fixed[label] = 1;
		if (partner[label])
			fixed[partner[label]] = 1;
		updateValues();
	}

	/** @brief Construct one signed label action taking source to its current
	 * Value.
	 * @details
	 * @anchor niehoff_label_representative
	 *
	 * A repeated label uses a transposition; a dummy uses pair exchange and any
	 * permitted orientation reversal. For an antisymmetric metric only, a
	 * reversal exchanges the sign points. This is an ordinary label-group action;
	 * the extra even-class propagated exchange is applied by incrementalSearch().
	 */
	Permutation representative(int source) const {
		const int target = value[source];
		if (target == source)
			return identityPermutation(degree);
		if (!partner[source])
			return transpositionRepresentative(source, target, degree);
		const int first = second[source] ? partner[source] : source;
		const int last = partner[first];
		const int targetFirst = second[target] ? partner[target] : target;
		const bool reverse = second[source] != second[target];
		auto result = pairRepresentative(first, last, targetFirst,
																		 partner[targetFirst], reverse, degree);
		if (type[source] == LabelGroupType::AntisymmetricMetric && reverse)
			multiplySign(result, -1);
		return result;
	}
};

/**
 * @brief Production pair `(g,s)` retaining the provenance of each current slot.
 * @ingroup niehoff_api
 * @details
 * @anchor niehoff_configuration
 *
 * labels is the signed current configuration. slots records the accumulated
 * slot action so that propagation entries remain attached to original slots.
 * A slot representative changes both arrays; a label action changes only
 * labels. Deduplication uses labels, but retains its matching whole pair.
 */
struct PropagatedConfiguration {
	Permutation labels; ///< Current slot -> label map g, including sign points.
	Permutation slots;  ///< Current slot -> original slot map s.
};

/** @brief A least-value occurrence from B.1. @ingroup niehoff_api
 * @details
 * @anchor niehoff_least_instance
 * p is reachable by a slot representative. q is p itself, or a lower-valued
 * position reachable through p's even propagated class, possibly outside the
 * slot orbit. All entries in a level's list attain the same global Value.
 */
struct LeastInstance {
	std::size_t parent; ///< Zero-based configuration index.
	int p, q; ///< One-based current positions: slot-orbit point and label source.
};

/**
 * @brief Incremental signed propagation classes, implementing B.2 and B.3.
 * @ingroup niehoff_api
 * @details
 * @anchor niehoff_propagated_symmetries
 *
 * A signed odd ID marks a source total subset; the corresponding even ID marks
 * the exchange transported to its contracted partners. Zero means that there
 * is no recorded certificate. Entries use original-slot coordinates and are
 * read at `s[p]`; they are not indexed directly by the changing label g[p].
 * The Symmetric-Subsets lookup instead describes current slot positions.
 *
 * Existing entries take precedence. Unmarked sources are processed in slot
 * order, and consumed singletons cannot be reused to invent a later symmetry.
 * At least two source occurrences are needed before a class is installed.
 * These rules make provenance persistent across levels and parents, without
 * enumerating a propagated group. See @ref niehoff_algorithm.
 */
class PropagatedSymmetries {
	IntegerRow entries; ///< Signed odd/even IDs indexed by original slot, entry 0
											///< unused.
	std::vector<unsigned char>
			consumed; ///< Original sources already examined, including singletons.
	int nextOdd =
			1; ///< Next unused positive source ID; the partner ID is nextOdd+1.

public:
	/// Initialize an empty propagation certificate for one complete search.
	explicit PropagatedSymmetries(int n) : entries(n + 1), consumed(n + 1) {}
	/// Pull back the original-slot entry through a configuration's accumulated
	/// action s.
	int at(const PropagatedConfiguration &configuration, int slot) const {
		return entries[configuration.slots[slot - 1]];
	}

	/** @brief Extend propagation from one parent's least instances (B.2).
	 * @details
	 * @anchor niehoff_update_propagation
	 *
	 * Sources must be active labels in the same nonzero total subset and attain
	 * the common least Value. That Value identifies one label orbit, including
	 * variance for no-metric pairs. Entries are installed for all sources before
	 * their dummy partners: a contraction internal to the source keeps two odd
	 * entries and can expose a metric sign conflict in zero().
	 *
	 * @param configuration Parent pair `(g,s)` for these least instances.
	 * @param instances Level's globally least occurrences, grouped by parent.
	 * @param begin Inclusive zero-based start for this parent.
	 * @param end Exclusive zero-based end for this parent.
	 * @param labels Current Values, types and explicit pair data.
	 * @param subsets Signed total-subset ID by one-based current slot.
	 * @param statistics Optional count of allocated non-singleton class pairs.
	 */
	void update(const PropagatedConfiguration &configuration,
							const std::vector<LeastInstance> &instances, std::size_t begin,
							std::size_t end, const IncrementalLabels &labels,
							const IntegerRow &subsets, CanonicalizationStats *statistics) {
		IntegerRow inverse(configuration.labels.size() + 1);
		for (std::size_t i = 0; i < configuration.labels.size(); ++i)
			inverse[configuration.labels[i]] = static_cast<int>(i + 1);
		std::map<int, IntegerRow> members;
		for (std::size_t j = begin; j < end; ++j) {
			const int q = instances[j].q;
			if (subsets[q] && labels.active(configuration.labels[q - 1]))
				members[subsets[q]].push_back(q);
		}
		std::vector<int> subsetOrder;
		for (const auto &[subset, slots] : members)
			subsetOrder.push_back(subset);
		std::sort(subsetOrder.begin(), subsetOrder.end(), [&](int a, int b) {
			return *std::min_element(members[a].begin(), members[a].end()) <
						 *std::min_element(members[b].begin(), members[b].end());
		});
		for (int subset : subsetOrder) {
			auto &slots = members[subset];
			std::sort(slots.begin(), slots.end());
			slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
			slots.erase(
					std::remove_if(slots.begin(), slots.end(),
												 [&](int q) {
													 const int original = configuration.slots[q - 1];
													 return entries[original] != 0 || consumed[original];
												 }),
					slots.end());
			for (int q : slots)
				consumed[configuration.slots[q - 1]] = 1;
			if (slots.size() < 2)
				continue;
			const int sign = subset < 0 ? -1 : 1;
			const int source = sign * nextOdd, target = sign * (nextOdd + 1);
			nextOdd += 2;
			if (statistics)
				++statistics->propagatedGroupsCreated;
			// All members have the same least Value, hence the same label orbit
			// (including variance when there is no metric). Install sources together
			// before partners: an internal contraction
			// keeps its odd entry and can then trigger the metric sign check.
			for (int q : slots)
				entries[configuration.slots[q - 1]] = source;
			for (int q : slots) {
				const int partner = labels.partner[configuration.labels[q - 1]];
				if (!partner)
					continue;
				const int original = configuration.slots[inverse[partner] - 1];
				if (!entries[original])
					entries[original] = target;
			}
		}
	}

	/** @brief Detect signed cancellation before dropping equivalent branches
	 * (B.3).
	 * @details
	 * @anchor niehoff_zero_check
	 *
	 * Three certificates are checked: an antisymmetric source acting on equal
	 * component occurrences; two equal even-class entries in an opposite-sign
	 * nonzero total subset; and a dummy pair internal to a source whose character
	 * conflicts with the metric. A lone partner in an unrelated subset cannot
	 * certify zero, so the second case explicitly requires two occurrences.
	 *
	 * Debug builds also assert the pairing invariant behind pruning: endpoints
	 * have entries `(0,0)`, `(odd,odd)` or `(odd,matching even)`, with matching
	 * signs and either endpoint order. Ordinary pair relabeling, slot motion and
	 * even-class label exchanges preserve this provenance.
	 *
	 * Negative identity in the slot group and opposite signed surviving
	 * configurations are separate checks outside this method.
	 */
	bool zero(const PropagatedConfiguration &configuration,
						const IncrementalLabels &labels, const IntegerRow &subsets,
						int realDegree) const {
		IntegerRow labelSymmetry(configuration.labels.size() + 1);
		std::set<std::pair<int, int>> seen;
		for (int p = 1; p <= realDegree; ++p) {
			const int symmetry = at(configuration, p);
			if (!symmetry)
				continue;
			const int label = configuration.labels[p - 1];
			labelSymmetry[label] = symmetry;
			if (labels.type[label] == LabelGroupType::Repeated && symmetry < 0)
				return true;
			if (std::abs(symmetry) % 2 == 0 && subsets[p] &&
					(symmetry < 0) != (subsets[p] < 0) &&
					!seen.emplace(symmetry, subsets[p]).second)
				return true;
		}
		for (int label = 1; label <= realDegree; ++label) {
			const int symmetry = labelSymmetry[label];
			// A dummy's endpoints keep their source/partner provenance even after
			// an even-class label swap. This is the certificate used by pruning.
#ifndef NDEBUG
			if (labels.partner[label]) {
				const int other = labelSymmetry[labels.partner[label]];
				const int sign = symmetry < 0 ? -1 : 1;
				assert((symmetry == 0 && other == 0) ||
							 (symmetry != 0 &&
								((std::abs(symmetry) % 2 &&
									(other == symmetry || other == symmetry + sign)) ||
								 (std::abs(symmetry) % 2 == 0 && other == symmetry - sign))));
			}
#endif
			if (symmetry && labels.partner[label] &&
					labelSymmetry[labels.partner[label]] == symmetry &&
					((labels.type[label] == LabelGroupType::SymmetricMetric &&
						symmetry < 0) ||
					 (labels.type[label] == LabelGroupType::AntisymmetricMetric &&
						symmetry > 0)))
				return true;
		}
		return false;
	}
};

/**
 * @brief Production Algorithm 4.1, with 4.2 pruning and B.1–B.4 bookkeeping.
 * @ingroup niehoff_api
 * @details
 * @anchor niehoff_search
 *
 * For each natural real-slot position, B.1 finds the global least Value among
 * its slot orbit and any reachable even propagated classes. B.2 extends the
 * certificate; B.3 checks signs before pruning. Certified representatives are
 * constructed, then B.4 freezes the selected label and its partner. Equal g
 * values are deduplicated with a matching s; opposite signs of the same real
 * configuration yield zero. The canonical prefix is never moved afterwards.
 *
 * Algorithm 4.2's slot-subset pruning is refined to respect this API's
 * arbitrary numeric labels and overlapping contractions: the certificate key
 * also needs the propagated class and whether the partner is internal to the
 * subset. A zero entry is not a certificate. This preserves distinct
 * lexicographic choices that literal subset-only pruning loses; see @ref
 * design_choices.
 *
 * Class minima and slot/label representatives are cached within a level.
 * This removes the factorial equivalent frontier for the central total-tensor
 * contraction family, but does not bound all groups polynomially. Unrelated
 * inequivalent configurations may still proliferate; BSGS construction and
 * total-subset inference also have their own costs. See @ref validation.
 *
 * @param input Valid signed tensor input.
 * @param groups Validated disjoint label pools.
 * @param chain Complete slot chain with natural real-slot base prefix.
 * @param subsets Signed total-subset lookup inferred from that same group.
 * @param statistics Optional work counters, already reset by
 * canonicalizeCore().
 */
LegacyCanonicalPermResult
incrementalSearch(const LegacyCanonicalPermInput &input,
									const std::vector<LabelGroup> &groups,
									StabilizerChainState chain, const IntegerRow &subsets,
									CanonicalizationStats *statistics) {
	const int realDegree = input.degree - 2;
	IncrementalLabels labels(groups, input.degree);
	PropagatedSymmetries propagated(input.degree);
	std::vector<PropagatedConfiguration> configurations{
			{input.permutation, identityPermutation(input.degree)}};
	for (int selected = 1; selected <= realDegree; ++selected) {
		chain.level = static_cast<std::size_t>(selected - 1);
		const BasicOrbit orbit = currentBasicOrbit(chain);
		int minimum = input.degree + 1;
		std::vector<LeastInstance> instances;
		// B.1: cache the least remaining position in each even class. Such a class
		// supplies a signed exchange bringing a label from outside this slot orbit.
		for (std::size_t parent = 0; parent < configurations.size(); ++parent) {
			const auto &configuration = configurations[parent];
			std::unordered_map<int, int> leastInClass;
			for (int k = selected; k <= realDegree; ++k) {
				const int symmetry = propagated.at(configuration, k);
				if (!symmetry || std::abs(symmetry) % 2)
					continue;
				auto [position, inserted] = leastInClass.emplace(symmetry, k);
				if (!inserted &&
						labels.value[configuration.labels[k - 1]] <
								labels.value[configuration.labels[position->second - 1]])
					position->second = k;
			}
			for (int p : orbit.points) {
				if (p > realDegree)
					continue;
				int q = p;
				const auto found = leastInClass.find(propagated.at(configuration, p));
				if (found != leastInClass.end() &&
						labels.value[configuration.labels[found->second - 1]] <
								labels.value[configuration.labels[p - 1]])
					q = found->second;
				const int value = labels.value[configuration.labels[q - 1]];
				if (value < minimum) {
					minimum = value;
					instances.clear();
				}
				if (value == minimum)
					instances.push_back({parent, p, q});
			}
		}
		if (instances.empty())
			throw std::runtime_error(
					"incremental canonical search has no least instance");
		std::unordered_map<int, Permutation> slotRepresentatives,
				labelRepresentatives;
		std::vector<PropagatedConfiguration> survivors;
		for (std::size_t begin = 0; begin < instances.size();) {
			std::size_t end = begin + 1;
			while (end < instances.size() &&
						 instances[end].parent == instances[begin].parent)
				++end;
			const auto &configuration = configurations[instances[begin].parent];
			propagated.update(configuration, instances, begin, end, labels, subsets,
												statistics);
			if (statistics)
				++statistics->earlyZeroChecks;
			if (propagated.zero(configuration, labels, subsets, realDegree)) {
				if (statistics)
					statistics->earlyZeroDetected = true;
				return {true, {}};
			}
			IntegerRow inverse(input.degree + 1);
			for (int p = 1; p <= realDegree; ++p)
				inverse[configuration.labels[p - 1]] = p;
			std::set<std::tuple<int, int, bool>> visitedSubsets;
			for (std::size_t j = begin; j < end; ++j) {
				const auto &instance = instances[j];
				// Algorithm 4.2, refined for the API's arbitrary label order and
				// overlapping contractions. Only a recorded common propagated class
				// certifies equivalence. An internal pair and an external endpoint
				// can have different earliest partner positions, so keep both choices.
				// In particular, never merge a pre-existing even class with a new odd
				// class (or an unmarked occurrence) merely because their slots agree.
				const int candidateLabel = configuration.labels[instance.q - 1];
				const int partner = labels.partner[candidateLabel];
				const bool internal =
						partner && subsets[inverse[partner]] == subsets[instance.p];
				if (subsets[instance.p] && propagated.at(configuration, instance.p) &&
						!visitedSubsets
								 .emplace(subsets[instance.p],
													propagated.at(configuration, instance.p), internal)
								 .second) {
					if (statistics)
						++statistics->symmetryEquivalentCandidatesPruned;
					continue;
				}
				if (statistics)
					++statistics->candidatesExpanded;
				auto [slotPosition, slotInserted] =
						slotRepresentatives.try_emplace(instance.p);
				if (slotInserted)
					slotPosition->second =
							traceRepresentative(orbit, instance.p, input.degree);
				const int source = configuration.labels[instance.q - 1];
				const int orbitLabel = configuration.labels[instance.p - 1];
				auto [labelPosition, labelInserted] =
						labelRepresentatives.try_emplace(source);
				if (labelInserted)
					labelPosition->second = labels.representative(source);
				// In left-to-right product notation, the new g is t*g*e*d:
				// t sends selected -> p, e exchanges g[p] and g[q] when needed, and
				// d sends the original source g[q] to the common minimum. Thus the
				// selected position gets minimum while earlier positions stay fixed.
				// In ordinary function notation this is d composed with e, g and t.
				const auto &slotAction = slotPosition->second;
				const auto &labelAction = labelPosition->second;
				PropagatedConfiguration candidate{Permutation(input.degree),
																					Permutation(input.degree)};
				for (int k = 0; k < input.degree; ++k) {
					int label = configuration.labels[slotAction[k] - 1];
					if (instance.p != instance.q) {
						if (label == source)
							label = orbitLabel;
						else if (label == orbitLabel)
							label = source;
					}
					candidate.labels[k] = labelAction[label - 1];
					candidate.slots[k] = configuration.slots[slotAction[k] - 1];
				}
				if (instance.p != instance.q &&
						propagated.at(configuration, instance.p) < 0)
					multiplySign(candidate.labels, -1);
				survivors.push_back(std::move(candidate));
			}
			begin = end;
		}
		// B.4: every surviving candidate shares this minimum, so its label
		// stabilizer can be advanced once for the whole frontier.
		labels.freeze(minimum);
		std::sort(
				survivors.begin(), survivors.end(), [](const auto &a, const auto &b) {
					return std::tie(a.labels, a.slots) < std::tie(b.labels, b.slots);
				});
		// Only g determines the canonical candidate, but keep its corresponding s:
		// propagation lookups in subsequent levels still need that provenance.
		survivors.erase(std::unique(survivors.begin(), survivors.end(),
																[](const auto &a, const auto &b) {
																	return a.labels == b.labels;
																}),
										survivors.end());
		for (std::size_t j = 1; j < survivors.size(); ++j)
			if (std::equal(survivors[j - 1].labels.begin(),
										 survivors[j - 1].labels.end() - 2,
										 survivors[j].labels.begin()) &&
					survivors[j - 1].labels[realDegree] !=
							survivors[j].labels[realDegree])
				return {true, {}};
		configurations = std::move(survivors);
		if (statistics)
			statistics->survivingConfigurations.push_back(configurations.size());
	}
	if (configurations.empty())
		throw std::runtime_error(
				"incremental canonical search produced no configuration");
	return {false, configurations.front().labels};
}

/**
 * @brief Common checked entry to the production algorithm.
 * @ingroup niehoff_api
 * @details
 * @anchor niehoff_core
 *
 * Validate the legacy representation, add any genuinely declared slot
 * relations, construct/rebase the SGS, and exclude negative identity before
 * inference and search. Explicit subsets are generated by adjacent signed
 * transpositions, so every later transversal respects the fixed base prefix;
 * sorting an entire subset after a step would violate that invariant.
 *
 * Both public APIs take this path. Inference uses the augmented group, while
 * the original configuration and label pools enter incrementalSearch().
 * Statistics are reset here and do not include inference/BSGS work.
 */
LegacyCanonicalPermResult
canonicalizeCore(const LegacyCanonicalPermInput &input,
								 const std::vector<TotalSymmetrySubset> &subsets,
								 CanonicalizationStats *statistics) {
	if (statistics)
		*statistics = {};
	const auto groups = makeLegacyLabelGroups(input); // validates the full input
	const int realDegree = input.degree - 2;
	validateTotalSubsets(subsets, realDegree);
	const IntegerRow selectedSlots = realSearchBase(input);
	// Subsets declare additional slot symmetries. Include their signed adjacent
	// transpositions in the group so that every search step fixes its prefix.
	// Sorting an entire subset after a step could move previously frozen slots.
	LegacyCanonicalPermInput augmented = input;
	for (const auto &subset : subsets) {
		for (std::size_t index = 1; index < subset.slots.size(); ++index) {
			auto generator = identityPermutation(input.degree);
			std::swap(generator[subset.slots[index - 1] - 1],
								generator[subset.slots[index] - 1]);
			multiplySign(generator, subset.sign);
			augmented.slotGenerators.push_back(std::move(generator));
		}
	}
	if (!subsets.empty())
		augmented.slotGeneratorsAreStrong = false;
	auto isNegativeIdentity = [realDegree](const Permutation &generator) {
		for (int slot = 0; slot < realDegree; ++slot)
			if (generator[slot] != slot + 1)
				return false;
		return signOf(generator) < 0;
	};
	if (std::any_of(augmented.slotGenerators.begin(),
									augmented.slotGenerators.end(), isNegativeIdentity))
		return {true, {}};
	StabilizerChainState chain = legacyChain(augmented, selectedSlots);
	// A signed group may contain -identity, including as a product of generators.
	// After all real slots enter the base, its SGS must expose that stabilizer.
	if (std::any_of(chain.strongGenerators.begin(), chain.strongGenerators.end(),
									isNegativeIdentity))
		return {true, {}};
	const auto totalSymmetry = inferTotalSymmetrySubsets(chain);
	if (totalSymmetry.zero)
		return {true, {}};
	return incrementalSearch(input, groups, std::move(chain),
													 totalSymmetry.subsetOfSlot, statistics);
}

} // namespace

LegacyCanonicalPermResult
canonicalizeLegacyInput(const LegacyCanonicalPermInput &input) {
	return canonicalizeCore(input, {}, nullptr);
}

LegacyCanonicalPermResult
canonicalizeExtendedInput(const ExtendedCanonicalPermInput &input) {
	return canonicalizeCore(input.legacy, input.totalSymmetrySubsets, nullptr);
}

LegacyCanonicalPermResult
canonicalizeLegacyInput(const LegacyCanonicalPermInput &input,
												CanonicalizationStats *statistics) {
	return canonicalizeCore(input, {}, statistics);
}

LegacyCanonicalPermResult
canonicalizeExtendedInput(const ExtendedCanonicalPermInput &input,
													CanonicalizationStats *statistics) {
	return canonicalizeCore(input.legacy, input.totalSymmetrySubsets, statistics);
}

} // namespace xperm::niehoff
