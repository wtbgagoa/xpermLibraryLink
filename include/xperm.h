
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
 *	- ISO C90 does not support `long long`
 *	- ISO C90 forbids variable-size arrays
 *
 *********************************************************************
 *********************************************************************
 *********************************************************************/

/**
 * @file xperm.h
 * @brief Inherited xPerm permutation-group and Butler-Portugal interfaces.
 *
 * These routines use one-based mathematical point values in zero-based C++
 * buffers. A generating set is a flat row-major array of m permutations of
 * degree n. product() composes from left to right. Tensor canonicalization
 * reserves points n-1 and n for the sign; generic group routines have no such
 * requirement. See @ref representations and @ref butler_portugal.
 *
 * @warning These are low-level interfaces: sizes, permutations, group membership,
 * strongness of a supplied SGS, and writable output capacities are preconditions,
 * not checked contracts. The validated C++/LibraryLink adapters are preferable
 * for external callers. Do not pass the all-zero tensor result back as a group
 * permutation. Unless expressly supported, input and output buffers must not
 * overlap. Generic group operations require n > 0; signed tensor operations
 * require n >= 2 and preservation of the two-point sign set.
 *
 * The historical license and attribution above apply to this inherited code.
 */

/** @defgroup permutation_groups Permutation groups and stabilizer chains
 * @brief Shared xPerm infrastructure used by both canonicalization algorithms.
 * See @ref representations for storage and @ref butler_portugal for the older
 * search. These functions operate on permutations, not tensor expressions.
 */

/** @defgroup butler_portugal_api Butler-Portugal canonicalization
 * @brief Original xPerm coset and double-coset search, retained as an alternative.
 * See @ref butler_portugal for the algorithm walkthrough and its relation to
 * @ref niehoff_algorithm. The newer search still uses @ref permutation_groups.
 */

#include <vector>



/*********************************************************************
 *                             PROTOTYPES                            *
 *********************************************************************/

/* Debug output: available only in verbose builds. */
#if defined(VERBOSE_LISTS) || defined(VERBOSE_SCHREIER) || \
    defined(VERBOSE_COSET) || defined(VERBOSE_DOUBLE) || \
    defined(VERBOSE_CANON)
void print_perm(const int *p, int n, int nl);
void print_array_perm(const int *perms, int m, int n, int nl);
void print_list(const int *list, int n, int nl);
#endif

/* Lists */
/** @brief Return whether two buffers of n integers have identical entries. */
int equal_list(int *list1, int *list2, int n);
/** @brief Copy n integers; this implementation permits overlapping buffers. */
void copy_list(int *list1, int *list2, int n);
/** @brief Return a one-based position of i in list, or zero if absent. */
int position(int i, int *list, int n);
/** @brief Find a length-n row in an m-by-n flat matrix, returning one-based row or zero. */
int position_list(int *matrix, int m, int *row, int n);
/** @brief Fill n integers with zero; this is not an identity permutation. */
void zeros(int *list, int n);
/** @brief Write the degree-n identity permutation, {1, ..., n}. */
void range(int *list, int n);
void complement(int *all, int al, int *part, int pl, int n,
		int *com, int *cl);
void sort(int *list, int *slist, int l);
/** @brief Sort l entries using B first as the preferred point order, then numeric order. */
void sortB(int *list, int *slist, int l, int *B, int Bl);
void intersection(int *list1, int l1, int *list2, int l2, int *list,
		int *l);

/* Permutations */
/** @ingroup permutation_groups
 * @brief Test whether the degree-n images array is the identity.
 */
int isid(const int *list, int n );
/** @ingroup permutation_groups
 * @brief Compose p1 then p2: p[i-1] = p2[p1[i-1]-1].
 * @param p1 First degree-n permutation.
 * @param p2 Second degree-n permutation.
 * @param p Output buffer of n integers, distinct from the inputs.
 * @param n Permutation degree, including sign points when present.
 * @details Thus a slot action s, configuration g, and label action d yield
 * product(product(s,g),d), or S g D. Do not read this as right-to-left function
 * composition. The sign transposition composes by the same ordinary rule.
 */
void product(int *p1, int *p2, int *p, int n);
/** @ingroup permutation_groups
 * @brief Invert a degree-n permutation into a separate n-integer buffer.
 */
void inverse(int *p, int *ip, int n);
/** @ingroup permutation_groups
 * @brief Apply p to a one-based point: p[point-1].
 * @pre 1 <= point <= n and p contains a valid permutation.
 */
int onpoints(int point, int *p, int n);
void stable_points(int *p, int n, int *list, int *m);
int first_nonstable_point(int *p, int n);
void nonstable_points(int *list1, int l1, int *GS, int m, int n,
	int *list2, int *l2);
/** @ingroup permutation_groups
 * @brief Filter GS to the generators fixing all k given points.
 * @param points One-based points to fix.
 * @param k Number of points.
 * @param GS Flat degree-n input generators.
 * @param m Number of input generators.
 * @param n Positive permutation degree.
 * @param subGS Output capacity at least m*n integers; may be GS itself.
 * @param mm Output number of retained generators.
 * @warning This does not construct a stabilizer of an arbitrary generating set.
 * It describes the required group stabilizer only when the strong-generating-set
 * and base-prefix invariants hold. Both canonicalizers establish those invariants
 * before advancing their stabilizer chains.
 */
void stabilizer(int *points, int k, int *GS, int m, int n, int *subGS, 
	int *mm);
/** @ingroup permutation_groups
 * @brief Breadth-first orbit of point under m flat degree-n generators.
 * @param point One-based orbit root.
 * @param GS Flat degree-n input generators.
 * @param m Number of generators.
 * @param n Positive permutation degree.
 * @param orbit Output capacity n; the first element is point.
 * @param ol Output orbit length; equals one when m is zero.
 */
void one_orbit(int point, int *GS, int m, int n, int *orbit, int *ol);
/** @ingroup permutation_groups
 * @brief Label every point by its orbit number (starting at one).
 * @param GS Flat degree-n input generators.
 * @param m Number of generators.
 * @param n Positive permutation degree.
 * @param orbits Output array of n component identifiers, not an orbit list.
 */
void all_orbits(int *GS, int m, int n, int *orbits);
/** @ingroup permutation_groups
 * @brief Construct a Schreier forest, starting with the orbit of point.
 * @param point One-based preferred first root.
 * @param GS Flat degree-n input generators.
 * @param m Number of generators.
 * @param n Positive permutation degree.
 * @param nu Output n-by-n array: row x-1 stores the generator that discovered x.
 * @param w Output n-integer predecessor array; zero denotes an orbit root.
 * @details trace_schreier() composes the recorded edges to obtain a transversal.
 */
void schreier_vector(int point, int *GS, int m, int n, int *nu, int *w);
/** @ingroup permutation_groups
 * @brief Return a permutation sending the root of point's Schreier tree to point.
 * @param point One-based target point.
 * @param n Positive permutation degree.
 * @param nu Schreier edge-permutation rows, as produced by schreier_vector().
 * @param w Schreier predecessor points (zero at a root).
 * @param perm Output degree-n permutation.
 * @pre point belongs to a valid tree in the supplied Schreier forest.
 */
void trace_schreier(int point, int *nu, int *w, int *perm, int n);

/* Algorithms */
/** @ingroup permutation_groups
 * @brief Multiply basic-orbit sizes along a complete base/SGS chain.
 * @pre base and GS describe a complete strong generating set.
 * @warning The signed long long result has no overflow checking; large group
 * orders need a separate arbitrary-precision implementation.
 */
long long int order_of_group(int *base, int bl, int *GS, int m, int n);
/** @ingroup permutation_groups
 * @brief Sift p through a complete base/SGS; return one for membership, zero otherwise.
 * @details Each transversal cancels one base image, then the remainder is sifted
 * in the next point stabilizer. An empty base or empty GS accepts only identity.
 */
int perm_member(int *p, int *base, int bl, int *GS, int m, int n);
/** @ingroup permutation_groups
 * @brief Internal recursive Schreier-Sims extension at one-based level i.
 * @details Extends the tentative base and strong generators using the additional
 * generators T. Prefer schreier_sims(), which establishes the recursive invariant.
 */
void schreier_sims_step(int *base, int bl, int *GS, int m, int n, int i,
	int *T, int mm, int *newbase, int *nbl, std::vector<int>& newGS, int *nm,
	int *num);
/** @ingroup permutation_groups
 * @brief Build a base and strong generating set for the group generated by GS.
 * @param base Requested initial base points; the result may add points.
 * @param bl Length of the requested base prefix.
 * @param GS Flat input array of m degree-n generators.
 * @param m Number of input generators.
 * @param n Positive degree.
 * @param newbase Output buffer of at least n integers.
 * @param nbl Output base length.
 * @param newGS Owning output vector of flattened strong generators; may grow.
 * @param nm Output number of strong generators.
 * @param num In/out diagnostic counter used by the recursive construction.
 * @details Strongness is what makes later generator filtering a valid
 * stabilizer-chain operation. Redundant base points are permitted.
 */
void schreier_sims(int *base, int bl, int *GS, int m, int n,
	int *newbase, int *nbl, std::vector<int>& newGS, int *nm, int *num);
/** @ingroup butler_portugal_api
 * @brief Canonicalize the free-index part of the right coset S p.
 * @param p Input slot-to-label permutation.
 * @param n Degree including the two sign points.
 * @param base Base defining the inherited slot/label priority convention.
 * @param bl Base length.
 * @param GS In/out flat SGS; replaced by the retained stabilizer generators.
 * @param m In/out number of generators in GS.
 * @param freeps In/out slots containing free labels; updated after slot moves.
 * @param fl Number of free slots.
 * @param cr Output degree-n representative.
 * @details Follows the free-index stage of the Portugal construction. At each
 * base point it intersects the slot orbit with free slots, selects the least
 * available label using sortB(), applies a Schreier representative, then fixes
 * that base point. No dummy renaming occurs here.
 */
void coset_rep(int *p, int n, int *base, int bl, int *GS, int *m,
	int *freeps, int fl, int *cr);
/** @ingroup butler_portugal_api
 * @brief Construct the current label-group SGS from dummy and repeated sets.
 * @param vds Lengths of the vdsl dummy sets (numbers of labels, not pairs).
 * @param vdsl Number of dummy sets.
 * @param dummies In/out flattened ordered pairs; may be reordered for firstd.
 * @param dl Total length of dummies.
 * @param mQ Metric signs: +1 symmetric, -1 antisymmetric, 0 no metric.
 * @param vrs Lengths of the vrsl repeated sets.
 * @param vrsl Number of repeated sets.
 * @param repes In/out flattened repeated-label sets; may be reordered.
 * @param rl Total length of repes.
 * @param n Degree including sign points.
 * @param firstd Preferred first label, or zero to preserve the current ordering.
 * @param KD Output flat generators; n*n integers suffice for valid disjoint sets.
 * @param KDl Output generator count.
 * @param bD Output base buffer of n integers.
 * @param bDl Output base length.
 * @details Generates pair exchanges, metric-permitted within-pair flips, and
 * positive adjacent transpositions of repeated labels. Different sets never mix.
 * The Niehoff implementation replaces this repeated construction with explicit
 * label metadata and incremental Values/Label-Codes updates.
 */
void SGSD(int *vds, int vdsl, int *dummies, int dl, int *mQ,
          int *vrs, int vrsl, int *repes, int rl, int n,
          int firstd, int *KD, int *KDl, int *bD, int *bDl);
/** @ingroup butler_portugal_api
 * @brief Butler-Portugal search over the remaining double coset S g D.
 * @param g Slot-to-label configuration after free-index canonicalization.
 * @param n Degree including the two sign points.
 * @param base Base of the remaining slot stabilizer.
 * @param bl Base length.
 * @param GS Flat strong generators for that slot stabilizer.
 * @param m Number of generators.
 * @param vds In/out lengths of the dummy sets; consumed as labels are fixed.
 * @param vdsl Number of dummy sets.
 * @param dummies In/out flattened dummy pairs; reordered and shortened in place.
 * @param dl Initial dummy-label count.
 * @param mQ Metric signs for the dummy sets.
 * @param vrs In/out repeated-set lengths; consumed as labels are fixed.
 * @param vrsl Number of repeated sets.
 * @param repes In/out repeated-label buffer; reordered and shortened in place.
 * @param rl Initial repeated-label count.
 * @param dcr Output n-integer representative, or all zeros for a sign conflict.
 * @pre Free slots are already fixed by GS, and at least one dummy/repeated label
 * remains. Call canonical_perm_ext() to establish the two-stage setup.
 * @details ALPHA stores candidate paths and TAB pairs (s,d). The search selects
 * the least attainable label at each base point, retains every compatible path,
 * and checks whether two resulting configurations have equal real-slot images
 * but opposite signs. This retention of equivalent paths is the factorial case
 * that @ref niehoff_algorithm addresses using propagated total symmetries.
 */
void double_coset_rep(int *g, int n, int *base, int bl, int *GS, int m,
        int *vds, int vdsl, int *dummies, int dl, int *mQ,
        int *vrs, int vrsl, int *repes, int rl, int *dcr);
/** @ingroup butler_portugal_api
 * @brief Historical single-dummy-set adapter using index-to-slot permutations.
 * @param perm Index-to-slot signed configuration.
 * @param SGSQ One if the input generators are strong for base, otherwise zero.
 * @param base Slot base, or initial base points when SGSQ is zero.
 * @param bl Number of base points.
 * @param GS Flat array of m degree-n slot generators.
 * @param m Number of generators.
 * @param n Degree including the sign points.
 * @param freeps Initial slots of free labels.
 * @param fl Number of free slots.
 * @param dummyps Initial slots of ordered dummy pairs.
 * @param dl Number of dummy pairs (not labels) in this historical interface.
 * @param ob Ignored backward-compatibility argument.
 * @param metricQ Metric flag for the single dummy set: 0, +1, or -1.
 * @details Inverts the input and output around canonical_perm_ext(). freeps and
 * dummyps are initial slots; the dummy count parameter is a number of pairs
 * (despite the historical declaration name dl). ob is ignored. This convention
 * differs from canonical_perm_ext() and the LibraryLink canonicalization ABI.
 * @pre n = fl + 2*dl + 2, with valid signed input and one metric flag.
 * @param cperm Output degree-n permutation in the input convention, or all zeros.
 */
void canonical_perm(int *perm,
	int SGSQ, int *base, int bl, int *GS, int m, int n,
	int *freeps, int fl, int *dummyps, int dl, int ob, int metricQ,
	int *cperm);
/** @ingroup butler_portugal_api
 * @brief Original xPerm canonicalizer supporting multiple dummy/repeated sets.
 * @param perm Slot-to-label images permutation; final two points encode sign.
 * @param n Degree including sign points.
 * @param SGSQ One if GS is already strong for base, zero to compute an SGS.
 * @param base Associated base, or requested initial points when SGSQ is zero.
 * @param bl Base length.
 * @param GS Flat array of m degree-n slot generators.
 * @param m Number of generators.
 * @param frees Free-label identifiers (not initial slots).
 * @param fl Number of free labels.
 * @param vds In/out dummy-set lengths, each even; sum equals dl.
 * @param vdsl Number of dummy sets.
 * @param dummies In/out flattened dummy pairs; each pair's order encodes variance.
 * @param dl Number of dummy labels, twice the total number of dummy pairs.
 * @param mQ One metric flag per dummy set, each 0, +1, or -1.
 * @param vrs In/out repeated-set lengths; sum equals rl.
 * @param vrsl Number of repeated sets.
 * @param repes In/out flattened repeated-label sets.
 * @param rl Number of repeated labels.
 * @param cperm Output degree-n permutation, or all zeros for a vanishing tensor.
 * @details Constructs an SGS when necessary, runs coset_rep() on the free labels,
 * fixes their slots, then runs double_coset_rep(). The base and slot generators
 * are copied internally, but dummy/repeated buffers and lengths can be modified.
 * The inherited ordering is not the natural lexicographic ordering used by the
 * Niehoff entry points; compare equivalence classes rather than requiring their
 * returned representatives to be identical. See @ref butler_portugal.
 * @pre Labels in the free, dummy, and repeated lists are disjoint real points;
 * permutations preserve the sign pair and all stated lengths/capacities agree.
 */
void canonical_perm_ext(int *perm, int n,
	int SGSQ, int *base, int bl, int *GS, int m,
	int *frees, int fl,
        int *vds, int vdsl, int *dummies, int dl, int *mQ,
        int *vrs, int vrsl, int *repes, int rl,
	int *cperm);


/** @ingroup permutation_groups
 * @brief Owning integer buffer used by the inherited chain implementation.
 */
using IntList = std::vector<int>;
/** @ingroup permutation_groups
 * @brief At level i, zero-based generator row indices fixing base[0..i-1].
 * @details Point values remain one-based; these entries are storage row indices.
 * The chain indexes a separate flat GS vector and must be updated with it.
 */
using StabilizerChain = std::vector<IntList>;

/** @ingroup permutation_groups
 * @brief Index a valid SGS by successive base-prefix stabilizers.
 * @details Level zero includes every generator; later levels filter the preceding
 * level. No new group generators are constructed. Empty base gives an empty chain.
 */
void stab_chain(int *base, int bl, int *GS, int m, int n, StabilizerChain& chain);

/** @ingroup permutation_groups
 * @brief Compute an orbit using only zero-based GS rows named by poslist.
 * @param point One-based orbit root.
 * @param GS Flat array containing all generators.
 * @param poslist Zero-based indices of the generator rows to use.
 * @param poslistl Number of selected rows.
 * @param n Positive permutation degree.
 * @param orbit Output capacity n.
 * @param ol Output orbit length.
 */
void one_orbit_chain(int point, int *GS, int *poslist, int poslistl, int n, int *orbit, int *ol);

/** @ingroup permutation_groups
 * @brief Compute one orbit and its Schreier tree using selected GS rows.
 * @param point One-based orbit root.
 * @param GS Flat array containing all generators.
 * @param poslist Zero-based indices of the generator rows to use.
 * @param poslistl Number of selected rows.
 * @param n Positive permutation degree.
 * @param orbit Output capacity n, with the root first.
 * @param ol Output orbit length.
 * @param nu Output n-by-n edge-permutation rows.
 * @param w Output n-integer predecessor array.
 * @param init Nonzero clears nu and w; zero extends an existing forest.
 * @details Used by the Niehoff search to obtain slot transversals without copying
 * an entire stabilizer's generators at every level.
 */
void one_schreier_orbit_chain(int point, int *GS, int *poslist, int poslistl,
	int n, int *orbit, int *ol, int *nu, int *w, int init);

/** @ingroup permutation_groups
 * @brief Replace base points by their p-images and each generator x by p^-1 x p.
 * @details Mutates base and GS, preserving their simultaneous group description.
 */
void conjugate_chain(int *base, int bl, int *GS, int m, int n, int *p);


/** @ingroup permutation_groups
 * @brief Change an existing SGS to begin with the requested base points.
 * @details Mutates base, flattened GS, and chain together through conjugation and
 * adjacent base interchanges; additional base points can remain after the prefix.
 * The new canonicalizer uses this to enforce natural slot order independently of
 * the caller's BSGS base. This operation is not a change to the tensor symmetry.
 * @pre The input base/GS/chain are consistent and form a complete BSGS; when the
 * requested prefix is nonempty the input base is nonempty.
 */
void basechange_chain(std::vector<int>& base, std::vector<int>& GS,
                      int n, StabilizerChain& chain,
                      const int *newbase, int newbl);

/** @ingroup permutation_groups
 * @brief Append a redundant base point and an empty stabilizer level.
 * @pre The current base already determines group elements uniquely.
 */
void appendbasepoint_chain(std::vector<int>& base, StabilizerChain& chain, int newbasepoint);

/** @ingroup permutation_groups
 * @brief Interchange adjacent base positions j and j+1, updating SGS and chain.
 * @pre j is one-based and 1 <= j < base.size(); the input is a valid BSGS.
 */
void interchange_chain(std::vector<int>& base, std::vector<int>& GS, int n, StabilizerChain& chain, int j);


/** @ingroup permutation_groups
 * @brief Inherited subgroup backtracking for the selected property.
 * @details prop=1 selects a permutation centralizer and prop=4 a set stabilizer.
 * See property() for implemented tests. This utility is separate from tensor
 * double-coset canonicalization and Niehoff's propagated-symmetry search.
 */
void search(int *base, int bl, int *GS, int m, int n, int prop, int *info, int infol,
        int s, std::vector<int>& GSK, int *mK, int *num);

/* Backtrack search */
/** @ingroup permutation_groups
 * @brief Build an orbit/Schreier tree, then sort the orbit by the supplied base.
 */
void sorted_schreier_orbit(int point, int *base, int bl, int *GS, int m, int n,
        int *orbit, int *ol, int *nu, int *w, int init);

/** @ingroup permutation_groups
 * @brief Build the ordered Schreier forest used by subgroup backtracking.
 */
void sorted_schreier_vector(int point, int *base, int bl, int *GS, int m, int n, int *nu, int *w);

/** @ingroup permutation_groups
 * @brief Reconstruct a group element from successive prescribed base images.
 * @pre The image prefix is attainable in the given base/SGS chain.
 */
void from_base_image(int *images, int ll, int *base, int bl, int *GS, int m, int n, int *perm);

/** @ingroup permutation_groups
 * @brief Test the property code used by inherited subgroup backtracking.
 * @details Implemented cases are 1 (commutation with info's permutation) and
 * 4 (preservation of the set encoded by info's point-membership mask). Historical
 * comments mention normalizers/intersections, but those cases are not implemented.
 */
int property(int *g, int n, int prop, int *info, int infol);

/** @ingroup permutation_groups
 * @brief Recursive candidate extension for search(); not a canonicalizer entry point.
 */
void generate(int *base, int bl, int *GS, int m, int n, int prop, int *info, int infol,
        int s, int i, int *list, int ll, std::vector<int>& GSK, int *mK, int *mark, int *num);

void search(int *base, int bl, int *GS, int m, int n, int prop, int *info, int infol,
        int s, std::vector<int>& GSK, int *mK, int *num);
