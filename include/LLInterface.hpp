#ifndef XPERM_LL_INTERFACE_HPP
#define XPERM_LL_INTERFACE_HPP

#include "WolframLibrary.h"

// Basic permutation-group operations.
EXTERN_C DLLEXPORT int LL_schreier_sims(WolframLibraryData, mint,
                                         MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_orbit(WolframLibraryData, mint,
                                MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_set_stabilizer(WolframLibraryData, mint,
                                         MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_basechangestabchain(WolframLibraryData, mint,
                                              MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_basechange(WolframLibraryData, mint,
                                     MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_stabsgs(WolframLibraryData, mint,
                                  MArgument *, MArgument);

// Canonicalization and staged Niehoff interfaces.
EXTERN_C DLLEXPORT int LL_niehoff_persistent_search(WolframLibraryData, mint,
                                                    MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_niehoff_label_group_search(WolframLibraryData, mint,
                                                     MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_niehoff_propagated_symmetry_search(
    WolframLibraryData, mint, MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_canonical_perm(WolframLibraryData, mint,
                                         MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_niehoff_canonical_perm(WolframLibraryData, mint,
                                                 MArgument *, MArgument);
EXTERN_C DLLEXPORT int LL_niehoff_canonical_perm_ext(WolframLibraryData, mint,
                                                     MArgument *, MArgument);

#endif
