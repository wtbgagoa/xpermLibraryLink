/*********************************************************************
 *********************************************************************
 *********************************************************************
 *
 *  xperm.c
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
 *  August 2022. Port to LibraryLink API (Alfonso García-Parrado).
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


#include "xperm.h"
#include "WolframLibrary.h"

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


void ML_schreier_sims( int *base, long bl, int *GS, long m, int n, WSLINK stdlink) {

	int newbase[n];
	int nbl;
	int **newGS = NULL;
	int *pointer = NULL;
	int nm;
	int num=0;

	pointer = malloc(m*n*sizeof(int));
	newGS = &pointer;

	schreier_sims(base, bl, GS, m/n, n, 
		newbase, &nbl, newGS, &nm, &num);
	WSPutFunction(stdlink, "StrongGenSet", 3);
	WSPutIntegerList(stdlink, newbase, nbl);
	WSPutIntegerList(stdlink, *newGS, nm*n);
	WSPutInteger(stdlink, n);

	free(pointer);

	return;

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

	int orbit[n];
	int ol;

	one_orbit(point, GS, m/n, n, orbit, &ol);
	WSPutIntegerList(stdlink, orbit, ol);
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

	int *cperm= (int*)malloc(nn*sizeof(int));
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

	free(cperm);
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

        int num=0;
        int *charac=NULL, i;
        int *pointer=NULL;
        int **GSK=NULL, mK;
        pointer= malloc(m*sizeof(int));
        GSK = &pointer;
        charac= malloc(n*sizeof(int));

        /* Convert list of points into a characteristic function */
        zeros(charac,n);
        for(i=0; i<nn; i++) {
                charac[list[i]-1]=1;
        }

        /* Note that even though we send a characteristic function of
           n points, we send the length nn of the original list. We
           need both in the computations */
	search(base, bl, GS, m/n, n, 4, charac, nn, 1, GSK, &mK, &num);

	WSPutFunction(stdlink, "StrongGenSet", 3);
	WSPutIntegerList(stdlink, base, bl);
	WSPutIntegerList(stdlink, *GSK, mK*n);
	WSPutInteger(stdlink, n);

        free(pointer);
        free(charac);
        
	return;

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


void ML_basechangestabchain(
	int n,
	int *base, long bl,
	int *GS, long m,
	int *newbase, long nn, WSLINK stdlink) {
	
	int i, k;
	int *cl=NULL;
	int **chain=NULL;
	int *GSK=NULL;
	int nbl;
	int *nGS=NULL;
	int gslen=m/n;
	int *nbase = NULL;

	nbase = (int *)malloc(bl*sizeof(int));
	cl=(int *)malloc(bl*sizeof(int));
	chain=(int* *)malloc(bl*sizeof(int*));
	nGS=(int *)malloc(m*sizeof(int));

	nbl=bl;

	zeros(cl,nbl);

	for(i=0; i < nbl; i++) {
		chain[i]=NULL;
	}

	stab_chain(base, nbl, GS, gslen, n, chain, cl); 
	
	memmove(nbase, base, nbl*sizeof(int));
	memmove(nGS, GS, m*sizeof(int));
	
	basechange_chain(&nbase, &nbl, &nGS, &gslen, n, &chain, &cl, newbase, nn);

	GSK=(int *)malloc(gslen*n*sizeof(int));	

	WSPutFunction(stdlink, "List", nbl);

	for(i=0; i < nbl; i++) {
		for(k=0; k < cl[i]; k++) {
			memmove(&GSK[n*k], &nGS[n*(chain[i][k])], n*sizeof(int));
		}

		WSPutFunction(stdlink, "StrongGenSet", 3);
		WSPutIntegerList(stdlink, nbase+i, nbl-i);
		WSPutIntegerList(stdlink, GSK, n*cl[i]);
		WSPutInteger(stdlink, n);
	}

	for(i=0; i < nbl; i++) {
		free(chain[i]);
	}
	free(nbase);
	free(nGS);
	free(GSK);
	free(cl);
	free(chain);

	return;

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


void ML_basechange(
	int n,
	int *base, long bl,
	int *GS, long m,
	int *newbase, long nn, WSLINK stdlink) {
	
	int i, k;
	int *cl=NULL;
	int **chain=NULL;
	int *GSK=NULL;
	int nbl;
	int *nGS=NULL;
	int gslen=m/n;
	int *nbase = NULL;

	nbase = (int *)malloc(bl*sizeof(int));
	cl=(int *)malloc(bl*sizeof(int));
	chain=(int* *)malloc(bl*sizeof(int*));
	nGS=(int *)malloc(m*sizeof(int));

	nbl=bl;

	zeros(cl,bl);

	for(i=0; i < nbl; i++) {
		chain[i]=NULL;
	}

	stab_chain(base, nbl, GS, gslen, n, chain, cl); 
	
	memmove(nbase, base, bl*sizeof(int));
	memmove(nGS, GS, m*sizeof(int));
	
	basechange_chain(&nbase, &nbl, &nGS, &gslen, n, &chain, &cl, newbase, nn);

	GSK=(int *)malloc(gslen*n*sizeof(int));

	for(k=0; k < cl[0]; k++) {
		memmove(&GSK[n*k], &nGS[n*(chain[0][k])], n*sizeof(int));
	}
	WSPutFunction(stdlink, "StrongGenSet", 3);
	WSPutIntegerList(stdlink, nbase, nbl);
	WSPutIntegerList(stdlink, GSK, n*cl[0]);
	WSPutInteger(stdlink, n);
	

	for(i=0; i < nbl; i++) {
		free(chain[i]);
	}
	free(nbase);
	free(nGS);
	free(GSK);
	free(cl);
	free(chain);

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




void ML_stabsgs(
	int n,
	int *base, long bl,
	int *GS, long m,
	int *pts, long ptsl, WSLINK stdlink) {
	
	int i, k;
	int *cl=NULL;
	int **chain=NULL;
	int *GSK=NULL;
	int nbl;
	int tmpbasel;
	int *tmpbase=NULL;
	int *nGS=NULL;
	int gslen=m/n;
	int *nbase = NULL;

	tmpbase= (int *)malloc((bl+ptsl)*sizeof(int));
	nbase = (int *)malloc(bl*sizeof(int));
	cl=(int *)malloc(bl*sizeof(int));
	chain=(int* *)malloc(bl*sizeof(int*));
	nGS=(int *)malloc(m*sizeof(int));

	nbl=bl;

	zeros(cl,bl);

	for(i=0; i < nbl; i++) {
		chain[i]=NULL;
	}

	stab_chain(base, nbl, GS, gslen, n, chain, cl); 
	
	memmove(nbase, base, bl*sizeof(int));
	memmove(nGS, GS, m*sizeof(int));
	
	/* We want to keep the order of the remaining base points  - updated 2014-09-24 */
	
	memmove(tmpbase, pts, ptsl*sizeof(int));
	tmpbasel=ptsl;
	
	for(k=0; k < bl; k++) {
		if(!position(base[k], pts, ptsl)){
			tmpbase[tmpbasel++]=base[k];
		};
	}
	
	basechange_chain(&nbase, &nbl, &nGS, &gslen, n, &chain, &cl, tmpbase, tmpbasel);

	GSK=(int *)malloc(gslen*n*sizeof(int));

	for(k=0; k < cl[ptsl]; k++) {
		memmove(&GSK[n*k], &nGS[n*(chain[ptsl][k])], n*sizeof(int));
	}
	WSPutFunction(stdlink, "StrongGenSet", 3);
	WSPutIntegerList(stdlink, &(nbase[ptsl]), nbl-ptsl);
	WSPutIntegerList(stdlink, GSK, n*cl[ptsl]);
	WSPutInteger(stdlink, n);
	

	for(i=0; i < nbl; i++) {
		free(chain[i]);
	}
	free(nbase);
	free(nGS);
	free(GSK);
	free(cl);
	free(chain);

}

