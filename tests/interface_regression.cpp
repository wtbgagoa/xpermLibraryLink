// Exercise the native LibraryLink boundary without requiring a Wolfram kernel.
#include "LLInterface.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct st_MNumericArray {
  mint type = MType_Integer;
  bool unavailableData = false;
  std::vector<mint> dimensions;
  std::vector<mint> values;
};

namespace {
bool missingOutputData = false;
int outstandingOutputs = 0;
int newTensor(mint type, mint rank, const mint *dimensions, MTensor *out) {
  auto *tensor = new st_MNumericArray;
  tensor->type = type;
  tensor->unavailableData = missingOutputData;
  tensor->dimensions.assign(dimensions, dimensions + rank);
  mint length = 1;
  for (mint dimension : tensor->dimensions)
    length *= dimension;
  tensor->values.resize(static_cast<std::size_t>(length));
  *out = tensor;
  ++outstandingOutputs;
  return LIBRARY_NO_ERROR;
}
void freeTensor(MTensor tensor) { delete tensor; --outstandingOutputs; }
mint rank(MTensor tensor) { return static_cast<mint>(tensor->dimensions.size()); }
mint type(MTensor tensor) { return tensor->type; }
const mint *dimensions(MTensor tensor) { return tensor->dimensions.data(); }
mint *data(MTensor tensor) {
  return tensor->unavailableData ? nullptr : tensor->values.data();
}

st_WolframLibraryData library = [] {
  st_WolframLibraryData result{};
  result.MTensor_new = newTensor;
  result.MTensor_free = freeTensor;
  result.MTensor_getRank = rank;
  result.MTensor_getType = type;
  result.MTensor_getDimensions = dimensions;
  result.MTensor_getIntegerData = data;
  return result;
}();

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

struct Call {
  std::vector<st_MNumericArray> tensors;
  std::vector<MTensor> tensorPointers;
  std::vector<mint> integers;
  std::vector<MArgument> arguments;
  MTensor result = nullptr;
  MArgument resultArgument{};
  explicit Call(std::size_t count)
      : tensors(count), tensorPointers(count), integers(count), arguments(count) {
    resultArgument.tensor = &result;
  }
  ~Call() { if (result) freeTensor(result); }
  void tensor(std::size_t index, std::vector<mint> shape,
              std::vector<mint> values) {
    tensors[index].dimensions = std::move(shape);
    tensors[index].values = std::move(values);
    tensorPointers[index] = &tensors[index];
    arguments[index].tensor = &tensorPointers[index];
  }
  void vector(std::size_t index, std::vector<mint> values) {
    const mint length = static_cast<mint>(values.size());
    tensor(index, {length}, std::move(values));
  }
  void integer(std::size_t index, mint value) {
    integers[index] = value;
    arguments[index].integer = &integers[index];
  }
  int invoke(int (*function)(WolframLibraryData, mint, MArgument *, MArgument)) {
    return function(&library, static_cast<mint>(arguments.size()),
                    arguments.data(), resultArgument);
  }
};

void propagatedTransport() {
  Call call(10);
  call.tensor(0, {1, 3}, {3, 2, 1});
  call.tensor(1, {1, 3}, {1, 2, 3});
  call.vector(2, {1});
  call.tensor(3, {1, 0}, {});
  call.tensor(4, {1, 3}, {2, 1, 3});
  call.vector(5, {1});
  call.vector(6, {1});
  call.tensor(7, {3, 3}, {0, 1, 1, 0, 1, 2, 0, 1, 3});
  call.tensor(8, {1, 4}, {1, 2, 1, 3});
  call.integer(9, 100);
  require(call.invoke(LL_niehoff_propagated_symmetry_search) == LIBRARY_NO_ERROR,
          "propagated search must accept nonempty signed symmetry generators");
  require(call.result && call.result->dimensions[1] >= 15,
          "propagated output must retain the degree after moving input rows");
  const auto width = static_cast<std::size_t>(call.result->dimensions[1]);
  bool foundConfiguration = false;
  for (std::size_t row = 0; row < call.result->values.size(); row += width) {
    if (call.result->values[row] != 0) continue;
    foundConfiguration = true;
    auto first = call.result->values.begin() + static_cast<std::ptrdiff_t>(row + 6);
    std::vector<mint> slot(first, first + 3), labels(first + 3, first + 6);
    std::sort(slot.begin(), slot.end());
    std::sort(labels.begin(), labels.end());
    require(slot == std::vector<mint>({1, 2, 3}) && slot == labels,
            "propagated configurations must have distinct permutation fields");
  }
  require(foundConfiguration, "propagated search must return configurations");
  freeTensor(call.result); call.result = nullptr;
  missingOutputData = true;
  const int memoryResult = call.invoke(LL_niehoff_propagated_symmetry_search);
  missingOutputData = false;
  require(memoryResult == LIBRARY_MEMORY_ERROR && !call.result,
          "missing output data must return a memory error and free the tensor");
}

void malformedStagedInputs() {
  Call call(7);
  call.tensor(0, {1, 2}, {1, 2});
  call.tensor(1, {1, 12}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  call.vector(2, {1});
  call.tensor(3, {1, 0}, {});
  call.tensor(4, {1, 2}, {1, 2});
  call.vector(5, {}); call.vector(6, {});
  require(call.invoke(LL_niehoff_persistent_search) == LIBRARY_FUNCTION_ERROR,
          "staged search must reject mismatched row widths even with no search levels");
  call.tensor(1, {1, 2}, {1, 2}); call.vector(2, {0});
  require(call.invoke(LL_niehoff_persistent_search) == LIBRARY_FUNCTION_ERROR,
          "staged search must reject zero signs even with no search levels");
  call.vector(2, {1}); call.tensor(4, {1, 3}, {1, 2, 3});
  require(call.invoke(LL_niehoff_persistent_search) == LIBRARY_FUNCTION_ERROR,
          "staged search must reject generator/configuration degree mismatch");
}

void groupBoundaries() {
  {
    Call call(4);
    call.integer(0, 3); call.vector(1, {}); call.vector(2, {});
    call.vector(3, {2});
    require(call.invoke(LL_basechange) == LIBRARY_NO_ERROR,
            "base change must support the identity group with an empty base");
    require(call.result->values == std::vector<mint>({3, 1, 0, 2}),
            "identity group base change result");
  }
  {
    Call call(4);
    call.integer(0, 3); call.vector(1, {1}); call.vector(2, {2, 1, 3});
    call.vector(3, {1});
    require(call.invoke(LL_stabsgs) == LIBRARY_NO_ERROR,
            "stabilizing a complete base must return the identity group");
    require(call.result->values == std::vector<mint>({3, 0, 0}),
            "full-base stabilizer must have an empty base and generator list");
  }
  {
    Call call(3);
    call.integer(0, 1); call.vector(1, {0, 1, 3}); call.integer(2, 3);
    require(call.invoke(LL_orbit) == LIBRARY_FUNCTION_ERROR,
            "orbit must reject nonpermutation generators before indexing");
  }
  {
    Call call(3);
    call.vector(0, {1, 1, 1, 1}); call.vector(1, {2, 1, 3}); call.integer(2, 3);
    require(call.invoke(LL_schreier_sims) == LIBRARY_FUNCTION_ERROR,
            "Schreier-Sims must reject duplicate or oversized bases");
  }
}

void canonicalValidation() {
  Call call(11);
  call.vector(0, {1, 2, 3, 4}); call.integer(1, 4); call.integer(2, 0);
  call.vector(3, {}); call.vector(4, {}); call.vector(5, {});
  call.vector(6, {2}); call.vector(7, {1, 2}); call.vector(8, {});
  call.vector(9, {}); call.vector(10, {});
  require(call.invoke(LL_canonical_perm) == LIBRARY_FUNCTION_ERROR,
          "legacy canonicalization must reject missing metric symmetry entries");
  require(call.invoke(LL_niehoff_canonical_perm) == LIBRARY_FUNCTION_ERROR,
          "Niehoff canonicalization must reject missing metric symmetry entries");
  call.vector(6, {}); call.vector(7, {}); call.vector(5, {1, 2});
  require(call.invoke(LL_canonical_perm) == LIBRARY_NO_ERROR,
          "validated legacy canonicalization must accept valid data");
  require(call.result->values == std::vector<mint>({1, 2, 3, 4}),
          "legacy canonicalization identity result");
}
} // namespace

int main() {
  try {
    require(WolframLibrary_getVersion() == WolframLibraryVersion,
            "LibraryLink header version export");
    require(WolframLibrary_initialize(&library) == LIBRARY_NO_ERROR,
            "LibraryLink initialization");
    propagatedTransport();
    malformedStagedInputs();
    groupBoundaries();
    canonicalValidation();
    require(outstandingOutputs == 0, "all output tensors must be released");
    std::cout << "LibraryLink interface regressions passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
