# Papers and further reading {#references}

1. **Benjamin E. Niehoff, _Faster tensor canonicalization_.** Computer Physics
   Communications **228** (2018), 123–145.
   [Published article / DOI](https://doi.org/10.1016/j.cpc.2018.02.014),
   [author preprint, arXiv:1702.08114](https://arxiv.org/abs/1702.08114).
   The guide uses the supplied published paper's section and algorithm
   numbering: the Butler–Portugal review in section 3, Values/Label-Codes and
   propagated symmetry in section 4, Algorithms 4.1/4.2, and Appendix B.
   The preprint and publication have different pagination.

2. **L. R. U. Manssur and R. Portugal, _The Canonicalization of Tensor
   Expressions with Dummy Indices_.**
   [arXiv:math-ph/0107032](https://arxiv.org/abs/math-ph/0107032),
   [related journal DOI](https://doi.org/10.1142/S0129183102004571).
   This is background for the dummy-index double-coset construction. The
   historical source comments and @ref butler_portugal describe the particular
   xPerm implementation retained here.

3. **Gregory Butler, _Fundamental Algorithms for Permutation Groups_.**
   Lecture Notes in Computer Science **559**, Springer, 1991.
   This is the permutation-group reference cited in Niehoff's bibliography,
   covering the group-theoretic foundation underlying both searches.

4. **José M. Martín-García, _xPerm: fast index canonicalization for tensor
   computer algebra_.** Computer Physics Communications **179** (2008),
   597–603. The [xPerm project page](http://www.xact.es/xPerm/index.html)
   describes the original xAct package. Historical copyright and authorship
   notices remain in the inherited source.

This guide documents this repository's implementation. It distinguishes the
paper's mechanism from API-specific refinements, development helpers and
remaining complexity limits; it is not a claim that every step is a literal
transcription or that finite regression tests constitute a general proof.
