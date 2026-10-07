#include "xperm.h"
#include "LLInterface.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>



/********************************************************************** 
 *                           INTERFACE                                *
 **********************************************************************/

/**********************************************************************/
EXTERN_C DLLEXPORT int LL_schreier_sims(WolframLibraryData libData, WSLINK wslp){
	int * _tp1;
	long _tpl1;
	int * _tp2;
	long _tpl2;
	int _tp3;
	long int len;

	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp1, &_tpl1) ) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp2, &_tpl2) ) goto L1;
	if ( ! WSGetInteger( wslp, &_tp3) ) goto L2;
	if ( ! WSNewPacket(wslp) ) goto L3;

	ML_schreier_sims(_tp1, _tpl1, _tp2, _tpl2, _tp3, wslp);

	return LIBRARY_NO_ERROR;

	L3: L2:	WSReleaseInteger32List( wslp, _tp2, _tpl2);
	L1:	WSReleaseInteger32List( wslp, _tp1, _tpl1);

	L0:	return LIBRARY_FUNCTION_ERROR;
}


void ML_schreier_sims(
    int *base, long bl,
    int *GS, long m,
    int n, WSLINK stdlink) {

    std::vector<int> newBase(static_cast<std::size_t>(n));
    std::vector<int> newGS(GS, GS + m);
    int newBaseLength;
    int newGeneratorCount;
    int iterationCount = 0;

    schreier_sims(
        base,
        static_cast<int>(bl),
        GS,
        static_cast<int>(m / n),
        n,
        newBase.data(),
        &newBaseLength,
        newGS,
        &newGeneratorCount,
        &iterationCount);

    WSPutFunction(stdlink, "StrongGenSet", 3);
    WSPutIntegerList(stdlink, newBase.data(), newBaseLength);
    WSPutIntegerList(
        stdlink,
        newGS.data(),
        newGeneratorCount * n);
    WSPutInteger(stdlink, n);
}

/**********************************************************************/

EXTERN_C DLLEXPORT int LL_orbit(WolframLibraryData libData, WSLINK wslp){
	int _tp1;
	int * _tp2;
	long _tpl2;
	int _tp3;
	long int len;
	
	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetInteger( wslp, &_tp1) ) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp2, &_tpl2) ) goto L1;
	if ( ! WSGetInteger( wslp, &_tp3) ) goto L2;
	if ( ! WSNewPacket(wslp) ) goto L3;

	ML_orbit(_tp1, _tp2, _tpl2, _tp3, wslp);
	return LIBRARY_NO_ERROR;

	L3: L2:	WSReleaseInteger32List( wslp, _tp2, _tpl2);
	L1: 
	L0:
	return LIBRARY_FUNCTION_ERROR;
}

void ML_orbit( int point, int *GS, long m, int n, WSLINK stdlink) {

	int *orbit = new int[n];
	int ol;

	one_orbit(point, GS, m/n, n, orbit, &ol);
	WSPutIntegerList(stdlink, orbit, ol);

	delete  [] orbit;

	return;
}

/**********************************************************************/
EXTERN_C DLLEXPORT int LL_canonical_perm(WolframLibraryData libData, WSLINK wslp)
{
	int * _tp1;
	long _tpl1;
	int _tp2;
	int _tp3;
	int * _tp4;
	long _tpl4;
	int * _tp5;
	long _tpl5;
	int * _tp6;
	long _tpl6;
	int * _tp7;
	long _tpl7;
	int * _tp8;
	long _tpl8;
	int * _tp9;
	long _tpl9;
	int * _tp10;
	long _tpl10;
	int * _tp11;
	long _tpl11;
	long int len;
	
	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp1, &_tpl1) ) goto L0;
	if ( ! WSGetInteger( wslp, &_tp2) ) goto L1;
	if ( ! WSGetInteger( wslp, &_tp3) ) goto L2;
	if ( ! WSGetIntegerList( wslp, &_tp4, &_tpl4) ) goto L3;
	if ( ! WSGetIntegerList( wslp, &_tp5, &_tpl5) ) goto L4;
	if ( ! WSGetIntegerList( wslp, &_tp6, &_tpl6) ) goto L5;
	if ( ! WSGetIntegerList( wslp, &_tp7, &_tpl7) ) goto L6;
	if ( ! WSGetIntegerList( wslp, &_tp8, &_tpl8) ) goto L7;
	if ( ! WSGetIntegerList( wslp, &_tp9, &_tpl9) ) goto L8;
	if ( ! WSGetIntegerList( wslp, &_tp10, &_tpl10) ) goto L9;
	if ( ! WSGetIntegerList( wslp, &_tp11, &_tpl11) ) goto L10;
	if ( ! WSNewPacket(wslp) ) goto L11;

	ML_canonical_perm(_tp1, _tpl1, _tp2, _tp3, _tp4, _tpl4, _tp5, _tpl5, _tp6, _tpl6, _tp7, _tpl7, _tp8, _tpl8, _tp9, _tpl9, _tp10, _tpl10, _tp11, _tpl11, wslp);

	return LIBRARY_NO_ERROR;
	L11:
	WSReleaseInteger32List( wslp, _tp11, _tpl11);
	L10:
	WSReleaseInteger32List( wslp, _tp10, _tpl10);
	L9:
	WSReleaseInteger32List( wslp, _tp9, _tpl9);
	L8:
	WSReleaseInteger32List( wslp, _tp8, _tpl8);
	L7:
	WSReleaseInteger32List( wslp, _tp7, _tpl7);
	L6:
	WSReleaseInteger32List( wslp, _tp6, _tpl6);
	L5:
	WSReleaseInteger32List( wslp, _tp5, _tpl5);
	L4:
	WSReleaseInteger32List( wslp, _tp4, _tpl4);
	L3: L2: L1:
	WSReleaseInteger32List( wslp, _tp1, _tpl1);

	L0:
	return LIBRARY_FUNCTION_ERROR;
}


void ML_canonical_perm(
        int *perm, long nn,
        int deg,
	int SGSQ,
        int *base, long bl,
        int *GS, long m,
	int *freeps, long fl,
        int *vds, long vdsl,
        int *dummies, long dl,
        int *mQ, long mQl,
        int *vrs, long vrsl,
        int *repes, long rl, WSLINK stdlink) {

	int *cperm= new int[nn];
	int error;

	canonical_perm_ext(
                perm, deg,
                SGSQ, base, bl, GS, m/deg,
		freeps, fl,
                vds, vdsl, dummies, dl, mQ,
                vrs, vrsl, repes, rl, 
                cperm);

        error = WSError(stdlink);
	if(error) {
		WSPutFunction(stdlink, "Print", 1);
		WSPutString(stdlink, WSErrorMessage(stdlink));
	} else {
		WSPutFunction(stdlink, "xAct`xPerm`Private`ToSign", 2);
		WSPutFunction(stdlink, "Images", 1);
		WSPutIntegerList(stdlink, cperm, deg);
		WSPutInteger(stdlink, deg-2);
	}

	delete [] cperm;
	return;
}

/**********************************************************************/

EXTERN_C DLLEXPORT int LL_set_stabilizer(WolframLibraryData libData, WSLINK wslp)
{
	int * _tp1;
	long _tpl1;
	int _tp2;
	int * _tp3;
	long _tpl3;
	int * _tp4;
	long _tpl4;
	long int len;
	
	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp1, &_tpl1) ) goto L0;
	if ( ! WSGetInteger( wslp, &_tp2) ) goto L1;
	if ( ! WSGetIntegerList( wslp, &_tp3, &_tpl3) ) goto L2;
	if ( ! WSGetIntegerList( wslp, &_tp4, &_tpl4) ) goto L3;
	if ( ! WSNewPacket(wslp) ) goto L4;

	ML_set_stabilizer(_tp1, _tpl1, _tp2, _tp3, _tpl3, _tp4, _tpl4, wslp);

	return LIBRARY_NO_ERROR;

	L4:
	WSReleaseInteger32List( wslp, _tp4, _tpl4);
	L3:
	WSReleaseInteger32List( wslp, _tp3, _tpl3);
	L2: L1:
	WSReleaseInteger32List( wslp, _tp1, _tpl1);

	L0:
	return LIBRARY_FUNCTION_ERROR;
} 

void ML_set_stabilizer(
    int *list, long nn,
    int n,
    int *base, long bl,
    int *GS, long m, WSLINK stdlink) {

    int iterationCount = 0;
    int subgroupGeneratorCount = 0;
    std::vector<int> characteristic(static_cast<std::size_t>(n), 0);
    std::vector<int> subgroupGenerators;
    subgroupGenerators.reserve(static_cast<std::size_t>(m));

    for (long i = 0; i < nn; ++i) {
        characteristic[static_cast<std::size_t>(list[i] - 1)] = 1;
    }

    /* Although characteristic has n entries, nn is the length of the
       original point list and is also required by the search. */
    search(
        base,
        static_cast<int>(bl),
        GS,
        static_cast<int>(m / n),
        n,
        4,
        characteristic.data(),
        static_cast<int>(nn),
        1,
        subgroupGenerators,
        &subgroupGeneratorCount,
        &iterationCount);

    WSPutFunction(stdlink, "StrongGenSet", 3);
    WSPutIntegerList(stdlink, base, static_cast<int>(bl));
    WSPutIntegerList(
        stdlink,
        subgroupGenerators.data(),
        subgroupGeneratorCount * n);
    WSPutInteger(stdlink, n);
}

/**********************************************************************/

EXTERN_C DLLEXPORT int LL_basechangestabchain(WolframLibraryData libData, WSLINK wslp)
{
	int _tp1;
	int * _tp2;
	long _tpl2;
	int * _tp3;
	long _tpl3;
	int * _tp4;
	long _tpl4;
	long int len;
	
	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetInteger( wslp, &_tp1) ) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp2, &_tpl2) ) goto L1;
	if ( ! WSGetIntegerList( wslp, &_tp3, &_tpl3) ) goto L2;
	if ( ! WSGetIntegerList( wslp, &_tp4, &_tpl4) ) goto L3;
	if ( ! WSNewPacket(wslp) ) goto L4;

	ML_basechangestabchain(_tp1, _tp2, _tpl2, _tp3, _tpl3, _tp4, _tpl4, wslp);

	return LIBRARY_NO_ERROR;

	L4:
	WSReleaseInteger32List( wslp, _tp4, _tpl4);
	L3:
	WSReleaseInteger32List( wslp, _tp3, _tpl3);
	L2:
	WSReleaseInteger32List( wslp, _tp2, _tpl2);
	L1: 
	L0:
	return LIBRARY_FUNCTION_ERROR;
} 


void ML_basechangestabchain(int n, int *base, long bl, int *GS, long m,
    int *newbase, long nn, WSLINK stdlink) {
    std::vector<int> changedBase(base, base + bl);
    std::vector<int> changedGS(GS, GS + m);
    StabilizerChain chain;
    stab_chain(changedBase.data(), static_cast<int>(changedBase.size()),
        changedGS.data(), static_cast<int>(changedGS.size() / n), n, chain);
    basechange_chain(changedBase, changedGS, n, chain, newbase, static_cast<int>(nn));
    WSPutFunction(stdlink, "List", static_cast<int>(changedBase.size()));
    for (std::size_t i = 0; i < changedBase.size(); ++i) {
        const auto& level = chain[i];
        std::vector<int> levelGS(level.size() * static_cast<std::size_t>(n));
        for (std::size_t k = 0; k < level.size(); ++k)
            std::copy_n(changedGS.data() + static_cast<std::size_t>(n) * level[k], n,
                        levelGS.data() + static_cast<std::size_t>(n) * k);
        WSPutFunction(stdlink, "StrongGenSet", 3);
        WSPutIntegerList(stdlink, changedBase.data() + i,
                         static_cast<int>(changedBase.size() - i));
        WSPutIntegerList(stdlink, levelGS.data(), static_cast<int>(levelGS.size()));
        WSPutInteger(stdlink, n);
    }
}

/**********************************************************************/
EXTERN_C DLLEXPORT int LL_basechange(WolframLibraryData libData, WSLINK wslp)
{
	int _tp1;
	int * _tp2;
	long _tpl2;
	int * _tp3;
	long _tpl3;
	int * _tp4;
	long _tpl4;
	long int len;

	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetInteger( wslp, &_tp1) ) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp2, &_tpl2) ) goto L1;
	if ( ! WSGetIntegerList( wslp, &_tp3, &_tpl3) ) goto L2;
	if ( ! WSGetIntegerList( wslp, &_tp4, &_tpl4) ) goto L3;
	if ( ! WSNewPacket(wslp) ) goto L4;

	ML_basechange(_tp1, _tp2, _tpl2, _tp3, _tpl3, _tp4, _tpl4, wslp);
	return LIBRARY_NO_ERROR;
	
	L4:
	WSReleaseInteger32List( wslp, _tp4, _tpl4);
	L3:
	WSReleaseInteger32List( wslp, _tp3, _tpl3);
	L2:
	WSReleaseInteger32List( wslp, _tp2, _tpl2);
	L1: 
	L0:
	return LIBRARY_FUNCTION_ERROR;
} 


void ML_basechange(int n, int *base, long bl, int *GS, long m,
    int *newbase, long nn, WSLINK stdlink) {
    std::vector<int> changedBase(base, base + bl);
    std::vector<int> changedGS(GS, GS + m);
    StabilizerChain chain;
    stab_chain(changedBase.data(), static_cast<int>(changedBase.size()),
        changedGS.data(), static_cast<int>(changedGS.size() / n), n, chain);
    basechange_chain(changedBase, changedGS, n, chain, newbase, static_cast<int>(nn));
    const auto& level = chain.front();
    std::vector<int> levelGS(level.size() * static_cast<std::size_t>(n));
    for (std::size_t k = 0; k < level.size(); ++k)
        std::copy_n(changedGS.data() + static_cast<std::size_t>(n) * level[k], n,
                    levelGS.data() + static_cast<std::size_t>(n) * k);
    WSPutFunction(stdlink, "StrongGenSet", 3);
    WSPutIntegerList(stdlink, changedBase.data(), static_cast<int>(changedBase.size()));
    WSPutIntegerList(stdlink, levelGS.data(), static_cast<int>(levelGS.size()));
    WSPutInteger(stdlink, n);
}



/**********************************************************************/

EXTERN_C DLLEXPORT int LL_stabsgs(WolframLibraryData libData, WSLINK wslp)
{
	int _tp1;
	int * _tp2;
	long _tpl2;
	int * _tp3;
	long _tpl3;
	int * _tp4;
	long _tpl4;
	long int len;

	if ( ! WSCheckFunction( wslp, "List", &len)) goto L0;
	if ( ! WSGetInteger( wslp, &_tp1) ) goto L0;
	if ( ! WSGetIntegerList( wslp, &_tp2, &_tpl2) ) goto L1;
	if ( ! WSGetIntegerList( wslp, &_tp3, &_tpl3) ) goto L2;
	if ( ! WSGetIntegerList( wslp, &_tp4, &_tpl4) ) goto L3;
	if ( ! WSNewPacket(wslp) ) goto L4;

	ML_stabsgs(_tp1, _tp2, _tpl2, _tp3, _tpl3, _tp4, _tpl4, wslp);
	return LIBRARY_NO_ERROR;

	L4:
	WSReleaseInteger32List( wslp, _tp4, _tpl4);
	L3:
	WSReleaseInteger32List( wslp, _tp3, _tpl3);
	L2:
	WSReleaseInteger32List( wslp, _tp2, _tpl2);
	L1: 
	L0:
	return LIBRARY_FUNCTION_ERROR;
} 




void ML_stabsgs(int n, int *base, long bl, int *GS, long m,
    int *pts, long ptsl, WSLINK stdlink) {
    std::vector<int> changedBase(base, base + bl);
    std::vector<int> changedGS(GS, GS + m);
    StabilizerChain chain;
    stab_chain(changedBase.data(), static_cast<int>(changedBase.size()),
        changedGS.data(), static_cast<int>(changedGS.size() / n), n, chain);
    std::vector<int> requestedBase(pts, pts + ptsl);
    requestedBase.reserve(static_cast<std::size_t>(ptsl + bl));
    for (long k = 0; k < bl; ++k)
        if (!position(base[k], pts, static_cast<int>(ptsl))) requestedBase.push_back(base[k]);
    basechange_chain(changedBase, changedGS, n, chain, requestedBase.data(),
                     static_cast<int>(requestedBase.size()));
    const std::size_t levelIndex = static_cast<std::size_t>(ptsl);
    const auto& level = chain[levelIndex];
    std::vector<int> levelGS(level.size() * static_cast<std::size_t>(n));
    for (std::size_t k = 0; k < level.size(); ++k)
        std::copy_n(changedGS.data() + static_cast<std::size_t>(n) * level[k], n,
                    levelGS.data() + static_cast<std::size_t>(n) * k);
    WSPutFunction(stdlink, "StrongGenSet", 3);
    WSPutIntegerList(stdlink, changedBase.data() + levelIndex,
                     static_cast<int>(changedBase.size() - levelIndex));
    WSPutIntegerList(stdlink, levelGS.data(), static_cast<int>(levelGS.size()));
    WSPutInteger(stdlink, n);
}

