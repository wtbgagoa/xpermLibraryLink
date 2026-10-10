// Native equivalent of xpermLibraryLinkSample.nb's antisymmetric tensor ring.
// No elapsed-time assertions: run with 125 3 to measure the notebook workload.
#include "NiehoffCanonicalizer.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

namespace n = xperm::niehoff;
n::Permutation identity(int degree) {
  n::Permutation permutation(degree);
  std::iota(permutation.begin(), permutation.end(), 1);
  return permutation;
}
n::LegacyCanonicalPermInput ring(int factors) {
  n::LegacyCanonicalPermInput input;
  input.degree = 2 * factors + 2;
  input.base = identity(2 * factors);
  input.permutation = identity(input.degree);
  std::rotate(input.permutation.begin(), input.permutation.begin() + 1,
              input.permutation.begin() + 2 * factors);
  input.slotGeneratorsAreStrong = true;
  // A complete SGS for signed internal pair swaps and unsigned factor swaps.
  for (int index = 0; index < factors; ++index) {
    auto generator = identity(input.degree);
    std::swap(generator[2 * index], generator[2 * index + 1]);
    std::swap(generator[input.degree - 2], generator[input.degree - 1]);
    input.slotGenerators.push_back(std::move(generator));
  }
  for (int index = 0; index + 1 < factors; ++index) {
    auto generator = identity(input.degree);
    std::swap(generator[2 * index], generator[2 * index + 2]);
    std::swap(generator[2 * index + 1], generator[2 * index + 3]);
    input.slotGenerators.push_back(std::move(generator));
  }
  input.dummySetLengths = {2 * factors};
  input.dummyLabels = identity(2 * factors);
  input.metricSymmetries = {1};
  return input;
}

int main(int argc, char **argv) {
  try {
    if (argc == 1) {
      for (int factors = 1; factors <= 10; ++factors) {
        const auto result = n::canonicalizeLegacyInput(ring(factors));
        if (result.zero != (factors % 2 == 1))
          throw std::runtime_error("wrong odd/even antisymmetric-ring result");
      }
      std::cout << "Odd/even tensor-ring regressions passed (1..10 factors)\n";
      return 0;
    }
    const int factors = std::stoi(argv[1]);
    const int repeats = argc > 2 ? std::stoi(argv[2]) : 3;
    if (factors < 1 || factors > 500 || repeats < 1 || repeats > 100)
      throw std::invalid_argument("use 1..500 factors and 1..100 repetitions");
    auto input = ring(factors);
    // Exact permutation captured at the native boundary of FProduct[125].
    // All other fields equal the generated ring input above.
    if (factors == 125)
      input.permutation = {
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
        22, 23, 24, 25, 26, 27, 28, 75, 30, 51, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
        42, 43, 44, 45, 46, 47, 48, 49, 50, 53, 52, 59, 54, 55, 56, 57, 58, 1, 60, 61,
        62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 77, 76, 97, 78, 79, 80, 81,
        82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 99, 98, 119, 100, 101,
        102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 121, 120, 141,
        122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 143,
        142, 163, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161,
        162, 165, 164, 185, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181,
        182, 183, 184, 187, 186, 207, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201,
        202, 203, 204, 205, 206, 209, 208, 229, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221,
        222, 223, 224, 225, 226, 227, 228, 231, 230, 29, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241,
        242, 243, 244, 245, 246, 247, 248, 249, 250, 31, 251, 252};
    auto validate = [factors](const n::LegacyCanonicalPermResult &result) {
      if (result.zero != (factors % 2 == 1))
        throw std::runtime_error("wrong benchmark result");
    };
    validate(n::canonicalizeLegacyInput(input)); // warmup
    for (int iteration = 0; iteration < repeats; ++iteration) {
      const auto start = std::chrono::steady_clock::now();
      auto result = n::canonicalizeLegacyInput(input);
      const auto end = std::chrono::steady_clock::now();
      validate(result);
      std::cout << "factors=" << factors << " iteration=" << iteration + 1
                << " seconds=" << std::chrono::duration<double>(end - start).count()
                << " zero=" << result.zero << '\n';
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
