/**
 * @file LLInterface.hpp
 * @brief C ABI used to connect the Wolfram paclet to both canonicalizers.
 *
 * These exports transport integer arrays; they do not themselves implement the
 * canonicalization search. See @ref librarylink_integration for the argument
 * layouts and the distinction between algorithm selection and WSTP replacement.
 */
#ifndef XPERM_LL_INTERFACE_HPP
#define XPERM_LL_INTERFACE_HPP

#include "WolframLibrary.h"

/**
 * @defgroup librarylink_api LibraryLink interface
 * @brief Native entry points, transport formats, and algorithm dispatch.
 *
 * For every `LL_*` entry point, `libraryData` supplies the Wolfram runtime's
 * tensor callbacks, `argumentCount` is the number of entries in `arguments`, and
 * `result` receives a newly allocated integer tensor on success. Input tensors
 * are read and copied, never freed or modified by these wrappers. The runtime
 * takes ownership of the result after `MArgument_setMTensor`.
 *
 * Functions return `LIBRARY_NO_ERROR` on success, `LIBRARY_MEMORY_ERROR` on
 * allocation failure, or `LIBRARY_FUNCTION_ERROR` for rejected input or a
 * caught `std::exception`. An identically zero tensor expression is a successful
 * canonicalization result, not a LibraryLink error. See
 * @ref librarylink_errors and @ref librarylink_wire_formats.
 * @{
 */

/** @brief Report the LibraryLink ABI version used to compile this library. */
EXTERN_C DLLEXPORT mint WolframLibrary_getVersion();
/** @brief Initialize the library; no global native resources are allocated. */
EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libraryData);
/** @brief Lifecycle hook; native searches retain no resources between calls. */
EXTERN_C DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libraryData);

/**
 * @brief Build a base and strong generating set using the inherited xPerm code.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 3.
 * @param arguments `{tentativeBase, flattenedGenerators, degree}`.
 * @param result Encoded strong generating set; see @ref librarylink_wire_formats.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @see schreier_sims
 */
EXTERN_C DLLEXPORT int LL_schreier_sims(WolframLibraryData libraryData,
                                     mint argumentCount, MArgument *arguments,
                                     MArgument result);
/**
 * @brief Compute the orbit of one point under a supplied generator set.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 3.
 * @param arguments `{point, flattenedGenerators, degree}`; points are one-based.
 * @param result Integer vector of orbit points.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @see one_orbit
 */
EXTERN_C DLLEXPORT int LL_orbit(WolframLibraryData libraryData,
                              mint argumentCount, MArgument *arguments,
                              MArgument result);
/**
 * @brief Compute a setwise stabilizer with xPerm's subgroup search.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 4.
 * @param arguments `{points, degree, base, flattenedStrongGenerators}`.
 * @param result Encoded strong generating set of the setwise stabilizer.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @note A setwise stabilizer may permute the points amongst themselves. For a
 * pointwise stabilizer, use LL_stabsgs().
 */
EXTERN_C DLLEXPORT int LL_set_stabilizer(WolframLibraryData libraryData,
                                       mint argumentCount, MArgument *arguments,
                                       MArgument result);
/**
 * @brief Change the base and return every stabilizer-chain level.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 4.
 * @param arguments `{degree, base, flattenedStrongGenerators, requestedBase}`.
 * @param result Length-prefixed stream of encoded strong generating sets.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @see LL_basechange
 */
EXTERN_C DLLEXPORT int LL_basechangestabchain(WolframLibraryData libraryData,
                                           mint argumentCount,
                                           MArgument *arguments,
                                           MArgument result);
/**
 * @brief Change the base and return the group at the first chain level.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 4.
 * @param arguments `{degree, base, flattenedStrongGenerators, requestedBase}`.
 * @param result Encoded strong generating set with the changed base.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 */
EXTERN_C DLLEXPORT int LL_basechange(WolframLibraryData libraryData,
                                   mint argumentCount, MArgument *arguments,
                                   MArgument result);
/**
 * @brief Fix each requested point and return its pointwise stabilizer.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 4.
 * @param arguments `{degree, base, flattenedStrongGenerators, points}`.
 * @param result Encoded strong generating set after the fixed base prefix.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 */
EXTERN_C DLLEXPORT int LL_stabsgs(WolframLibraryData libraryData,
                                mint argumentCount, MArgument *arguments,
                                MArgument result);

/**
 * @brief Expose the staged persistent slot-group search for inspection.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 7.
 * @param arguments Seven staged-search arrays described in @ref librarylink_stages.
 * @param result Padded integer matrix with row tags 0 through 4.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @note This diagnostic helper is not the production Niehoff entry point.
 * @see xperm::niehoff::runPersistentGroupSearch
 */
EXTERN_C DLLEXPORT int LL_niehoff_persistent_search(WolframLibraryData libraryData,
                                                 mint argumentCount,
                                                 MArgument *arguments,
                                                 MArgument result);
/**
 * @brief Add explicit label-group actions to the staged search.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 8.
 * @param arguments Seven staged-search arrays followed by a label-group matrix.
 * @param result Padded integer matrix with row tags 0 through 4.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @see librarylink_stages
 * @see xperm::niehoff::runLabelGroupSearch
 */
EXTERN_C DLLEXPORT int LL_niehoff_label_group_search(WolframLibraryData libraryData,
                                                  mint argumentCount,
                                                  MArgument *arguments,
                                                  MArgument result);
/**
 * @brief Expose bounded, explicit propagated-symmetry orbit exploration.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 10.
 * @param arguments Eight label-search arguments, signed-generator matrix, and
 * positive maximum orbit size; see @ref librarylink_stages.
 * @param result Tagged integer matrix, including tag 5 for zero/merge status.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @warning This staged helper enumerates an explicitly supplied symmetry orbit.
 * It is not the incremental propagation/pruning implementation of Algorithm 4.2
 * used by LL_niehoff_canonical_perm().
 * @see xperm::niehoff::runPropagatedSymmetrySearch
 */
EXTERN_C DLLEXPORT int LL_niehoff_propagated_symmetry_search(
    WolframLibraryData libraryData, mint argumentCount, MArgument *arguments,
    MArgument result);
/**
 * @brief Run the original Butler–Portugal canonicalizer through LibraryLink.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 11.
 * @param arguments The xPerm-compatible layout in @ref librarylink_canonical_input.
 * @param result Signed image permutation, or the legacy zero-result vector.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @details Validates the shared input contract before calling
 * canonical_perm_ext(). This is the paclet's default backend (`Niehoff=False`);
 * it does not call the old WSTP executable. See @ref butler_portugal.
 */
EXTERN_C DLLEXPORT int LL_canonical_perm(WolframLibraryData libraryData,
                                      mint argumentCount, MArgument *arguments,
                                      MArgument result);
/**
 * @brief Run the production accelerated Niehoff canonicalizer.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 11.
 * @param arguments The xPerm-compatible layout in @ref librarylink_canonical_input.
 * @param result Signed image permutation, or `degree` zeros for a zero expression.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @details The word "legacy" in canonicalizeLegacyInput refers to the input
 * format, not to the selected algorithm. This entry point includes automatic
 * total-symmetry inference and the production acceleration. See
 * @ref niehoff_algorithm and @ref algorithm_overview.
 * @see xperm::niehoff::canonicalizeLegacyInput
 */
EXTERN_C DLLEXPORT int LL_niehoff_canonical_perm(WolframLibraryData libraryData,
                                              mint argumentCount,
                                              MArgument *arguments,
                                              MArgument result);
/**
 * @brief Run Niehoff canonicalization with additional declared total symmetries.
 * @param libraryData Wolfram runtime callbacks.
 * @param argumentCount Must be 14.
 * @param arguments The 11 canonicalization arguments followed by subset offsets,
 * flattened subset slots, and subset signs; see @ref librarylink_canonical_input.
 * @param result Signed image permutation, or `degree` zeros for a zero expression.
 * @return A LibraryLink status code, as described in @ref librarylink_api.
 * @warning Declared subsets add symmetry generators to the mathematical group;
 * they are not unchecked performance hints for an otherwise unchanged problem.
 * @see xperm::niehoff::canonicalizeExtendedInput
 */
EXTERN_C DLLEXPORT int LL_niehoff_canonical_perm_ext(WolframLibraryData libraryData,
                                                  mint argumentCount,
                                                  MArgument *arguments,
                                                  MArgument result);
/** @} */

#endif
