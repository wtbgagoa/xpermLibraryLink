(* ::Package:: *)

(* TensorReduce backend for xPerm's signed-permutation canonicalization API.
   This file is loaded in xPermLibraryLink's Private context. It never calls
   either xPerm canonicalizer.

   One formal array carries the exact supplied slot group S. A symmetric
   dummy pool is represented by ordinary TensorContract pairs on that array:
   pair orientation and pair ordering already have exactly that pool's D
   symmetries. Only one pool is contracted this way, so different pools cannot
   mix. Remaining pools use distinct symbolic factors: identical rank-two
   arrays for each DummySet, identical vectors for each RepeatedSet, and
   distinct vectors for free/unlisted labels.

   Version 0.3.0 represented all dummy symmetries in another rank-n array.
   TensorReduce's search on that redundant two-array network exhausts memory
   even for a small antisymmetric ring. Direct dummy contractions remove that
   representation defect without recognizing a particular tensor expression
   or using another canonicalizer to find zero.

   Symbolic dimensions introduce no finite-dimensional identities. The signed
   scalar contraction represents exactly the original double coset; its edges
   decode a representative chosen by TensorReduce itself.
*)

TensorReduceCanonicalPerm::input =
    "Invalid signed permutation, symmetry group, or disjoint free/dummy/repeated labels for TensorReduce canonicalization (degree `1`).";
TensorReduceCanonicalPerm::reduce =
    "TensorReduce did not return a signed factor contraction with a valid permutation of the `1` labels. No other canonicalization backend was used.";

(* Convert valid xPerm notation without constructing a strong generating set.
   PermQ alone also accepts general numeric multiples and sums; the exact
   signed Images match below intentionally limits the domain to phase +/-1. *)
TensorReduceSignedImages[permutation_, degree_Integer] :=
    Module[{converted, result},
        If[!TrueQ[xAct`xPerm`PermQ[permutation]] ||
            !TrueQ[xAct`xPerm`PermDeg[permutation] <= degree], Return[$Failed]];
        converted = Quiet[Check[
            xAct`xPerm`TranslatePerm[permutation, {xAct`xPerm`Images, degree}],
            $Failed]];
        result = Replace[converted, {
            xAct`xPerm`Images[images_List] :> {images, 1},
            Times[-1, xAct`xPerm`Images[images_List]] :> {images, -1},
            _ :> $Failed
        }];
        If[result === $Failed || Sort[result[[1]]] =!= Range[degree],
            $Failed, result]
    ];

(* Adjacent transpositions generate each whole symmetric group. For dummy
   pairs the analogous adjacent pair exchange preserves endpoint orientation.
   These sparse descriptions avoid an unnecessary Schreier-Sims computation. *)
TensorReduceSwapGenerator[degree_Integer, pairs_List, sign_Integer] :=
    {ReplacePart[Range[degree], Flatten[
        ({#[[1]] -> #[[2]], #[[2]] -> #[[1]]} &) /@ pairs]], sign};

TensorReduceSlotGenerators[group_, degree_Integer] :=
    Module[{generators, slots, sign},
        Which[
            MatchQ[group, xAct`xPerm`StrongGenSet[_List, _xAct`xPerm`GenSet]],
                If[!VectorQ[group[[1]], IntegerQ] ||
                    Length[DeleteDuplicates[group[[1]]]] =!= Length[group[[1]]] ||
                    !SubsetQ[Range[degree + 2], group[[1]]], Return[$Failed]];
                generators = List @@ group[[2]],
            Head[group] === xAct`xPerm`GenSet,
                generators = List @@ group,
            MatchQ[group, (System`Symmetric | System`Antisymmetric)[_List]],
                slots = First[group];
                If[!VectorQ[slots, IntegerQ] ||
                    Length[DeleteDuplicates[slots]] =!= Length[slots] ||
                    !SubsetQ[Range[degree], slots], Return[$Failed]];
                sign = If[Head[group] === System`Antisymmetric, -1, 1];
                Return[TensorReduceSwapGenerator[degree, {#}, sign] & /@
                    Partition[slots, 2, 1]],
            True, Return[$Failed]
        ];
        generators = TensorReduceSignedImages[#, degree] & /@ generators;
        If[MemberQ[generators, $Failed], $Failed, generators]
    ];

(* Each label factor has {original labels, factor class, internal symmetry}.
   Distinct classes never exchange. The order of a metric-free pair matters;
   antisymmetric metrics carry a negative endpoint flip. Product exchange of
   identical factors supplies whole-pair permutations without enumerating D. *)
TensorReduceLabelFactors[sets_List, degree_Integer, directPairs_List] :=
    Module[{records = {}, class = 0, set, symmetry, used},
        Do[
            class++;
            If[Head[set] === xAct`xPerm`DummySet,
                symmetry = Switch[set[[3]], 1, Symmetric[{1, 2}],
                    -1, Antisymmetric[{1, 2}], 0, {}];
                records = Join[records, {#, class, symmetry} & /@ set[[2]]],
                records = Join[records, {{#}, class, {}} & /@ First[set]]
            ], {set, sets}];
        used = Join[Flatten[directPairs], Flatten[If[records === {}, {}, records[[All, 1]]]]];
        Do[class++; AppendTo[records, {{label}, class, {}}],
            {label, Complement[Range[degree], used]}];
        records
    ];

(* A nonzero result contains the slot array exactly once. Attach each
   occurrence of an identical label factor to the next original pair/label:
   this choice differs only by an allowed D permutation. Direct self-edges
   similarly receive the original symmetric dummy pairs in sorted slot order.
   Validate all endpoints and factors before reconstructing a permutation. *)
TensorReduceDecode[reduced_, records_List, degree_Integer, directPairs_List] :=
    Module[{term = reduced, phase = 1, remaining = Range[Length[records]], result,
        factors, contractions, endpoints = {}, position, record, edge, factor,
        directEdges = {}, valid = True},
        If[MatchQ[term, Times[-1, _TensorContract]], phase = -1; term = -term];
        If[!MatchQ[term, TensorContract[_, _List]], Return[$Failed]];
        factors = If[Head[First[term]] === TensorProduct, List @@ First[term], {First[term]}];
        contractions = Last[term];
        Do[
            position = FirstPosition[records[[remaining, 1]], factor];
            If[MissingQ[position], valid = False; Break[]];
            record = records[[remaining[[First[position]]]]];
            remaining = Delete[remaining, position];
            endpoints = Join[endpoints, {record[[4]], #} & /@ record[[2]]],
            {factor, factors}];
        If[!valid || remaining =!= {} ||
            !MatchQ[contractions, {{_Integer, _Integer} ...}] ||
            Sort[Flatten[contractions]] =!= Range[Length[endpoints]], Return[$Failed]];
        result = ConstantArray[0, degree];
        Do[
            edge = Sort[endpoints[[pair]]];
            Switch[edge[[All, 1]],
                {1, 1}, AppendTo[directEdges, edge[[All, 2]]],
                {1, 2}, result[[edge[[1, 2]]]] = edge[[2, 2]],
                _, valid = False; Break[]],
            {pair, contractions}];
        If[!valid || Length[directEdges] =!= Length[directPairs], Return[$Failed]];
        MapThread[(result[[#1]] = #2) &, {Sort[directEdges], directPairs}];
        If[Sort[result] =!= Range[degree], $Failed, phase xAct`xPerm`Images[result]]
    ];

TensorReduceCanonicalPerm[permutation_, degree_Integer, group_, frees_List, sets_List] :=
    Block[{$Assumptions = True, tensorReduceFactor, tensorReduceDimension},
        Module[{signedPermutation, slotGenerators, labels, labelFactors, records,
            assumptions, expression, reduced, result, directCandidates, directIndex,
            directPairs = {}, remainingSets = sets, inverse, factorLabels, contractions},
            If[degree < 0,
                Message[TensorReduceCanonicalPerm::input, degree]; Return[$Failed]];
            signedPermutation = TensorReduceSignedImages[permutation, degree];
            slotGenerators = TensorReduceSlotGenerators[group, degree];
            If[signedPermutation === $Failed || slotGenerators === $Failed,
                Message[TensorReduceCanonicalPerm::input, degree]; Return[$Failed]];

            (* Validate every label class before building any tensor expression.
               Unlisted labels are allowed;
               like explicit free labels they have no D generator and stay fixed. *)
            If[!AllTrue[sets, MatchQ[#,
                    xAct`xPerm`DummySet[_, {{_Integer, _Integer} ...}, -1 | 0 | 1] |
                    xAct`xPerm`RepeatedSet[{___Integer}]] &],
                Message[TensorReduceCanonicalPerm::input, degree]; Return[$Failed]];
            labels = Join[frees, Flatten[
                (If[Head[#] === xAct`xPerm`DummySet, #[[2]], First[#]] &) /@ sets]];
            If[!VectorQ[labels, IntegerQ] ||
                Length[DeleteDuplicates[labels]] =!= Length[labels] ||
                !SubsetQ[Range[degree], labels],
                Message[TensorReduceCanonicalPerm::input, degree]; Return[$Failed]];

            (* A supplied negative identity forces zero at every rank. Handle
               that relation directly: TensorReduce in Wolfram 15.1 otherwise
               misses it for a rank-one symbolic array. This is not a call to
               another canonicalizer. Rank-zero inputs then need no contraction. *)
            If[MemberQ[slotGenerators, {Range[degree], -1}], Return[0]];
            If[degree === 0,
                Return[signedPermutation[[2]] xAct`xPerm`Images[{}]]];

            (* TensorContract already identifies the two endpoints and the
               ordering of its contraction pairs. Inline the largest symmetric
               dummy pool; retain explicit colored factors for all other pools.
               Inlining two pools would incorrectly allow exchanges between
               them, even when they describe different vector bundles. *)
            directCandidates = Select[Range[Length[sets]],
                Head[sets[[#]]] === xAct`xPerm`DummySet && sets[[#, 3]] === 1 &];
            If[directCandidates =!= {},
                directIndex = First[MaximalBy[directCandidates, Length[sets[[#, 2]]] &]];
                directPairs = sets[[directIndex, 2]];
                remainingSets = Delete[sets, directIndex]];
            labelFactors = TensorReduceLabelFactors[remainingSets, degree, directPairs];
            (* Store {symbol, original slots/labels, internal symmetry, side}.
               Indexed private symbols keep stable ordering without creating
               new symbols or persistent assumptions on every call. *)
            records = Join[
                {{tensorReduceFactor[1, 1], Range[degree], slotGenerators, 1}},
                {tensorReduceFactor[2, #[[2]]], #[[1]], #[[3]], 2} & /@ labelFactors];
            assumptions = Quiet[DeleteDuplicates[
                Element[#[[1]], System`Arrays[
                    ConstantArray[tensorReduceDimension, Length[#[[2]]]], Reals, #[[3]]]] & /@
                    records], Arrays::symm0];
            inverse = Ordering[signedPermutation[[1]]];
            factorLabels = Flatten[If[labelFactors === {}, {}, labelFactors[[All, 1]]]];
            contractions = Join[
                (inverse[[#]] &) /@ directPairs,
                MapIndexed[{inverse[[#1]], degree + First[#2]} &, factorLabels]];
            expression = System`TensorContract[
                System`TensorProduct @@ records[[All, 1]], contractions];
            reduced = Quiet[Check[
                System`TensorReduce[expression, Assumptions -> assumptions], $Failed]];
            If[reduced === 0, Return[0]];
            result = TensorReduceDecode[reduced, records, degree, directPairs];
            If[result === $Failed,
                Message[TensorReduceCanonicalPerm::reduce, degree]; Return[$Failed]];
            signedPermutation[[2]] result
        ]
    ];

TensorReduceCanonicalPerm[___] :=
    (Message[TensorReduceCanonicalPerm::input, "unknown"]; $Failed);
