
/*********************************************************************
 *********************************************************************
 *********************************************************************
 *
 *  xperm.h
 *
 * 	(C) Jose M. Martin-Garcia 2003-2011.
 *      jose@xAct.es
 *      http://www.xAct.es/
 *
 *	This is free software, distributed under the GNU GPL license.
 *      See http://metric.iem.csic.es/Martin-Garcia/xAct/
 *
 *  These are a collection of C-functions that find Strong Generating
 *  Sets, Coset representatives and Double-coset representatives.
 *  The algorithms are based on Butler and Portugal et al.
 *
 *  xperm can be used standalone or from the Mathematica package xPerm.
 *
 *  20 June - 5 July 2003. Main development.
 *   3 September 2004. Eliminated MAX variables.
 *   9 November 2005. Corrected treatment of SGS of group D.
 *   6 May 2006. All arrays declared dynamically to avoid stack limits.
 *     Thanks to Kasper Peeters for spotting and correcting this.
 *  25-28 June 2007. Large extension to included multiple dummysets and
 *     repeatedsets.
 *
 *  Main ideas:
 *      - Permutations are represented using Images notation.
 *      - Generating sets with m n-permutations are stored as lists of
 *	  length m*n.
 *	- #define VERBOSE_* to turn on log output.               *PPC*
 *
 *  Comments:
 *	- Permutations are assumed to have degree n>0.
 *	- Lists can have length 0 or positive.
 *
 *  This is ISO C99, not ANSI-C. There are some gcc extensions:
 *	- ISO C forbids nested functions
 *	- ISO C89 forbids mixed declarations and code
 *	- ISO C90 does not support `long long'
 *	- ISO C90 forbids variable-size arrays
 *
 *********************************************************************
 *********************************************************************
 *********************************************************************/

#pragma once

#include "wstp.h"
#include "WolframLibrary.h"


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

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




/*********************************************************************
 *                             PROTOTYPES                            *
 *********************************************************************/

/* Output */
void print_perm(int *p, int n, int nl);
void print_array_perm(int *perms, int m, int n, int nl);
void print_list(int *list, int n, int nl);
void print_array(int *array, int m, int n, int nl);

/* Lists */
int equal_list(int *list1, int *list2, int n);
void copy_list(int *list1, int *list2, int n);
int position(int i, int *list, int n);
int position_list(int *matrix, int m, int *row, int n);
void zeros(int *list, int n);
void range(int *list, int n);
void complement(int *all, int al, int *part, int pl, int n,
		int *com, int *cl);
void sort(int *list, int *slist, int l);
void sortB(int *list, int *slist, int l, int *B, int Bl);
int minim(int *list, int n);
int maxim(int *list, int n);
void intersection(int *list1, int l1, int *list2, int l2, int *list,
		int *l);

/* Permutations */
int isid(int *list, int n );
void product(int *p1, int *p2, int *p, int n);
void inverse(int *p, int *ip, int n);
int onpoints(int point, int *p, int n);
void stable_points(int *p, int n, int *list, int *m);
int first_nonstable_point(int *p, int n);
void nonstable_points(int *list1, int l1, int *GS, int m, int n,
	int *list2, int *l2);
void stabilizer(int *points, int k, int *GS, int m, int n, int *subGS, 
	int *mm);
void one_orbit(int point, int *GS, int m, int n, int *orbit, int *ol);
void all_orbits(int *GS, int m, int n, int *orbits);
void schreier_vector(int point, int *GS, int m, int n, int *nu, int *w);
void trace_schreier(int point, int *nu, int *w, int *perm, int n);

/* Algorithms */
long long int order_of_group(int *base, int bl, int *GS, int m, int n);
int perm_member(int *p, int *base, int bl, int *GS, int m, int n);
void schreier_sims_step(int *base, int bl, int *GS, int m, int n, int i,
	int *T, int mm, int *newbase, int *nbl, int **newGS, int *nm,
	int *num);
void schreier_sims(int *base, int bl, int *GS, int m, int n,
	int *newbase, int *nbl, int **newGS, int *nm, int *num);
void coset_rep(int *p, int n, int *base, int bl, int *GS, int *m,
	int *freeps, int fl, int *cr);
void SGSD(int *vds, int vdsl, int *dummies, int dl, int *mQ,
          int *vrs, int vrsl, int *repes, int rl, int n,
          int firstd, int *KD, int *KDl, int *bD, int *bDl);
void double_coset_rep(int *g, int n, int *base, int bl, int *GS, int m,
        int *vds, int vdsl, int *dummies, int dl, int *mQ,
        int *vrs, int vrsl, int *repes, int rl, int *dcr);
void canonical_perm(int *perm,
	int SGSQ, int *base, int bl, int *GS, int m, int n,
	int *freeps, int fl, int *dummyps, int dl, int ob, int metricQ,
	int *cperm);
void canonical_perm_ext(int *perm, int n,
	int SGSQ, int *base, int bl, int *GS, int m,
	int *frees, int fl,
        int *vds, int vdsl, int *dummies, int dl, int *mQ,
        int *vrs, int vrsl, int *repes, int rl,
	int *cperm);

void stab_chain(int *base, int bl, int *GS, int m, int n, int **chain, int *cl);

void one_orbit_chain(int point, int *GS, int *poslist, int poslistl, int n, int *orbit, int *ol);

void one_schreier_orbit_chain(int point, int *GS, int *poslist, int poslistl,
	int n, int *orbit, int *ol, int *nu, int *w, int init);

void conjugate_chain(int *base, int bl, int *GS, int m, int n, int *p);

void basechange_chain(int **base, int *bl, int **GS, int *m, int n, int ***chain, int **cl, int *newbase, int newbl);

void appendbasepoint_chain(int **base, int *bl, int ***chain, int **cl, int newbasepoint);

void interchange_chain(int **base, int *bl, int **GS, int *m, int n, int ***chain, int **cl, int j);

void search(int *base, int bl, int *GS, int m, int n, int prop, int *info, int infol,
        int s, int **GSK, int *mK, int *num);


/* Auxiliary variables & functions */

/* ALPHA */
	struct alphastruct {
		/* L */
		int *L; 	/* We assume that elements of L cannot
				   be repeated. I only have experimental
				   evidence for this */
		int Ll;
		/* s, d */
		int *s;
		int *d;
		/* other */
		int *o;				   
	};
	
/* TAB */
void TAB(int *L, int Ll, int *s1, int *d1, int n, struct alphastruct *ALPHA);

/* Subroutines F1 and F2 */
void F2(int *TAB1, int *g, int *TAB2, int *sgd, int n);

void F1(int *L, int Ll, int *g, int *list, int *listl, int n, int Deltabl, int *Deltab, int oi, int *DeltaD, struct alphastruct *ALPHA);
