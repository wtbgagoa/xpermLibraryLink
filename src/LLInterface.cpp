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
  if (!dimensions || dimensions[0] < 0)
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

// Encoded structured result. Each row is {tag, index, length, data...}.
// tag 0 = degree, 1 = base, 2 = flattened generators.
int structuredResult(WolframLibraryData libraryData,
                     const std::vector<Row> &rows, MArgument result) {
  std::size_t width = 3;
  for (const Row &row : rows)
    width = std::max(width, row.size());
  mint dimensions[2] = {static_cast<mint>(rows.size()),
                        static_cast<mint>(width)};
  MTensor tensor = nullptr;
  if (libraryData->MTensor_new(MType_Integer, 2, dimensions, &tensor) !=
      LIBRARY_NO_ERROR)
    return LIBRARY_MEMORY_ERROR;
  mint *data = libraryData->MTensor_getIntegerData(tensor);
  if (!data && !rows.empty()) {
    libraryData->MTensor_free(tensor);
    return LIBRARY_MEMORY_ERROR;
  }
  if (!rows.empty())
    std::fill(data, data + rows.size() * width, static_cast<mint>(0));
  for (std::size_t i = 0; i < rows.size(); ++i)
    for (std::size_t j = 0; j < rows[i].size(); ++j)
      data[i * width + j] = static_cast<mint>(rows[i][j]);
  MArgument_setMTensor(result, tensor);
  return LIBRARY_NO_ERROR;
}

Row encodedRow(int tag, int index, const int *data, std::size_t length) {
  Row row{tag, index, static_cast<int>(length)};
  if (length != 0)
    row.insert(row.end(), data, data + length);
  return row;
}

Row encodedScalar(int tag, int index, int value) {
  return Row{tag, index, 1, value};
}

bool validGenerators(const Row &generators, int degree) {
  return degree > 0 &&
         generators.size() % static_cast<std::size_t>(degree) == 0;
}

int strongGenSetResult(WolframLibraryData libraryData, const Row &base,
                       const Row &generators, int degree, MArgument result,
                       int index = 0) {
  std::vector<Row> rows;
  rows.push_back(encodedScalar(0, index, degree));
  rows.push_back(encodedRow(1, index, base.data(), base.size()));
  rows.push_back(encodedRow(2, index, generators.data(), generators.size()));
  return structuredResult(libraryData, rows, result);
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
        !validGenerators(generators, degree))
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
        !validGenerators(generators, degree))
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
        !validGenerators(generators, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row changedBase = base, changedGenerators = generators;
    StabilizerChain chain;
    stab_chain(changedBase.data(), static_cast<int>(changedBase.size()),
               changedGenerators.data(),
               static_cast<int>(changedGenerators.size() / degree), degree, chain);
    basechange_chain(changedBase, changedGenerators, degree, chain,
                     newBase.data(), static_cast<int>(newBase.size()));
    if (chain.size() < changedBase.size())
      return LIBRARY_FUNCTION_ERROR;

    std::vector<Row> rows;
    for (std::size_t levelIndex = 0; levelIndex < changedBase.size(); ++levelIndex) {
      const auto &level = chain[levelIndex];
      Row levelGenerators(level.size() * static_cast<std::size_t>(degree));
      for (std::size_t k = 0; k < level.size(); ++k)
        std::copy_n(changedGenerators.data() +
                        static_cast<std::size_t>(degree) * level[k],
                    degree, levelGenerators.data() +
                                static_cast<std::size_t>(degree) * k);
      rows.push_back(encodedScalar(0, static_cast<int>(levelIndex), degree));
      rows.push_back(encodedRow(1, static_cast<int>(levelIndex),
                                changedBase.data() + levelIndex,
                                changedBase.size() - levelIndex));
      rows.push_back(encodedRow(2, static_cast<int>(levelIndex),
                                levelGenerators.data(), levelGenerators.size()));
    }
    return structuredResult(libraryData, rows, result);
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
        !validGenerators(generators, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row changedBase = base, changedGenerators = generators;
    StabilizerChain chain;
    stab_chain(changedBase.data(), static_cast<int>(changedBase.size()),
               changedGenerators.data(),
               static_cast<int>(changedGenerators.size() / degree), degree, chain);
    basechange_chain(changedBase, changedGenerators, degree, chain,
                     newBase.data(), static_cast<int>(newBase.size()));
    if (chain.empty())
      return LIBRARY_FUNCTION_ERROR;
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
        !validGenerators(generators, degree))
      return LIBRARY_FUNCTION_ERROR;
    Row changedBase = base, changedGenerators = generators;
    StabilizerChain chain;
    stab_chain(changedBase.data(), static_cast<int>(changedBase.size()),
               changedGenerators.data(),
               static_cast<int>(changedGenerators.size() / degree), degree, chain);
    Row requestedBase = points;
    requestedBase.reserve(points.size() + base.size());
    for (int point : base)
      if (std::find(points.begin(), points.end(), point) == points.end())
        requestedBase.push_back(point);
    basechange_chain(changedBase, changedGenerators, degree, chain,
                     requestedBase.data(), static_cast<int>(requestedBase.size()));
    const std::size_t levelIndex = points.size();
    if (chain.size() <= levelIndex || changedBase.size() < levelIndex)
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
bool cv(mint x, int &y) {
  if (x < std::numeric_limits<int>::min() ||
      x > std::numeric_limits<int>::max())
    return false;
  y = static_cast<int>(x);
  return true;
}
bool matrix(WolframLibraryData l, MTensor t, std::vector<IntegerRow> &r) {
  if (!t || l->MTensor_getType(t) != MType_Integer ||
      l->MTensor_getRank(t) != 2)
    return false;
  auto *d = l->MTensor_getDimensions(t);
  auto *p = l->MTensor_getIntegerData(t);
  if (!d || !p)
    return false;
  r.assign(d[0], IntegerRow(d[1]));
  for (mint i = 0; i < d[0]; ++i)
    for (mint j = 0; j < d[1]; ++j)
      if (!cv(p[i * d[1] + j], r[i][j]))
        return false;
  return true;
}
bool vector(WolframLibraryData l, MTensor t, IntegerRow &r) {
  if (!t || l->MTensor_getType(t) != MType_Integer ||
      l->MTensor_getRank(t) != 1)
    return false;
  auto *d = l->MTensor_getDimensions(t);
  if (!d || d[0] < 0)
    return false;
  r.assign(static_cast<std::size_t>(d[0]), 0);
  if (d[0] == 0)
    return true;
  auto *p = l->MTensor_getIntegerData(t);
  if (!p)
    return false;
  for (mint i = 0; i < d[0]; ++i)
    if (!cv(p[i], r[static_cast<std::size_t>(i)]))
      return false;
  return true;
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
    if (slots.empty() || labels.size() != slots.size() ||
        signs.size() != slots.size() || metadata.size() != slots.size())
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
    if (slots.empty() || labels.size() != slots.size() ||
        signs.size() != slots.size() || metadata.size() != slots.size())
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
          static_cast<std::size_t>(row[1] + 2) > row.size())
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
    if (maximumOrbitArgument < 1 || slots.empty() ||
        labels.size() != slots.size() || signs.size() != slots.size() ||
        metadata.size() != slots.size())
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
          static_cast<std::size_t>(row[1] + 2) > row.size())
        return LIBRARY_FUNCTION_ERROR;
      LabelGroup group;
      group.type = static_cast<LabelGroupType>(row[0]);
      group.labels.assign(row.begin() + 2, row.begin() + 2 + row[1]);
      input.labelSearch.labelGroups.push_back(std::move(group));
    }

    const std::size_t degree = slots.front().size();
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

    IntegerRow permutation, base, flatGenerators, freeLabels;
    IntegerRow dummySetLengths, dummyLabels, metricSymmetries;
    IntegerRow repeatedSetLengths, repeatedLabels;
    int degree = 0;
    int strongFlag = 0;

    if (!vector(libraryData, MArgument_getMTensor(arguments[0]), permutation) ||
        !cv(MArgument_getInteger(arguments[1]), degree) ||
        !cv(MArgument_getInteger(arguments[2]), strongFlag) ||
        !vector(libraryData, MArgument_getMTensor(arguments[3]), base) ||
        !vector(libraryData, MArgument_getMTensor(arguments[4]), flatGenerators) ||
        !vector(libraryData, MArgument_getMTensor(arguments[5]), freeLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[6]), dummySetLengths) ||
        !vector(libraryData, MArgument_getMTensor(arguments[7]), dummyLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[8]), metricSymmetries) ||
        !vector(libraryData, MArgument_getMTensor(arguments[9]), repeatedSetLengths) ||
        !vector(libraryData, MArgument_getMTensor(arguments[10]), repeatedLabels))
      return LIBRARY_FUNCTION_ERROR;

    if (degree <= 0 || permutation.size() < static_cast<std::size_t>(degree) ||
        (strongFlag != 0 && strongFlag != 1) ||
        flatGenerators.size() % static_cast<std::size_t>(degree) != 0)
      return LIBRARY_FUNCTION_ERROR;

    IntegerRow canonical(static_cast<std::size_t>(degree));
    canonical_perm_ext(
        permutation.data(), degree, strongFlag,
        base.data(), static_cast<int>(base.size()),
        flatGenerators.data(),
        static_cast<int>(flatGenerators.size() / static_cast<std::size_t>(degree)),
        freeLabels.data(), static_cast<int>(freeLabels.size()),
        dummySetLengths.data(), static_cast<int>(dummySetLengths.size()),
        dummyLabels.data(), static_cast<int>(dummyLabels.size()),
        metricSymmetries.data(),
        repeatedSetLengths.data(), static_cast<int>(repeatedSetLengths.size()),
        repeatedLabels.data(), static_cast<int>(repeatedLabels.size()),
        canonical.data());

    mint dimensions[1] = {static_cast<mint>(canonical.size())};
    MTensor tensor = nullptr;
    if (libraryData->MTensor_new(MType_Integer, 1, dimensions, &tensor) !=
        LIBRARY_NO_ERROR)
      return LIBRARY_MEMORY_ERROR;
    mint *data = libraryData->MTensor_getIntegerData(tensor);
    if (!data) {
      libraryData->MTensor_free(tensor);
      return LIBRARY_MEMORY_ERROR;
    }
    for (std::size_t index = 0; index < canonical.size(); ++index)
      data[index] = static_cast<mint>(canonical[index]);
    MArgument_setMTensor(result, tensor);
    return LIBRARY_NO_ERROR;
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
    IntegerRow flatGenerators;
    if (!vector(libraryData, MArgument_getMTensor(arguments[0]),
                input.permutation) ||
        !cv(MArgument_getInteger(arguments[1]), input.degree) ||
        !vector(libraryData, MArgument_getMTensor(arguments[3]), input.base) ||
        !vector(libraryData, MArgument_getMTensor(arguments[4]),
                flatGenerators) ||
        !vector(libraryData, MArgument_getMTensor(arguments[5]),
                input.freeLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[6]),
                input.dummySetLengths) ||
        !vector(libraryData, MArgument_getMTensor(arguments[7]),
                input.dummyLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[8]),
                input.metricSymmetries) ||
        !vector(libraryData, MArgument_getMTensor(arguments[9]),
                input.repeatedSetLengths) ||
        !vector(libraryData, MArgument_getMTensor(arguments[10]),
                input.repeatedLabels))
      return LIBRARY_FUNCTION_ERROR;

    int strongFlag = 0;
    if (!cv(MArgument_getInteger(arguments[2]), strongFlag) ||
        (strongFlag != 0 && strongFlag != 1) || input.degree <= 0 ||
        flatGenerators.size() % static_cast<std::size_t>(input.degree) != 0)
      return LIBRARY_FUNCTION_ERROR;
    input.slotGeneratorsAreStrong = strongFlag == 1;
    for (std::size_t offset = 0; offset < flatGenerators.size();
         offset += static_cast<std::size_t>(input.degree))
      input.slotGenerators.emplace_back(
          flatGenerators.begin() + static_cast<std::ptrdiff_t>(offset),
          flatGenerators.begin() +
              static_cast<std::ptrdiff_t>(offset + input.degree));

    const auto canonical = canonicalizeLegacyInput(input);
    const IntegerRow outputRow =
        canonical.zero ? IntegerRow(static_cast<std::size_t>(input.degree), 0)
                       : canonical.permutation;
    std::vector<IntegerRow> outputMatrix{outputRow};
    mint dimensions[1] = {static_cast<mint>(outputRow.size())};
    MTensor tensor = nullptr;
    if (libraryData->MTensor_new(MType_Integer, 1, dimensions, &tensor) !=
        LIBRARY_NO_ERROR)
      return LIBRARY_MEMORY_ERROR;
    mint *data = libraryData->MTensor_getIntegerData(tensor);
    for (std::size_t index = 0; index < outputRow.size(); ++index)
      data[index] = static_cast<mint>(outputRow[index]);
    MArgument_setMTensor(result, tensor);
    return LIBRARY_NO_ERROR;
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
    IntegerRow flatGenerators;
    IntegerRow subsetOffsets;
    IntegerRow subsetSlots;
    IntegerRow subsetSigns;
    auto &legacy = input.legacy;
    if (!vector(libraryData, MArgument_getMTensor(arguments[0]),
                legacy.permutation) ||
        !cv(MArgument_getInteger(arguments[1]), legacy.degree) ||
        !vector(libraryData, MArgument_getMTensor(arguments[3]), legacy.base) ||
        !vector(libraryData, MArgument_getMTensor(arguments[4]),
                flatGenerators) ||
        !vector(libraryData, MArgument_getMTensor(arguments[5]),
                legacy.freeLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[6]),
                legacy.dummySetLengths) ||
        !vector(libraryData, MArgument_getMTensor(arguments[7]),
                legacy.dummyLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[8]),
                legacy.metricSymmetries) ||
        !vector(libraryData, MArgument_getMTensor(arguments[9]),
                legacy.repeatedSetLengths) ||
        !vector(libraryData, MArgument_getMTensor(arguments[10]),
                legacy.repeatedLabels) ||
        !vector(libraryData, MArgument_getMTensor(arguments[11]),
                subsetOffsets) ||
        !vector(libraryData, MArgument_getMTensor(arguments[12]),
                subsetSlots) ||
        !vector(libraryData, MArgument_getMTensor(arguments[13]), subsetSigns))
      return LIBRARY_FUNCTION_ERROR;

    int strongFlag = 0;
    if (!cv(MArgument_getInteger(arguments[2]), strongFlag) ||
        (strongFlag != 0 && strongFlag != 1) || legacy.degree <= 0 ||
        flatGenerators.size() % static_cast<std::size_t>(legacy.degree) != 0)
      return LIBRARY_FUNCTION_ERROR;
    legacy.slotGeneratorsAreStrong = strongFlag == 1;
    for (std::size_t offset = 0; offset < flatGenerators.size();
         offset += static_cast<std::size_t>(legacy.degree))
      legacy.slotGenerators.emplace_back(
          flatGenerators.begin() + static_cast<std::ptrdiff_t>(offset),
          flatGenerators.begin() +
              static_cast<std::ptrdiff_t>(offset + legacy.degree));

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
    mint dimensions[1] = {static_cast<mint>(outputRow.size())};
    MTensor tensor = nullptr;
    if (libraryData->MTensor_new(MType_Integer, 1, dimensions, &tensor) !=
        LIBRARY_NO_ERROR)
      return LIBRARY_MEMORY_ERROR;
    mint *data = libraryData->MTensor_getIntegerData(tensor);
    for (std::size_t index = 0; index < outputRow.size(); ++index)
      data[index] = static_cast<mint>(outputRow[index]);
    MArgument_setMTensor(result, tensor);
    return LIBRARY_NO_ERROR;
  } catch (const std::bad_alloc &) {
    return LIBRARY_MEMORY_ERROR;
  } catch (const std::exception &) {
    return LIBRARY_FUNCTION_ERROR;
  }
}
