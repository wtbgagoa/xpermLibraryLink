#include "LLInterface.hpp"
#include "NiehoffCanonicalizer.hpp"
#include "xperm.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <limits>
#include <new>
#include <vector>

using namespace xperm::niehoff;

// LibraryLink negotiates its callback ABI through these lifecycle exports.
EXTERN_C DLLEXPORT mint WolframLibrary_getVersion() {
  return WolframLibraryVersion;
}
EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData) {
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData) {}

// ============================================================================
// Common native LibraryLink transport helpers
// ============================================================================

namespace {
using Row = std::vector<int>;

bool toInt(mint value, int &result) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max())
    return false;
  result = static_cast<int>(value);
  return true;
}

bool integerVector(WolframLibraryData libraryData, MTensor tensor, Row &result) {
  if (!tensor || libraryData->MTensor_getType(tensor) != MType_Integer ||
      libraryData->MTensor_getRank(tensor) != 1)
    return false;
  const mint *dimensions = libraryData->MTensor_getDimensions(tensor);
  if (!dimensions || dimensions[0] < 0 ||
      dimensions[0] > std::numeric_limits<int>::max())
    return false;
  result.assign(static_cast<std::size_t>(dimensions[0]), 0);
  if (dimensions[0] == 0)
    return true;
  const mint *data = libraryData->MTensor_getIntegerData(tensor);
  if (!data)
    return false;
  for (mint i = 0; i < dimensions[0]; ++i)
    if (!toInt(data[i], result[static_cast<std::size_t>(i)]))
      return false;
  return true;
}

int integerVectorResult(WolframLibraryData libraryData, const Row &values,
                        MArgument result) {
  mint dimensions[1] = {static_cast<mint>(values.size())};
  MTensor tensor = nullptr;
  if (libraryData->MTensor_new(MType_Integer, 1, dimensions, &tensor) !=
      LIBRARY_NO_ERROR)
    return LIBRARY_MEMORY_ERROR;
  mint *data = libraryData->MTensor_getIntegerData(tensor);
  if (!data && !values.empty()) {
    libraryData->MTensor_free(tensor);
    return LIBRARY_MEMORY_ERROR;
  }
  for (std::size_t i = 0; i < values.size(); ++i)
    data[i] = static_cast<mint>(values[i]);
  MArgument_setMTensor(result, tensor);
  return LIBRARY_NO_ERROR;
}

bool validPoints(const Row &points, int degree) {
  if (degree <= 0 || points.size() > static_cast<std::size_t>(degree))
    return false;
  std::vector<bool> seen(static_cast<std::size_t>(degree));
  for (int point : points) {
    if (point < 1 || point > degree || seen[static_cast<std::size_t>(point - 1)])
      return false;
    seen[static_cast<std::size_t>(point - 1)] = true;
  }
  return true;
}

bool validGenerators(const Row &generators, int degree) {
  if (degree <= 0 ||
      generators.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      generators.size() % static_cast<std::size_t>(degree) != 0)
    return false;
  std::vector<bool> seen(static_cast<std::size_t>(degree));
  for (std::size_t offset = 0; offset < generators.size(); offset += degree) {
    std::fill(seen.begin(), seen.end(), false);
    for (int point = 0; point < degree; ++point) {
      const int image = generators[offset + static_cast<std::size_t>(point)];
      if (image < 1 || image > degree || seen[static_cast<std::size_t>(image - 1)])
        return false;
      seen[static_cast<std::size_t>(image - 1)] = true;
    }
  }
  return true;
}

// The legacy base-change routine dereferences base[0] for every nonempty
// requested base. Complete an omitted base first and handle the trivial group
// without calling that routine.
void changeBase(Row &base, Row &generators, int degree,
                const Row &requestedBase, StabilizerChain &chain) {
  if (base.empty() && !generators.empty()) {
    Row completedBase(static_cast<std::size_t>(degree));
    Row completedGenerators;
    int baseLength = 0, generatorCount = 0, iterations = 0;
    schreier_sims(base.data(), 0, generators.data(),
                  static_cast<int>(generators.size() / degree), degree,
                  completedBase.data(), &baseLength, completedGenerators,
                  &generatorCount, &iterations);
    completedBase.resize(static_cast<std::size_t>(baseLength));
    base = std::move(completedBase);
    generators = std::move(completedGenerators);
  }
  if (base.empty()) {
    base = requestedBase;
    generators.clear();
    chain.resize(base.size());
    return;
  }
  stab_chain(base.data(), static_cast<int>(base.size()), generators.data(),
             static_cast<int>(generators.size() / degree), degree, chain);
  basechange_chain(base, generators, degree, chain, requestedBase.data(),
                   static_cast<int>(requestedBase.size()));
}

// A StrongGenSet is serialized as one integer stream:
//   {degree, baseLength, generatorCount, base..., flattenedGenerators...}
Row encodeStrongGenSet(int degree, const Row &base, const Row &generators) {
  const std::size_t generatorCount =
      generators.size() / static_cast<std::size_t>(degree);

  Row encoded;
  encoded.reserve(3 + base.size() + generators.size());
  encoded.push_back(degree);
  encoded.push_back(static_cast<int>(base.size()));
  encoded.push_back(static_cast<int>(generatorCount));
  encoded.insert(encoded.end(), base.begin(), base.end());
  encoded.insert(encoded.end(), generators.begin(), generators.end());
  return encoded;
}

int strongGenSetResult(WolframLibraryData libraryData, const Row &base,
                       const Row &generators, int degree, MArgument result) {
  if (!validGenerators(generators, degree))
    return LIBRARY_FUNCTION_ERROR;

  return integerVectorResult(libraryData,
                             encodeStrongGenSet(degree, base, generators),
                             result);
}

// A chain is serialized as:
//   {streamCount, streamLength1, ..., streamLengthN,
//    strongGenSetStream1..., ..., strongGenSetStreamN...}
// Every embedded stream uses exactly the same format as encodeStrongGenSet.
int strongGenSetChainResult(WolframLibraryData libraryData,
                            const std::vector<Row> &streams,
                            MArgument result) {
  std::size_t totalLength = 1 + streams.size();
  for (const Row &stream : streams)
    totalLength += stream.size();

  Row encoded;
  encoded.reserve(totalLength);
  encoded.push_back(static_cast<int>(streams.size()));
  for (const Row &stream : streams)
    encoded.push_back(static_cast<int>(stream.size()));
  for (const Row &stream : streams)
    encoded.insert(encoded.end(), stream.begin(), stream.end());

  return integerVectorResult(libraryData, encoded, result);
}
} // namespace

EXTERN_C DLLEXPORT int LL_schreier_sims(WolframLibraryData libraryData,
                                         mint argumentCount,
                                         MArgument *arguments,
                                         MArgument result) {
  try {
    if (argumentCount != 3)
      return LIBRARY_FUNCTION_ERROR;
    Row base, generators;
    int degree = 0;
    if (!integerVector(libraryData, MArgument_getMTensor(arguments[0]), base) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[1]), generators) ||
        !toInt(MArgument_getInteger(arguments[2]), degree) ||
        !validGenerators(generators, degree) || !validPoints(base, degree))
      return LIBRARY_FUNCTION_ERROR;

    Row newBase(static_cast<std::size_t>(degree));
    Row newGenerators = generators;
    int newBaseLength = 0, newGeneratorCount = 0, iterationCount = 0;
    schreier_sims(base.data(), static_cast<int>(base.size()), generators.data(),
                  static_cast<int>(generators.size() / degree), degree,
                  newBase.data(), &newBaseLength, newGenerators,
                  &newGeneratorCount, &iterationCount);
    newBase.resize(static_cast<std::size_t>(newBaseLength));
    newGenerators.resize(static_cast<std::size_t>(newGeneratorCount * degree));
    return strongGenSetResult(libraryData, newBase, newGenerators, degree, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_orbit(WolframLibraryData libraryData,
                                mint argumentCount, MArgument *arguments,
                                MArgument result) {
  try {
    if (argumentCount != 3)
      return LIBRARY_FUNCTION_ERROR;
    int point = 0, degree = 0;
    Row generators;
    if (!toInt(MArgument_getInteger(arguments[0]), point) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[1]), generators) ||
        !toInt(MArgument_getInteger(arguments[2]), degree) ||
        !validGenerators(generators, degree) || point < 1 || point > degree)
      return LIBRARY_FUNCTION_ERROR;
    Row orbit(static_cast<std::size_t>(degree));
    int orbitLength = 0;
    one_orbit(point, generators.data(), static_cast<int>(generators.size() / degree),
              degree, orbit.data(), &orbitLength);
    orbit.resize(static_cast<std::size_t>(orbitLength));
    return integerVectorResult(libraryData, orbit, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_set_stabilizer(WolframLibraryData libraryData,
                                         mint argumentCount,
                                         MArgument *arguments,
                                         MArgument result) {
  try {
    if (argumentCount != 4)
      return LIBRARY_FUNCTION_ERROR;
    Row points, base, generators;
    int degree = 0;
    if (!integerVector(libraryData, MArgument_getMTensor(arguments[0]), points) ||
        !toInt(MArgument_getInteger(arguments[1]), degree) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[2]), base) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[3]), generators) ||
        !validGenerators(generators, degree) || !validPoints(base, degree) ||
        !validPoints(points, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row characteristic(static_cast<std::size_t>(degree), 0);
    for (int point : points) {
      if (point < 1 || point > degree)
        return LIBRARY_FUNCTION_ERROR;
      characteristic[static_cast<std::size_t>(point - 1)] = 1;
    }
    Row subgroupGenerators;
    subgroupGenerators.reserve(generators.size());
    int generatorCount = 0, iterationCount = 0;
    search(base.data(), static_cast<int>(base.size()), generators.data(),
           static_cast<int>(generators.size() / degree), degree, 4,
           characteristic.data(), static_cast<int>(points.size()), 1,
           subgroupGenerators, &generatorCount, &iterationCount);
    subgroupGenerators.resize(static_cast<std::size_t>(generatorCount * degree));
    return strongGenSetResult(libraryData, base, subgroupGenerators, degree, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_basechangestabchain(WolframLibraryData libraryData,
                                              mint argumentCount,
                                              MArgument *arguments,
                                              MArgument result) {
  try {
    if (argumentCount != 4)
      return LIBRARY_FUNCTION_ERROR;
    int degree = 0;
    Row base, generators, newBase;
    if (!toInt(MArgument_getInteger(arguments[0]), degree) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[1]), base) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[2]), generators) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[3]), newBase) ||
        !validGenerators(generators, degree) || !validPoints(base, degree) ||
        !validPoints(newBase, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row changedBase = base, changedGenerators = generators;
    StabilizerChain chain;
    changeBase(changedBase, changedGenerators, degree, newBase, chain);
    if (chain.size() < changedBase.size())
      return LIBRARY_FUNCTION_ERROR;

    std::vector<Row> streams;
    streams.reserve(changedBase.size());

    for (std::size_t levelIndex = 0; levelIndex < changedBase.size();
         ++levelIndex) {
      const auto &level = chain[levelIndex];

      Row levelBase(changedBase.begin() +
                        static_cast<std::ptrdiff_t>(levelIndex),
                    changedBase.end());
      Row levelGenerators(level.size() * static_cast<std::size_t>(degree));
      for (std::size_t generatorIndex = 0; generatorIndex < level.size();
           ++generatorIndex) {
        const std::size_t sourceOffset =
            static_cast<std::size_t>(degree) * level[generatorIndex];
        const std::size_t destinationOffset =
            static_cast<std::size_t>(degree) * generatorIndex;
        std::copy_n(changedGenerators.data() + sourceOffset, degree,
                    levelGenerators.data() + destinationOffset);
      }

      streams.push_back(
          encodeStrongGenSet(degree, levelBase, levelGenerators));
    }

    return strongGenSetChainResult(libraryData, streams, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_basechange(WolframLibraryData libraryData,
                                     mint argumentCount, MArgument *arguments,
                                     MArgument result) {
  try {
    if (argumentCount != 4)
      return LIBRARY_FUNCTION_ERROR;
    int degree = 0;
    Row base, generators, newBase;
    if (!toInt(MArgument_getInteger(arguments[0]), degree) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[1]), base) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[2]), generators) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[3]), newBase) ||
        !validGenerators(generators, degree) || !validPoints(base, degree) ||
        !validPoints(newBase, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row changedBase = base, changedGenerators = generators;
    StabilizerChain chain;
    changeBase(changedBase, changedGenerators, degree, newBase, chain);
    if (chain.empty())
      return strongGenSetResult(libraryData, changedBase, {}, degree, result);
    const auto &level = chain.front();
    Row levelGenerators(level.size() * static_cast<std::size_t>(degree));
    for (std::size_t k = 0; k < level.size(); ++k)
      std::copy_n(changedGenerators.data() +
                      static_cast<std::size_t>(degree) * level[k],
                  degree, levelGenerators.data() +
                              static_cast<std::size_t>(degree) * k);
    return strongGenSetResult(libraryData, changedBase, levelGenerators, degree,
                              result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_stabsgs(WolframLibraryData libraryData,
                                  mint argumentCount, MArgument *arguments,
                                  MArgument result) {
  try {
    if (argumentCount != 4)
      return LIBRARY_FUNCTION_ERROR;
    int degree = 0;
    Row base, generators, points;
    if (!toInt(MArgument_getInteger(arguments[0]), degree) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[1]), base) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[2]), generators) ||
        !integerVector(libraryData, MArgument_getMTensor(arguments[3]), points) ||
        !validGenerators(generators, degree) || !validPoints(base, degree) ||
        !validPoints(points, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row changedBase = base, changedGenerators = generators;
    StabilizerChain chain;
    Row requestedBase = points;
    requestedBase.reserve(points.size() + base.size());
    for (int point : base)
      if (std::find(points.begin(), points.end(), point) == points.end())
        requestedBase.push_back(point);
    changeBase(changedBase, changedGenerators, degree, requestedBase, chain);
    const std::size_t levelIndex = points.size();
    if (changedBase.size() < levelIndex)
      return LIBRARY_FUNCTION_ERROR;
    if (levelIndex == changedBase.size())
      return strongGenSetResult(libraryData, {}, {}, degree, result);
    if (chain.size() <= levelIndex)
      return LIBRARY_FUNCTION_ERROR;
    const auto &level = chain[levelIndex];
    Row levelGenerators(level.size() * static_cast<std::size_t>(degree));
    for (std::size_t k = 0; k < level.size(); ++k)
      std::copy_n(changedGenerators.data() +
                      static_cast<std::size_t>(degree) * level[k],
                  degree, levelGenerators.data() +
                              static_cast<std::size_t>(degree) * k);
    Row stabilizerBase(changedBase.begin() + static_cast<std::ptrdiff_t>(levelIndex),
                       changedBase.end());
    return strongGenSetResult(libraryData, stabilizerBase, levelGenerators,
                              degree, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}


// ============================================================================
// Niehoff canonicalizer LibraryLink interface
// ============================================================================

namespace {
bool cv(mint x, int &y) { return toInt(x, y); }
bool matrix(WolframLibraryData l, MTensor t, std::vector<IntegerRow> &r) {
  if (!t || l->MTensor_getType(t) != MType_Integer ||
      l->MTensor_getRank(t) != 2)
    return false;
  const mint *d = l->MTensor_getDimensions(t);
  if (!d || d[0] < 0 || d[1] < 0 ||
      d[0] > std::numeric_limits<int>::max() ||
      d[1] > std::numeric_limits<int>::max() ||
      (d[1] != 0 && d[0] > std::numeric_limits<int>::max() / d[1]))
    return false;
  r.assign(static_cast<std::size_t>(d[0]),
           IntegerRow(static_cast<std::size_t>(d[1])));
  if (d[0] == 0 || d[1] == 0)
    return true;
  const mint *p = l->MTensor_getIntegerData(t);
  if (!p)
    return false;
  for (mint i = 0; i < d[0]; ++i)
    for (mint j = 0; j < d[1]; ++j)
      if (!cv(p[i * d[1] + j], r[i][j]))
        return false;
  return true;
}
bool vector(WolframLibraryData l, MTensor t, IntegerRow &r) {
  return integerVector(l, t, r);
}

bool readCanonicalInput(WolframLibraryData libraryData, MArgument *arguments,
                        LegacyCanonicalPermInput &input) {
  IntegerRow flatGenerators;
  int strongFlag = 0;
  if (!vector(libraryData, MArgument_getMTensor(arguments[0]), input.permutation) ||
      !cv(MArgument_getInteger(arguments[1]), input.degree) ||
      !cv(MArgument_getInteger(arguments[2]), strongFlag) ||
      !vector(libraryData, MArgument_getMTensor(arguments[3]), input.base) ||
      !vector(libraryData, MArgument_getMTensor(arguments[4]), flatGenerators) ||
      !vector(libraryData, MArgument_getMTensor(arguments[5]), input.freeLabels) ||
      !vector(libraryData, MArgument_getMTensor(arguments[6]), input.dummySetLengths) ||
      !vector(libraryData, MArgument_getMTensor(arguments[7]), input.dummyLabels) ||
      !vector(libraryData, MArgument_getMTensor(arguments[8]), input.metricSymmetries) ||
      !vector(libraryData, MArgument_getMTensor(arguments[9]), input.repeatedSetLengths) ||
      !vector(libraryData, MArgument_getMTensor(arguments[10]), input.repeatedLabels) ||
      (strongFlag != 0 && strongFlag != 1) ||
      !validGenerators(flatGenerators, input.degree))
    return false;
  input.slotGeneratorsAreStrong = strongFlag == 1;
  for (std::size_t offset = 0; offset < flatGenerators.size(); offset += input.degree)
    input.slotGenerators.emplace_back(flatGenerators.begin() + offset,
                                      flatGenerators.begin() + offset + input.degree);
  return true;
}
bool validSearchData(const IntegerMatrix &slots, const IntegerMatrix &labels,
                     const IntegerRow &signs, const IntegerMatrix &metadata,
                     const IntegerMatrix &generators, const IntegerRow &base,
                     const IntegerRow &selectedSlots) {
  if (slots.empty() || slots.front().empty() || labels.size() != slots.size() ||
      signs.size() != slots.size() || metadata.size() != slots.size())
    return false;
  const auto degree = slots.front().size();
  for (std::size_t i = 0; i < slots.size(); ++i)
    if (slots[i].size() != degree || labels[i].size() != degree ||
        !isPermutation(slots[i]) || !isPermutation(labels[i]) ||
        (signs[i] != 1 && signs[i] != -1))
      return false;
  for (const auto &generator : generators)
    if (generator.size() != degree || !isPermutation(generator))
      return false;
  return validPoints(base, static_cast<int>(degree)) &&
         validPoints(selectedSlots, static_cast<int>(degree));
}

int output(WolframLibraryData l, const std::vector<IntegerRow> &m,
           MArgument result) {
  mint rows = m.size(), cols = rows ? m.front().size() : 0;
  for (const auto &r : m)
    if (static_cast<mint>(r.size()) != cols)
      return LIBRARY_FUNCTION_ERROR;
  mint dims[2] = {rows, cols};
  MTensor t = nullptr;
  if (l->MTensor_new(MType_Integer, 2, dims, &t) != LIBRARY_NO_ERROR)
    return LIBRARY_MEMORY_ERROR;
  auto *p = l->MTensor_getIntegerData(t);
  if (!p && rows != 0 && cols != 0) {
    l->MTensor_free(t);
    return LIBRARY_MEMORY_ERROR;
  }
  for (mint i = 0; i < rows; ++i)
    for (mint j = 0; j < cols; ++j)
      p[i * cols + j] = m[i][j];
  MArgument_setMTensor(result, t);
  return LIBRARY_NO_ERROR;
}
} // namespace
// slots, labels, signs, metadata, generators, tentative base, selected slots.
// Tagged rows: 0 configuration, 1 level summary, 2 base, 3 strong generator,
// 4 chain position list.
EXTERN_C DLLEXPORT int LL_niehoff_persistent_search(WolframLibraryData l,
                                                    mint argc, MArgument *args,
                                                    MArgument result) {
  try {
    if (argc != 7)
      return LIBRARY_FUNCTION_ERROR;
    std::vector<IntegerRow> slots, labels, metadata, generators;
    IntegerRow signs, base, selected;
    if (!matrix(l, MArgument_getMTensor(args[0]), slots) ||
        !matrix(l, MArgument_getMTensor(args[1]), labels) ||
        !vector(l, MArgument_getMTensor(args[2]), signs) ||
        !matrix(l, MArgument_getMTensor(args[3]), metadata) ||
        !matrix(l, MArgument_getMTensor(args[4]), generators) ||
        !vector(l, MArgument_getMTensor(args[5]), base) ||
        !vector(l, MArgument_getMTensor(args[6]), selected))
      return LIBRARY_FUNCTION_ERROR;
    if (!validSearchData(slots, labels, signs, metadata, generators, base, selected))
      return LIBRARY_FUNCTION_ERROR;
    PersistentSearchInput in;
    in.tentativeBase = std::move(base);
    in.slotGenerators = std::move(generators);
    in.selectedSlots = std::move(selected);
    for (std::size_t i = 0; i < slots.size(); ++i)
      in.configurations.push_back({std::move(slots[i]),
                                   std::move(labels[i]),
                                   signs[i],
                                   std::move(metadata[i]),
                                   {}});
    auto search = runPersistentGroupSearch(in);
    std::size_t n = in.configurations.front().slotPermutation.size(),
                q = in.configurations.front().metadata.size();
    std::size_t maxChain = 0;
    for (const auto &positions : search.group.generatorPositions)
      maxChain = std::max(maxChain, positions.size());
    std::size_t width = std::max(
        {5 + 2 * n + q, 5 + search.group.base.size(), 5 + maxChain, 5 + n});
    std::vector<IntegerRow> rows;
    for (const auto &c : search.configurations) {
      IntegerRow r(width);
      r[0] = 0;
      r[1] = c.sign;
      std::copy(c.slotPermutation.begin(), c.slotPermutation.end(),
                r.begin() + 5);
      std::copy(c.labelPermutation.begin(), c.labelPermutation.end(),
                r.begin() + 5 + n);
      std::copy(c.metadata.begin(), c.metadata.end(), r.begin() + 5 + 2 * n);
      rows.push_back(std::move(r));
    }
    for (std::size_t i = 0; i < search.levels.size(); ++i) {
      const auto &s = search.levels[i];
      IntegerRow r(width);
      r[0] = 1;
      r[1] = static_cast<int>(i + 1);
      r[2] = s.selectedSlot;
      r[3] = s.minimumImage;
      r[4] = static_cast<int>(s.configurationCount);
      std::copy(s.orbit.begin(), s.orbit.end(), r.begin() + 5);
      rows.push_back(std::move(r));
    }
    IntegerRow br(width);
    br[0] = 2;
    br[1] = static_cast<int>(search.group.base.size());
    br[2] = static_cast<int>(search.group.level);
    std::copy(search.group.base.begin(), search.group.base.end(),
              br.begin() + 5);
    rows.push_back(std::move(br));
    for (const auto &g : search.group.strongGenerators) {
      IntegerRow r(width);
      r[0] = 3;
      std::copy(g.begin(), g.end(), r.begin() + 5);
      rows.push_back(std::move(r));
    }
    for (std::size_t i = 0; i < search.group.generatorPositions.size(); ++i) {
      IntegerRow r(width);
      r[0] = 4;
      r[1] = static_cast<int>(i + 1);
      r[2] = static_cast<int>(search.group.generatorPositions[i].size());
      std::copy(search.group.generatorPositions[i].begin(),
                search.group.generatorPositions[i].end(), r.begin() + 5);
      rows.push_back(std::move(r));
    }
    return output(l, rows, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

// Stage 5 arguments are the Stage 4 arguments followed by a label-group matrix.
// Each label-group row is {type, labelCount, label1, ..., labelCount,
// padding...}.
EXTERN_C DLLEXPORT int
LL_niehoff_label_group_search(WolframLibraryData libraryData,
                              mint argumentCount, MArgument *arguments,
                              MArgument result) {
  try {
    if (argumentCount != 8)
      return LIBRARY_FUNCTION_ERROR;

    std::vector<IntegerRow> slots;
    std::vector<IntegerRow> labels;
    std::vector<IntegerRow> metadata;
    std::vector<IntegerRow> generators;
    std::vector<IntegerRow> encodedGroups;
    IntegerRow signs;
    IntegerRow base;
    IntegerRow selectedSlots;
    if (!matrix(libraryData, MArgument_getMTensor(arguments[0]), slots) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[1]), labels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[2]), signs) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[3]), metadata) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[4]), generators) ||
        !vector(libraryData, MArgument_getMTensor(arguments[5]), base) ||
        !vector(libraryData, MArgument_getMTensor(arguments[6]),
                selectedSlots) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[7]), encodedGroups))
      return LIBRARY_FUNCTION_ERROR;
    if (!validSearchData(slots, labels, signs, metadata, generators, base,
                         selectedSlots))
      return LIBRARY_FUNCTION_ERROR;

    LabelGroupSearchInput input;
    input.tentativeBase = std::move(base);
    input.slotGenerators = std::move(generators);
    input.selectedSlots = std::move(selectedSlots);
    for (std::size_t index = 0; index < slots.size(); ++index)
      input.configurations.push_back({std::move(slots[index]),
                                      std::move(labels[index]),
                                      signs[index],
                                      std::move(metadata[index]),
                                      {}});

    for (const auto &row : encodedGroups) {
      if (row.size() < 2 || row[0] < 0 || row[0] > 5 || row[1] < 1 ||
          static_cast<std::size_t>(row[1]) > row.size() - 2)
        return LIBRARY_FUNCTION_ERROR;
      LabelGroup group;
      group.type = static_cast<LabelGroupType>(row[0]);
      group.labels.assign(row.begin() + 2, row.begin() + 2 + row[1]);
      input.labelGroups.push_back(std::move(group));
    }

    const auto search = runLabelGroupSearch(input);
    const std::size_t degree =
        input.configurations.front().slotPermutation.size();
    const std::size_t metadataWidth =
        input.configurations.front().metadata.size();
    std::size_t maximumChainLength = 0;
    for (const auto &positions : search.group.generatorPositions)
      maximumChainLength = std::max(maximumChainLength, positions.size());
    const std::size_t width =
        std::max({6 + 3 * degree + metadataWidth, 6 + search.group.base.size(),
                  6 + maximumChainLength, 6 + degree});

    std::vector<IntegerRow> rows;
    for (const auto &configuration : search.configurations) {
      IntegerRow row(width);
      row[0] = 0;
      row[1] = configuration.sign;
      row[2] = static_cast<int>(configuration.fixedLabels.size());
      std::copy(configuration.slotPermutation.begin(),
                configuration.slotPermutation.end(), row.begin() + 6);
      std::copy(configuration.labelPermutation.begin(),
                configuration.labelPermutation.end(), row.begin() + 6 + degree);
      std::copy(configuration.metadata.begin(), configuration.metadata.end(),
                row.begin() + 6 + 2 * degree);
      std::copy(configuration.fixedLabels.begin(),
                configuration.fixedLabels.end(),
                row.begin() + 6 + 2 * degree + metadataWidth);
      rows.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < search.levels.size(); ++index) {
      const auto &level = search.levels[index];
      IntegerRow row(width);
      row[0] = 1;
      row[1] = static_cast<int>(index + 1);
      row[2] = level.selectedSlot;
      row[3] = level.minimumImage;
      row[4] = static_cast<int>(level.configurationCount);
      row[5] = static_cast<int>(level.orbit.size());
      std::copy(level.orbit.begin(), level.orbit.end(), row.begin() + 6);
      rows.push_back(std::move(row));
    }
    IntegerRow baseRow(width);
    baseRow[0] = 2;
    baseRow[1] = static_cast<int>(search.group.base.size());
    baseRow[2] = static_cast<int>(search.group.level);
    std::copy(search.group.base.begin(), search.group.base.end(),
              baseRow.begin() + 6);
    rows.push_back(std::move(baseRow));
    for (const auto &generator : search.group.strongGenerators) {
      IntegerRow row(width);
      row[0] = 3;
      std::copy(generator.begin(), generator.end(), row.begin() + 6);
      rows.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < search.group.generatorPositions.size();
         ++index) {
      IntegerRow row(width);
      row[0] = 4;
      row[1] = static_cast<int>(index + 1);
      row[2] = static_cast<int>(search.group.generatorPositions[index].size());
      std::copy(search.group.generatorPositions[index].begin(),
                search.group.generatorPositions[index].end(), row.begin() + 6);
      rows.push_back(std::move(row));
    }
    return output(libraryData, rows, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

// Stage 6 adds a signed propagated-symmetry generator matrix and an orbit cap.
// A signed-generator row is {sign, image1, ..., imageN}.
EXTERN_C DLLEXPORT int
LL_niehoff_propagated_symmetry_search(WolframLibraryData libraryData,
                                      mint argumentCount, MArgument *arguments,
                                      MArgument result) {
  try {
    if (argumentCount != 10)
      return LIBRARY_FUNCTION_ERROR;

    std::vector<IntegerRow> slots;
    std::vector<IntegerRow> labels;
    std::vector<IntegerRow> metadata;
    std::vector<IntegerRow> generators;
    std::vector<IntegerRow> encodedGroups;
    std::vector<IntegerRow> encodedSymmetries;
    IntegerRow signs;
    IntegerRow base;
    IntegerRow selectedSlots;
    if (!matrix(libraryData, MArgument_getMTensor(arguments[0]), slots) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[1]), labels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[2]), signs) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[3]), metadata) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[4]), generators) ||
        !vector(libraryData, MArgument_getMTensor(arguments[5]), base) ||
        !vector(libraryData, MArgument_getMTensor(arguments[6]),
                selectedSlots) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[7]),
                encodedGroups) ||
        !matrix(libraryData, MArgument_getMTensor(arguments[8]),
                encodedSymmetries))
      return LIBRARY_FUNCTION_ERROR;
    const mint maximumOrbitArgument = MArgument_getInteger(arguments[9]);
    if (maximumOrbitArgument < 1 ||
        !validSearchData(slots, labels, signs, metadata, generators, base,
                         selectedSlots))
      return LIBRARY_FUNCTION_ERROR;

    PropagatedSymmetryInput input;
    input.labelSearch.tentativeBase = std::move(base);
    input.labelSearch.slotGenerators = std::move(generators);
    input.labelSearch.selectedSlots = std::move(selectedSlots);
    input.maximumOrbitSize = static_cast<std::size_t>(maximumOrbitArgument);
    for (std::size_t index = 0; index < slots.size(); ++index)
      input.labelSearch.configurations.push_back({std::move(slots[index]),
                                                  std::move(labels[index]),
                                                  signs[index],
                                                  std::move(metadata[index]),
                                                  {}});

    for (const auto &row : encodedGroups) {
      if (row.size() < 2 || row[0] < 0 || row[0] > 5 || row[1] < 1 ||
          static_cast<std::size_t>(row[1]) > row.size() - 2)
        return LIBRARY_FUNCTION_ERROR;
      LabelGroup group;
      group.type = static_cast<LabelGroupType>(row[0]);
      group.labels.assign(row.begin() + 2, row.begin() + 2 + row[1]);
      input.labelSearch.labelGroups.push_back(std::move(group));
    }

    const std::size_t degree =
        input.labelSearch.configurations.front().slotPermutation.size();
    for (const auto &row : encodedSymmetries) {
      if (row.size() != degree + 1 || (row[0] != 1 && row[0] != -1))
        return LIBRARY_FUNCTION_ERROR;
      input.generators.push_back(
          {Permutation(row.begin() + 1, row.end()), row[0]});
    }

    const auto search = runPropagatedSymmetrySearch(input);
    const std::size_t metadataWidth =
        input.labelSearch.configurations.front().metadata.size();
    std::size_t maximumChainLength = 0;
    for (const auto &positions : search.group.generatorPositions)
      maximumChainLength = std::max(maximumChainLength, positions.size());
    const std::size_t width =
        std::max({6 + 3 * degree + metadataWidth, 6 + search.group.base.size(),
                  6 + maximumChainLength, 6 + degree});

    std::vector<IntegerRow> rows;
    for (const auto &configuration : search.configurations) {
      IntegerRow row(width);
      row[0] = 0;
      row[1] = configuration.sign;
      row[2] = static_cast<int>(configuration.fixedLabels.size());
      std::copy(configuration.slotPermutation.begin(),
                configuration.slotPermutation.end(), row.begin() + 6);
      std::copy(configuration.labelPermutation.begin(),
                configuration.labelPermutation.end(), row.begin() + 6 + degree);
      std::copy(configuration.metadata.begin(), configuration.metadata.end(),
                row.begin() + 6 + 2 * degree);
      std::copy(configuration.fixedLabels.begin(),
                configuration.fixedLabels.end(),
                row.begin() + 6 + 2 * degree + metadataWidth);
      rows.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < search.levels.size(); ++index) {
      const auto &level = search.levels[index];
      IntegerRow row(width);
      row[0] = 1;
      row[1] = static_cast<int>(index + 1);
      row[2] = level.selectedSlot;
      row[3] = level.minimumImage;
      row[4] = static_cast<int>(level.configurationCount);
      row[5] = static_cast<int>(level.orbit.size());
      std::copy(level.orbit.begin(), level.orbit.end(), row.begin() + 6);
      rows.push_back(std::move(row));
    }
    IntegerRow baseRow(width);
    baseRow[0] = 2;
    baseRow[1] = static_cast<int>(search.group.base.size());
    baseRow[2] = static_cast<int>(search.group.level);
    std::copy(search.group.base.begin(), search.group.base.end(),
              baseRow.begin() + 6);
    rows.push_back(std::move(baseRow));
    for (const auto &generator : search.group.strongGenerators) {
      IntegerRow row(width);
      row[0] = 3;
      std::copy(generator.begin(), generator.end(), row.begin() + 6);
      rows.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < search.group.generatorPositions.size();
         ++index) {
      IntegerRow row(width);
      row[0] = 4;
      row[1] = static_cast<int>(index + 1);
      row[2] = static_cast<int>(search.group.generatorPositions[index].size());
      std::copy(search.group.generatorPositions[index].begin(),
                search.group.generatorPositions[index].end(), row.begin() + 6);
      rows.push_back(std::move(row));
    }
    IntegerRow statusRow(width);
    statusRow[0] = 5;
    statusRow[1] = search.zero ? 1 : 0;
    statusRow[2] = static_cast<int>(search.mergedConfigurationCount);
    statusRow[3] = static_cast<int>(search.cancelledConfigurationCount);
    rows.push_back(std::move(statusRow));
    return output(libraryData, rows, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_canonical_perm(WolframLibraryData libraryData,
                                         mint argumentCount,
                                         MArgument *arguments,
                                         MArgument result) {
  try {
    if (argumentCount != 11)
      return LIBRARY_FUNCTION_ERROR;

    LegacyCanonicalPermInput input;
    if (!readCanonicalInput(libraryData, arguments, input))
      return LIBRARY_FUNCTION_ERROR;
    // The old xPerm routines assume valid permutations, disjoint label groups,
    // and matching length/metric arrays. Validate before entering those routines.
    (void)makeLegacyLabelGroups(input);
    IntegerRow flatGenerators;
    for (const auto &generator : input.slotGenerators)
      flatGenerators.insert(flatGenerators.end(), generator.begin(), generator.end());
    IntegerRow canonical(static_cast<std::size_t>(input.degree));
    canonical_perm_ext(
        input.permutation.data(), input.degree, input.slotGeneratorsAreStrong,
        input.base.data(), static_cast<int>(input.base.size()),
        flatGenerators.data(), static_cast<int>(input.slotGenerators.size()),
        input.freeLabels.data(), static_cast<int>(input.freeLabels.size()),
        input.dummySetLengths.data(), static_cast<int>(input.dummySetLengths.size()),
        input.dummyLabels.data(), static_cast<int>(input.dummyLabels.size()),
        input.metricSymmetries.data(),
        input.repeatedSetLengths.data(), static_cast<int>(input.repeatedSetLengths.size()),
        input.repeatedLabels.data(), static_cast<int>(input.repeatedLabels.size()),
        canonical.data());
    return integerVectorResult(libraryData, canonical, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int LL_niehoff_canonical_perm(WolframLibraryData libraryData,
                                                 mint argumentCount,
                                                 MArgument *arguments,
                                                 MArgument result) {
  try {
    if (argumentCount != 11)
      return LIBRARY_FUNCTION_ERROR;

    LegacyCanonicalPermInput input;
    if (!readCanonicalInput(libraryData, arguments, input))
      return LIBRARY_FUNCTION_ERROR;

    const auto canonical = canonicalizeLegacyInput(input);
    const IntegerRow outputRow =
        canonical.zero ? IntegerRow(static_cast<std::size_t>(input.degree), 0)
                       : canonical.permutation;
    return integerVectorResult(libraryData, outputRow, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}

EXTERN_C DLLEXPORT int
LL_niehoff_canonical_perm_ext(WolframLibraryData libraryData,
                              mint argumentCount, MArgument *arguments,
                              MArgument result) {
  try {
    if (argumentCount != 14)
      return LIBRARY_FUNCTION_ERROR;

    ExtendedCanonicalPermInput input;
    IntegerRow subsetOffsets, subsetSlots, subsetSigns;
    auto &legacy = input.legacy;
    if (!readCanonicalInput(libraryData, arguments, legacy) ||
        !vector(libraryData, MArgument_getMTensor(arguments[11]), subsetOffsets) ||
        !vector(libraryData, MArgument_getMTensor(arguments[12]), subsetSlots) ||
        !vector(libraryData, MArgument_getMTensor(arguments[13]), subsetSigns))
      return LIBRARY_FUNCTION_ERROR;

    if (subsetOffsets.empty()) {
      if (!subsetSlots.empty() || !subsetSigns.empty())
        return LIBRARY_FUNCTION_ERROR;
    } else {
      if (subsetOffsets.front() != 0 ||
          subsetOffsets.back() != static_cast<int>(subsetSlots.size()) ||
          subsetSigns.size() + 1 != subsetOffsets.size())
        return LIBRARY_FUNCTION_ERROR;
      for (std::size_t subset = 0; subset < subsetSigns.size(); ++subset) {
        const int begin = subsetOffsets[subset];
        const int end = subsetOffsets[subset + 1];
        if (begin < 0 || end < begin ||
            end > static_cast<int>(subsetSlots.size()))
          return LIBRARY_FUNCTION_ERROR;
        input.totalSymmetrySubsets.push_back(
            {IntegerRow(subsetSlots.begin() + begin, subsetSlots.begin() + end),
             subsetSigns[subset]});
      }
    }

    const auto canonical = canonicalizeExtendedInput(input);
    const IntegerRow outputRow =
        canonical.zero ? IntegerRow(static_cast<std::size_t>(legacy.degree), 0)
                       : canonical.permutation;
    return integerVectorResult(libraryData, outputRow, result);
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}
