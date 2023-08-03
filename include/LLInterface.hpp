#include "wstp.h"
#include "WolframLibrary.h"

/********************************************************************** 
 *                           INTERFACE                                *
 **********************************************************************/

/**********************************************************************/

EXTERN_C DLLEXPORT int LL_schreier_sims(WolframLibraryData, WSLINK);

void ML_schreier_sims ( intp_nt, long, intp_nt, long, int, WSLINK);
/*
:Begin:
:Function:      ML_schreier_sims
:Pattern:       xAct`xPerm`Private`MLSchreierSims[ xAct`xPerm`Private`base_List, xAct`xPerm`Private`GS_List, xAct`xPerm`Private`n_Integer]
:Arguments:     {xAct`xPerm`Private`base, xAct`xPerm`Private`GS, xAct`xPerm`Private`n}
:ArgumentTypes: {IntegerList, IntegerList, Integer}
:ReturnType:    Manual
:End:

:Evaluate:
*/

/**********************************************************************/

EXTERN_C DLLEXPORT int LL_orbit(WolframLibraryData, WSLINK);

void ML_orbit ( int, intp_nt, long, int, WSLINK);

/*
:Begin:
:Function:	ML_orbit
:Pattern:	xAct`xPerm`Private`MLOrbit[ xAct`xPerm`Private`point_Integer, xAct`xPerm`Private`GS_List, xAct`xPerm`Private`n_Integer]
:Arguments:	{xAct`xPerm`Private`point, xAct`xPerm`Private`GS, xAct`xPerm`Private`n}
:ArgumentTypes:	{Integer, IntegerList, Integer}
:ReturnType:	Manual
:End:

:Evaluate:
*/


/**********************************************************************/

EXTERN_C DLLEXPORT int LL_canonical_perm(WolframLibraryData, WSLINK);

void ML_canonical_perm ( 
        intp_nt, long, 
	int,
	int,
        intp_nt, long,
        intp_nt, long,
	intp_nt, long,
        intp_nt, long,
        intp_nt, long,
        intp_nt, long,
        intp_nt, long,
        intp_nt, long, WSLINK);

/*
:Begin:
:Function:      ML_canonical_perm
:Pattern:       xAct`xPerm`Private`MLCanonicalPerm[ 
        xAct`xPerm`Private`perm_List,
        xAct`xPerm`Private`deg_Integer,
        xAct`xPerm`Private`SGSQ_Integer,
        xAct`xPerm`Private`base_List,
        xAct`xPerm`Private`GS_List,
        xAct`xPerm`Private`freeps_List,
        xAct`xPerm`Private`vds_List,
        xAct`xPerm`Private`dummies_List,
        xAct`xPerm`Private`mQ_List,
        xAct`xPerm`Private`vrs_List,
        xAct`xPerm`Private`repes_List]
:Arguments:     { xAct`xPerm`Private`perm,
                  xAct`xPerm`Private`deg,
                  xAct`xPerm`Private`SGSQ,
                  xAct`xPerm`Private`base,
                  xAct`xPerm`Private`GS,
                  xAct`xPerm`Private`freeps,
                  xAct`xPerm`Private`vds,
                  xAct`xPerm`Private`dummies,
                  xAct`xPerm`Private`mQ,
                  xAct`xPerm`Private`vrs,
                  xAct`xPerm`Private`repes }
:ArgumentTypes: { IntegerList,
                  Integer,
                  Integer,
                  IntegerList,
                  IntegerList,
                  IntegerList,
                  IntegerList,
                  IntegerList,
                  IntegerList,
                  IntegerList,
                  IntegerList }
:ReturnType:    Manual
:End:

:Evaluate:
*/


/**********************************************************************/
EXTERN_C DLLEXPORT int LL_set_stabilizer(WolframLibraryData, WSLINK);

void ML_set_stabilizer ( 
        intp_nt, long, 
	int,
        intp_nt, long,
        intp_nt, long, WSLINK);
/*
:Begin:
:Function:      ML_set_stabilizer
:Pattern:       xAct`xPerm`Private`MLSetStabilizer[ 
        xAct`xPerm`Private`list_List,
        xAct`xPerm`Private`deg_Integer,
        xAct`xPerm`Private`base_List,
        xAct`xPerm`Private`GS_List]
:Arguments:     { xAct`xPerm`Private`list,
                  xAct`xPerm`Private`deg,
                  xAct`xPerm`Private`base,
                  xAct`xPerm`Private`GS }
:ArgumentTypes: { IntegerList,
                  Integer,
                  IntegerList,
                  IntegerList }
:ReturnType:    Manual
:End:

:Evaluate:
*/


/**********************************************************************/
/**********************************************************************/
EXTERN_C DLLEXPORT int LL_basechangestabchain(WolframLibraryData, WSLINK);

void ML_basechangestabchain ( 
	int,
	intp_nt, long,
	intp_nt, long,
	intp_nt, long, WSLINK );

/*
:Begin:
:Function:      ML_basechangestabchain
:Pattern:       xAct`xPerm`Private`MLBaseChangeStabilizerChain[ 
        xAct`xPerm`Private`deg_Integer,
        xAct`xPerm`Private`base_List,
        xAct`xPerm`Private`GS_List,
        xAct`xPerm`Private`newbase_List]
:Arguments:     { xAct`xPerm`Private`deg,
                  xAct`xPerm`Private`base,
                  xAct`xPerm`Private`GS,
			  xAct`xPerm`Private`newbase }
:ArgumentTypes: { Integer,
                  IntegerList,
                  IntegerList,
			  IntegerList }
:ReturnType:    Manual
:End:

:Evaluate:
*/

/**********************************************************************/
/**********************************************************************/

EXTERN_C DLLEXPORT int LL_basechange(WolframLibraryData, WSLINK);

void ML_basechange ( 
	int,
	intp_nt, long,
	intp_nt, long,
	intp_nt, long, WSLINK);

/*
:Begin:
:Function:      ML_basechange
:Pattern:       xAct`xPerm`Private`MLBaseChange[ 
        xAct`xPerm`Private`deg_Integer,
        xAct`xPerm`Private`base_List,
        xAct`xPerm`Private`GS_List,
        xAct`xPerm`Private`newbase_List]
:Arguments:     { xAct`xPerm`Private`deg,
                  xAct`xPerm`Private`base,
                  xAct`xPerm`Private`GS,
			  xAct`xPerm`Private`newbase }
:ArgumentTypes: { Integer,
                  IntegerList,
                  IntegerList,
			  IntegerList }
:ReturnType:    Manual
:End:

:Evaluate:
*/



/**********************************************************************/
EXTERN_C DLLEXPORT int LL_stabsgs(WolframLibraryData, WSLINK);

void ML_stabsgs ( int n, int* base, long int bl, int* GS, long int m, int* pts, long int ptsl, WSLINK stdlink );

/*
:Begin:
:Function:      ML_stabsgs
:Pattern:       xAct`xPerm`Private`MLStabilizerSGS[ 
        xAct`xPerm`Private`deg_Integer,
        xAct`xPerm`Private`base_List,
        xAct`xPerm`Private`GS_List,
        xAct`xPerm`Private`pts_List]
:Arguments:     { xAct`xPerm`Private`deg,
                  xAct`xPerm`Private`base,
                  xAct`xPerm`Private`GS,
			  xAct`xPerm`Private`pts }
:ArgumentTypes: { Integer,
                  IntegerList,
                  IntegerList,
			  IntegerList }
:ReturnType:    Manual
:End:

:Evaluate:
*/
