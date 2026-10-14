// Companion to docs/niehoff.md: propagation from a rank-three source tensor.
#include "NiehoffCanonicalizer.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace n = xperm::niehoff;

int main() {
  try {
    for (int character : {1, -1}) {
      n::LegacyCanonicalPermInput input;
      input.degree = 8; // Six real slots, followed by two sign points.
      input.permutation = {1, 3, 5, 2, 6, 4, 7, 8};
      input.dummySetLengths = {6}; // Endpoint count: three dummy pairs.
      input.dummyLabels = {1, 2, 3, 4, 5, 6};
      input.metricSymmetries = {1}; // Symmetric metric in both examples.

      // Adjacent transpositions generate the total symmetry on slots 1,2,3.
      // The generic tensor's slots 4,5,6 have no slot symmetry generators.
      for (int slot : {1, 2}) {
        n::Permutation generator = {1, 2, 3, 4, 5, 6, 7, 8};
        std::swap(generator[slot - 1], generator[slot]);
        if (character < 0)
          std::swap(generator[6], generator[7]);
        input.slotGenerators.push_back(std::move(generator));
      }

      n::CanonicalizationStats statistics;
      const auto result = n::canonicalizeLegacyInput(input, &statistics);
      n::Permutation expected = {1, 3, 5, 2, 4, 6, 7, 8};
      if (character < 0)
        std::swap(expected[6], expected[7]);
      if (result.zero || result.permutation != expected ||
          statistics.survivingConfigurations != std::vector<std::size_t>(6, 1))
        throw std::runtime_error("Unexpected signed result or survivor counts");

      std::cout << (character > 0 ? "symmetric: [" : "antisymmetric: [");
      for (std::size_t i = 0; i < result.permutation.size(); ++i)
        std::cout << (i ? "," : "") << result.permutation[i];
      std::cout << "]\nsurvivors:";
      for (auto count : statistics.survivingConfigurations)
        std::cout << ' ' << count;
      std::cout << '\n';
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
