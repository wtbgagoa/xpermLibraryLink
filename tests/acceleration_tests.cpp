// Independent correctness and scaling checks for total-symmetry propagation.
// The oracle explicitly enumerates S*g*D; it does not use xPerm group helpers,
// canonical-label normalization, or the production pruning algorithm.
#include "NiehoffCanonicalizer.hpp"

#include <algorithm>
#include <chrono>
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
using Subsets = std::vector<n::TotalSymmetrySubset>;

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
P compose(const P &a, const P &b) {
	P result(a.size());
	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = b[static_cast<std::size_t>(a[i] - 1)];
	return result;
}
P transposition(int degree, int a, int b, int sign = 1) {
	P result = identity(degree);
	std::swap(result[a - 1], result[b - 1]);
	if (sign < 0)
		std::swap(result[degree - 2], result[degree - 1]);
	return result;
}
std::vector<P> closure(int degree, const std::vector<P> &generators) {
	std::set<P> seen{identity(degree)};
	std::vector<P> elements(seen.begin(), seen.end());
	for (std::size_t cursor = 0; cursor < elements.size(); ++cursor) {
		const P element = elements[cursor];
		for (const auto &generator : generators) {
			P next = compose(element, generator);
			if (seen.insert(next).second) {
				elements.push_back(std::move(next));
				if (elements.size() > 50000)
					throw std::runtime_error("oracle group exceeded its small-case budget");
			}
		}
	}
	return elements;
}
std::vector<P> labelGenerators(const Input &input) {
	std::vector<P> generators;
	std::size_t offset = 0;
	for (std::size_t group = 0; group < input.dummySetLengths.size(); ++group) {
		const int length = input.dummySetLengths[group];
		for (int j = 0; j < length; j += 2) {
			const int a = input.dummyLabels[offset + j];
			const int b = input.dummyLabels[offset + j + 1];
			if (input.metricSymmetries[group])
				generators.push_back(transposition(input.degree, a, b,
																						input.metricSymmetries[group]));
			if (j) {
				P exchange = identity(input.degree);
				std::swap(exchange[input.dummyLabels[offset + j - 2] - 1], exchange[a - 1]);
				std::swap(exchange[input.dummyLabels[offset + j - 1] - 1], exchange[b - 1]);
				generators.push_back(std::move(exchange));
			}
		}
		offset += static_cast<std::size_t>(length);
	}
	offset = 0;
	for (int length : input.repeatedSetLengths) {
		for (int j = 1; j < length; ++j)
			generators.push_back(transposition(input.degree,
					input.repeatedLabels[offset + j - 1], input.repeatedLabels[offset + j]));
		offset += static_cast<std::size_t>(length);
	}
	return generators;
}
n::LegacyCanonicalPermResult oracle(const Input &input, const Subsets &subsets) {
	auto generators = input.slotGenerators;
	for (const auto &subset : subsets)
		for (std::size_t i = 1; i < subset.slots.size(); ++i)
			generators.push_back(transposition(input.degree, subset.slots[i - 1],
																				 subset.slots[i], subset.sign));
	const auto slots = closure(input.degree, generators);
	const auto labels = closure(input.degree, labelGenerators(input));
	if (slots.size() * labels.size() > 1000000)
		throw std::runtime_error("oracle double coset exceeded its small-case budget");
	std::map<P, int> signs;
	P minimum;
	for (const auto &slot : slots) {
		const P left = compose(slot, input.permutation);
		for (const auto &label : labels) {
			P candidate = compose(left, label);
			const int sign = candidate[input.degree - 2] == input.degree - 1 ? 1 : -1;
			P key(candidate.begin(), candidate.end() - 2);
			const auto previous = signs.emplace(std::move(key), sign);
			if (!previous.second && previous.first->second != sign)
				return {true, {}};
			if (minimum.empty() || candidate < minimum)
				minimum = std::move(candidate);
		}
	}
	return {false, minimum};
}
std::string list(const P &values) {
	std::ostringstream output;
	for (int value : values)
		output << value << ',';
	return output.str();
}
void compare(const Input &input, const Subsets &subsets = {}) {
	const auto expected = oracle(input, subsets);
	const auto actual = subsets.empty() ? n::canonicalizeLegacyInput(input)
			: n::canonicalizeExtendedInput({input, subsets});
	++checks;
	if (actual.zero == expected.zero &&
			(actual.zero || actual.permutation == expected.permutation))
		return;
	std::ostringstream detail;
	detail << "double-coset mismatch, input=" << list(input.permutation)
				 << " expected=" << (expected.zero ? "zero" : list(expected.permutation))
				 << " actual=" << (actual.zero ? "zero" : list(actual.permutation))
				 << " dummyLengths=" << list(input.dummySetLengths)
				 << " dummies=" << list(input.dummyLabels)
				 << " metrics=" << list(input.metricSymmetries)
				 << " repeatedLengths=" << list(input.repeatedSetLengths)
				 << " repeated=" << list(input.repeatedLabels)
				 << " free=" << list(input.freeLabels) << " generators=";
	for (const auto &generator : input.slotGenerators)
		detail << '{' << list(generator) << '}';
	detail << " subsets=";
	for (const auto &subset : subsets)
		detail << '{' << list(subset.slots) << ':' << subset.sign << '}';
	throw std::runtime_error(detail.str());
}
void addBlock(Input &input, const P &slots, int sign) {
	for (std::size_t j = 1; j < slots.size(); ++j)
		input.slotGenerators.push_back(transposition(input.degree, slots[j - 1], slots[j], sign));
}
Input contraction(int rank, int leftSign = 1, int rightSign = 0, int metric = 1) {
	Input input;
	input.degree = 2 * rank + 2;
	input.permutation = identity(input.degree);
	for (int j = 0; j < rank; ++j) {
		input.permutation[j] = 2 * j + 1;
		input.permutation[rank + j] = 2 * j + 2;
	}
	input.dummySetLengths = {2 * rank};
	input.dummyLabels = identity(2 * rank);
	input.metricSymmetries = {metric};
	addBlock(input, identity(rank), leftSign);
	if (rightSign) {
		P right = identity(rank);
		for (int &slot : right)
			slot += rank;
		addBlock(input, right, rightSign);
	}
	return input;
}

void smallContractions() {
	// Exhaust every slot arrangement of rank-2 contractions, both signs and all
	// three metric characters. Mismatched tensor characters must cancel before
	// symmetry-equivalent instances can safely be discarded.
	for (int metric : {-1, 0, 1})
		for (int left : {-1, 1})
			for (int right : {-1, 0, 1}) {
				Input input = contraction(2, left, right, metric);
				P order = identity(4);
				do {
					std::copy(order.begin(), order.end(), input.permutation.begin());
					compare(input);
					std::swap(input.permutation[4], input.permutation[5]);
					compare(input);
					std::swap(input.permutation[4], input.permutation[5]);
				} while (std::next_permutation(order.begin(), order.end()));
			}

	std::mt19937 random(0x4e494548U);
	// Rank 3 makes propagation leave a nontrivial group after fixing the first
	// contracted endpoint. Random slot arrangements include self-contractions,
	// reversed pairs, and partners on both sides of a total subset.
	for (int metric : {-1, 0, 1})
		for (int left : {-1, 1})
			for (int right : {-1, 0, 1})
				for (int trial = 0; trial < 16; ++trial) {
					Input input = contraction(3, left, right, metric);
					std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
					if (trial % 2)
						std::swap(input.permutation[6], input.permutation[7]);
					// Pair order has no semantics, including for a skew metric.
					std::rotate(input.dummyLabels.begin(), input.dummyLabels.begin() + 2,
											input.dummyLabels.end());
					input.base = {6, 4, 2, 5, 3, 1};
					compare(input);
					compare(input, {{{3, 1, 2}, left}});
				}
}

void mixedClassesAndGeneratedSubsets() {
	std::mt19937 random(0x50524f50U);
	for (int trial = 0; trial < 300; ++trial) {
		Input input;
		input.degree = 8;
		input.permutation = identity(input.degree);
		std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
		if (trial % 2)
			std::swap(input.permutation[6], input.permutation[7]);
		const int metric = (trial % 3) - 1;
		switch (trial % 5) {
		case 0: // Repeated class plus dummy pair and two fixed free labels.
			input.repeatedSetLengths = {2}; input.repeatedLabels = {5, 2};
			input.dummySetLengths = {2}; input.dummyLabels = {4, 1};
			input.metricSymmetries = {metric}; input.freeLabels = {6, 3};
			break;
		case 1: // Distinct dummy classes must never acquire an exchange symmetry.
			input.dummySetLengths = {2, 4}; input.dummyLabels = {3, 6, 2, 5, 4, 1};
			input.metricSymmetries = {metric, -metric};
			break;
		case 2:
			input.dummySetLengths = {4}; input.dummyLabels = {4, 1, 6, 3};
			input.metricSymmetries = {metric};
			input.repeatedSetLengths = {2}; input.repeatedLabels = {5, 2};
			break;
		case 3:
			input.repeatedSetLengths = {3, 3}; input.repeatedLabels = {6, 2, 4, 1, 5, 3};
			break;
		default:
			input.dummySetLengths = {6}; input.dummyLabels = {6, 2, 4, 1, 5, 3};
			input.metricSymmetries = {metric};
			break;
		}
		P block{1, 2, 3};
		const int sign = trial % 2 ? -1 : 1;
		// Transposition and 3-cycle generate total symmetry but do not explicitly
		// supply all its adjacent transpositions. The cycle is even for either
		// character, which tests recognition independently of SGS presentation.
		input.slotGenerators.push_back(transposition(input.degree, 1, 2, sign));
		P cycle = identity(input.degree);
		cycle[0] = 2; cycle[1] = 3; cycle[2] = 1;
		input.slotGenerators.push_back(cycle);
		if (trial % 3 == 0)
			input.slotGenerators.push_back(transposition(input.degree, 4, 6, sign));
		compare(input);
		compare(input, {{{3, 2, 1}, sign}});

		// Relabel all symbols, including their label classes, independently of
		// the slot positions. This changes lexicographic order; the oracle must
		// recompute the answer, instead of assuming invariance under renaming.
		const P rename{4, 6, 2, 5, 1, 3, 7, 8};
		input.permutation = compose(input.permutation, rename);
		for (auto *labels : {&input.freeLabels, &input.dummyLabels, &input.repeatedLabels})
			for (int &label : *labels)
				label = rename[label - 1];
		compare(input);
	}

	// Total subsets can be interleaved with each other and with slots already
	// frozen by the lexicographic search. A subset need not be a contiguous
	// tensor block, and its contracted partners need not appear after it.
	for (int trial = 0; trial < 90; ++trial) {
		const int left = trial % 2 ? -1 : 1;
		const int right = trial % 3 - 1;
		Input input = contraction(3, left, 0, trial % 3 - 1);
		input.slotGenerators.clear();
		P positions = identity(6);
		std::shuffle(positions.begin(), positions.end(), random);
		P first(positions.begin(), positions.begin() + 3);
		P second(positions.begin() + 3, positions.end());
		addBlock(input, first, left);
		if (right)
			addBlock(input, second, right);
		std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
		compare(input);
		compare(input, {{first, left}});
	}

	// Products of identical factors add permutations of whole slot blocks.
	// Those permutations are not individual total symmetries: accidentally
	// treating their orbit as a totally symmetric subset changes the problem.
	for (int trial = 0; trial < 60; ++trial) {
		Input input = contraction(3, 1, 0, trial % 3 - 1);
		input.slotGenerators.clear();
		const int sign = trial % 2 ? -1 : 1;
		if (trial % 4)
			for (int j = 0; j < 3; ++j)
				addBlock(input, {2 * j + 1, 2 * j + 2}, sign);
		for (int j = 0; j < 2; ++j) {
			P exchange = identity(input.degree);
			std::swap(exchange[2 * j], exchange[2 * j + 2]);
			std::swap(exchange[2 * j + 1], exchange[2 * j + 3]);
			input.slotGenerators.push_back(std::move(exchange));
		}
		std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
		compare(input);
	}

	// Four dummy pairs exercise successive refinements beyond rank 3. Keeping
	// the second tensor generic bounds the independent double-coset oracle.
	for (int trial = 0; trial < 36; ++trial) {
		Input input = contraction(4, trial % 2 ? -1 : 1, 0, trial % 3 - 1);
		std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
		compare(input);
	}

	// A slot swap outside the declared subset can enlarge it by conjugation.
	// Pruning must use valid symmetries of the combined group, while preserving
	// sign conflicts from inconsistent declarations.
	for (int first : {-1, 1})
		for (int second : {-1, 1}) {
			Input input = contraction(3, first, 0, -1);
			input.slotGenerators = {transposition(input.degree, 2, 4, second)};
			compare(input, {{{1, 3, 2}, first}});
		}
}

void randomizedSignedGroups() {
	std::mt19937 random(0x5349474eU);
	for (int trial = 0; trial < 120; ++trial) {
		const int realDegree = trial % 3 ? 6 : 7;
		Input input;
		input.degree = realDegree + 2;
		input.permutation = identity(input.degree);
		std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
		if (trial % 2)
			std::swap(input.permutation[realDegree], input.permutation[realDegree + 1]);
		const int count = 1 + static_cast<int>(random() % 3);
		for (int j = 0; j < count; ++j) {
			P generator = identity(input.degree);
			std::shuffle(generator.begin(), generator.begin() + realDegree, random);
			int inversions = 0;
			for (int a = 0; a < realDegree; ++a)
				for (int b = a + 1; b < realDegree; ++b)
					inversions += generator[a] > generator[b];
			// Consistent trivial/sign characters avoid filling this corpus mostly
			// with groups containing negative identity and an immediate zero.
			if (trial % 2 && inversions % 2)
				std::swap(generator[realDegree], generator[realDegree + 1]);
			input.slotGenerators.push_back(std::move(generator));
		}
		P labels = identity(realDegree);
		std::shuffle(labels.begin(), labels.end(), random);
		if (trial % 4) {
			input.dummySetLengths = {6};
			input.dummyLabels.assign(labels.begin(), labels.begin() + 6);
			input.metricSymmetries = {trial % 3 - 1};
		} else {
			input.dummySetLengths = {4};
			input.dummyLabels.assign(labels.begin(), labels.begin() + 4);
			input.metricSymmetries = {trial % 3 - 1};
			input.repeatedSetLengths = {2};
			input.repeatedLabels.assign(labels.begin() + 4, labels.begin() + 6);
		}
		if (realDegree == 7)
			input.freeLabels = {labels[6]};
		input.base = identity(realDegree);
		std::shuffle(input.base.begin(), input.base.end(), random);
		compare(input);
	}
}

void interleavedPropagationRegression() {
	// A minimum-valued label whose partner is inside the same total subset is
	// not interchangeable with one whose partner is outside that subset.
	for (int character : {-1, 1}) {
		Input internal;
		internal.degree = 6;
		internal.permutation = {1, 3, 4, 2, 5, 6};
		internal.dummySetLengths = {4};
		internal.dummyLabels = {1, 2, 3, 4};
		internal.metricSymmetries = {character};
		addBlock(internal, {1, 2, 3}, character);
		compare(internal);
	}

	Input sparse;
	sparse.degree = 8;
	sparse.permutation = {1, 2, 3, 5, 6, 4, 7, 8};
	sparse.dummySetLengths = {6};
	sparse.dummyLabels = {1, 6, 2, 4, 3, 5};
	sparse.metricSymmetries = {1};
	addBlock(sparse, {1, 6}, 1);
	addBlock(sparse, {2, 3}, 1);
	compare(sparse);

	// All dummy pairs connect distinct subsets (or an unconstrained slot).
	// The repeated class is interleaved with dummy endpoints in the second
	// subset. A single global notion of "visited subset" can lose the least
	// completion when propagated classes evolve at different search levels.
	Input input;
	input.degree = 10;
	input.permutation = {8, 2, 1, 3, 5, 6, 7, 4, 10, 9};
	input.dummySetLengths = {6};
	input.dummyLabels = {1, 5, 2, 6, 4, 8};
	input.metricSymmetries = {1};
	input.repeatedSetLengths = {2};
	input.repeatedLabels = {3, 7};
	addBlock(input, {1, 6}, 1);
	addBlock(input, {2, 4, 7, 3}, 1);
	compare(input);
}

void overlappingPropagatedClasses() {
	std::mt19937 random(198713);
	for (int trial = 0; trial < 300; ++trial) {
		const int size = trial % 3 ? 8 : 6;
		Input input;
		input.degree = size + 2;
		input.permutation = identity(input.degree);
		std::shuffle(input.permutation.begin(), input.permutation.end() - 2, random);
		if (random() % 2)
			std::swap(input.permutation[size], input.permutation[size + 1]);
		P positions = identity(size);
		std::shuffle(positions.begin(), positions.end(), random);
		const int firstLength = 2 + static_cast<int>(random() % 3);
		const int secondLength = std::min(4, size - firstLength);
		const int firstSign = random() % 2 ? -1 : 1;
		const int secondSign = random() % 2 ? -1 : 1;
		P first(positions.begin(), positions.begin() + firstLength);
		P second(positions.begin() + firstLength,
						 positions.begin() + firstLength + secondLength);
		addBlock(input, first, firstSign);
		if (trial % 4)
			addBlock(input, second, secondSign);
		if (firstLength == secondLength && firstSign == secondSign && trial % 5 == 0) {
			P exchange = identity(input.degree);
			for (int j = 0; j < firstLength; ++j)
				std::swap(exchange[first[j] - 1], exchange[second[j] - 1]);
			input.slotGenerators.push_back(std::move(exchange));
		}
		P labels = identity(size);
		std::shuffle(labels.begin(), labels.end(), random);
		const int metric = static_cast<int>(random() % 3) - 1;
		switch (trial % 5) {
		case 0:
			input.dummySetLengths = {size}; input.dummyLabels = labels;
			input.metricSymmetries = {metric};
			break;
		case 1:
			input.dummySetLengths = {2, size - 2}; input.dummyLabels = labels;
			input.metricSymmetries = {metric, static_cast<int>(random() % 3) - 1};
			break;
		case 2:
		case 3:
			input.dummySetLengths = {size - 2};
			input.dummyLabels.assign(labels.begin(), labels.end() - 2);
			input.metricSymmetries = {metric};
			if (trial % 5 == 2) {
				input.repeatedSetLengths = {2};
				input.repeatedLabels.assign(labels.end() - 2, labels.end());
			} else {
				input.freeLabels.assign(labels.end() - 2, labels.end());
			}
			break;
		default:
			input.dummySetLengths = {2};
			input.dummyLabels.assign(labels.begin(), labels.begin() + 2);
			input.metricSymmetries = {metric};
			input.repeatedSetLengths = {size - 2};
			input.repeatedLabels.assign(labels.begin() + 2, labels.end());
			break;
		}
		input.base = positions;
		compare(input);
		if (trial % 10 == 0)
			compare(input, {{first, firstSign}});
	}
}

struct Observation {
	n::LegacyCanonicalPermResult result;
	std::size_t peak = 0;
	std::size_t candidates = 0;
	std::size_t pruned = 0;
	std::size_t propagated = 0;
	bool earlyZero = false;
};
Observation observed(const Input &input, bool extended) {
	Observation observation;
	n::ExtendedCanonicalPermInput extendedInput;
	if (extended) {
		extendedInput.legacy = input;
		extendedInput.totalSymmetrySubsets = {{identity((input.degree - 2) / 2), 1}};
	}
#ifdef XPERM_TEST_BASELINE
	observation.result = extended ? n::canonicalizeExtendedInput(extendedInput)
			: n::canonicalizeLegacyInput(input);
#else
	n::CanonicalizationStats stats;
	observation.result = extended ? n::canonicalizeExtendedInput(extendedInput, &stats)
			: n::canonicalizeLegacyInput(input, &stats);
	if (!stats.survivingConfigurations.empty())
		observation.peak = *std::max_element(stats.survivingConfigurations.begin(),
																				stats.survivingConfigurations.end());
	observation.candidates = stats.candidatesExpanded;
	observation.pruned = stats.symmetryEquivalentCandidatesPruned;
	observation.propagated = stats.propagatedGroupsCreated;
	observation.earlyZero = stats.earlyZeroDetected;
#endif
	return observation;
}
void scalingCase(int rank, int left, int right, int metric, bool extended,
								 bool print) {
	Input input = contraction(rank, left, right, metric);
	const auto start = std::chrono::steady_clock::now();
	const auto measured = observed(input, extended);
	const auto end = std::chrono::steady_clock::now();
	const bool zero = right && left != right && rank >= 2;
	require(measured.result.zero == zero &&
							(zero || measured.result.permutation == input.permutation),
					"wrong total-symmetry contraction result, rank=" + std::to_string(rank));
#ifndef XPERM_TEST_BASELINE
	// This directly excludes the old k! retained rows. Time limits alone could
	// miss a regression on small cases or falsely fail on a busy machine.
	require(measured.peak <= static_cast<std::size_t>(rank * rank),
					"factorial configurations returned, rank=" + std::to_string(rank));
	if (!zero && rank >= 3) {
		require(measured.pruned > 0, "total-symmetry instance pruning was not exercised");
		require(measured.propagated > 0, "dummy-partner propagation was not exercised");
	}
#endif
	if (print)
		std::cout << "rank=" << rank << " left=" << left << " right=" << right
							<< " metric=" << metric << " extended=" << extended
							<< " seconds=" << std::chrono::duration<double>(end - start).count()
#ifdef XPERM_TEST_BASELINE
							<< " stats=unavailable"
#else
							<< " peak=" << measured.peak << " candidates=" << measured.candidates
							<< " pruned=" << measured.pruned << " propagated=" << measured.propagated
							<< " early_zero=" << measured.earlyZero
#endif
							<< " zero=" << measured.result.zero << '\n';
}

void zeroBeforePruning() {
#ifndef XPERM_TEST_BASELINE
	for (int rank : {2, 3, 8, 14}) {
		Input repeated = contraction(rank, -1);
		repeated.dummySetLengths.clear();
		repeated.dummyLabels.clear();
		repeated.metricSymmetries.clear();
		repeated.repeatedSetLengths = {rank};
		for (int j = 0; j < rank; ++j) {
			repeated.repeatedLabels.push_back(2 * j + 1);
			repeated.freeLabels.push_back(2 * j + 2);
		}
		const auto repeatedResult = observed(repeated, false);
		require(repeatedResult.result.zero && repeatedResult.earlyZero,
						"antisymmetric repeated labels must cancel before pruning");

		for (int metric : {-1, 1}) {
			Input trace = contraction(rank, -metric, 0, metric);
			// Bring both endpoints of the first pair into the total subset. The
			// slot exchange and metric exchange then fix the real arrangement with
			// opposite characters, so its double coset contains both signs.
			std::swap(trace.permutation[1], trace.permutation[rank]);
			const auto traceResult = observed(trace, false);
			require(traceResult.result.zero && traceResult.earlyZero,
							"slot/metric character conflict must cancel before pruning");
		}
	}
#endif
}
void scaling(bool benchmark, int maximumRank, int repeats) {
	for (int rank = 2; rank <= maximumRank; ++rank)
		for (int repetition = 0; repetition < repeats; ++repetition) {
			for (int metric : {-1, 0, 1}) {
				scalingCase(rank, 1, 0, metric, false, benchmark);
				scalingCase(rank, -1, 0, metric, false, benchmark);
				scalingCase(rank, 1, 1, metric, false, benchmark);
				scalingCase(rank, -1, -1, metric, false, benchmark);
				scalingCase(rank, 1, -1, metric, false, benchmark);
			}
			scalingCase(rank, 1, 0, 1, true, benchmark);
		}
}
} // namespace

int main(int argc, char **argv) {
	try {
		if (argc > 1 && std::string(argv[1]) == "--benchmark") {
			const int maximumRank = argc > 2 ? std::stoi(argv[2]) : 7;
			const int repeats = argc > 3 ? std::stoi(argv[3]) : 1;
			if (maximumRank < 2 || maximumRank > 64 || repeats < 1 || repeats > 100)
				throw std::invalid_argument("use --benchmark [maximum rank 2..64] [repetitions 1..100]");
			scaling(true, maximumRank, repeats);
		} else {
			smallContractions();
			mixedClassesAndGeneratedSubsets();
			randomizedSignedGroups();
			interleavedPropagationRegression();
			overlappingPropagatedClasses();
			zeroBeforePruning();
#ifndef XPERM_TEST_BASELINE
			scaling(false, 14, 1);
#endif
		}
		std::cout << "Total-symmetry acceleration: " << checks << " checks passed\n";
	} catch (const std::exception &error) {
		std::cerr << "After " << checks << " checks: " << error.what() << '\n';
		return 1;
	}
	return 0;
}
