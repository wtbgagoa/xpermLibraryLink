(* Exercise the public canonicalization backends in a fresh, licensed kernel:
     WolframKernel -noinit -noprompt -pacletbase /tmp/xperm-methods-test \
         -script tests/backend_methods.wl STAGED_PACLET /tmp/xperm-methods-test [before|after]
     The optional argument selects whether xTensor is loaded before the paclet.
     No paclet is installed and no files are written to the Wolfram user directory.
     TensorReduce need not choose xPerm's representative; the differential oracle
     canonicalizes its result with xPerm before comparing signed permutations.
*)

scriptPosition = FirstPosition[System`$CommandLine, "-script"];

arguments =
	If[MissingQ[scriptPosition],
		Rest[System`$ScriptCommandLine]
		,
		Drop[System`$CommandLine, First[scriptPosition] + 1]
	];

If[!MemberQ[{2, 3}, Length[arguments]],
	Print["Usage: backend_methods.wl STAGED_PACLET ISOLATED_PACLET_BASE [before|after]"
		];
	Quit[2]
];

{pacletPath, expectedBase} = ExpandFileName /@ Take[arguments, 2];

loadOrder =
	If[Length[arguments] === 3,
		Last[arguments]
		,
		"before"
	];

assert[condition_, label_] :=
	If[!TrueQ[condition],
		Print["FAIL: ", label];
		Quit[1]
	];

assert[MemberQ[{"before", "after"}, loadOrder], "valid dependency load order"
	];

assert[MemberQ[System`$CommandLine, "-pacletbase"], "explicit isolated paclet repository"
	];

assert[ExpandFileName[$UserBasePacletsDirectory] === expectedBase, "kernel uses requested isolated paclet repository"
	];

assert[!StringStartsQ[expectedBase, ExpandFileName[$UserBaseDirectory
	] <> $PathnameSeparator], "test repository is outside real Wolfram user directory"
	];

Print["Kernel: ", $Version, "; xTensor load order: ", loadOrder];

Needs["xAct`xPerm`"];

If[loadOrder === "before",
	Needs["xAct`xTensor`"]
];

PacletDirectoryLoad[pacletPath];

Needs["xAct`xPermLibraryLink`"];

assert[Quiet[FindFile["xAct`xPermLibraryLink`"]] === FileNameJoin[{pacletPath,
	 "Kernel", "init.m"}], "staged test paclet selected"];

assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "Needs leaves activation explicit"
	];

(* Load xTensor before saving the baseline. In the 'after' case it is loaded
     after the paclet, which must not overwrite or bypass the eventual hooks. *)

If[loadOrder === "after",
	Needs["xAct`xTensor`"]
];

capturePublic[s_Symbol] :=
	{DownValues[s], SubValues[s], UpValues[s], Options[s], Attributes[s]
		};

originalCanonical = capturePublic[xAct`xPerm`CanonicalPerm];

originalToCanonical = capturePublic[xAct`xTensor`ToCanonical];

originalNativeFlag = OwnValues[xAct`xPerm`$xpermQ];

originalDefinitions = xAct`xPermLibraryLink`Private`CaptureDefinition /@
	 xAct`xPermLibraryLink`Private`$wrappedSymbols;

restoredQ[] :=
	originalCanonical === capturePublic[xAct`xPerm`CanonicalPerm] && originalToCanonical ===
		 capturePublic[xAct`xTensor`ToCanonical] && originalNativeFlag === OwnValues[
		xAct`xPerm`$xpermQ] && originalDefinitions === (xAct`xPermLibraryLink`Private`CaptureDefinition
		 /@ xAct`xPermLibraryLink`Private`$wrappedSymbols);

beginCounters[] :=
	(
		nativeXPerm = xAct`xPermLibraryLink`Private`$llCanonicalPerm;
		nativeNiehoff = xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm
			;
		trDefinitions = DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm
			];
		Clear[xAct`xPermLibraryLink`Private`$llCanonicalPerm, xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm
			];
		xAct`xPermLibraryLink`Private`$llCanonicalPerm[args___] :=
			(
				xPermCalls++;
				nativeXPerm[args]
			);
		xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm[args___] :=
			(
				niehoffCalls++;
				nativeNiehoff[args]
			);
		DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm]
			 =
			trDefinitions /.
				HoldPattern[RuleDelayed[left_, right_]] :>
					RuleDelayed[
						left
						,
						(
							tensorReduceCalls++;
							right
						)
					];
		resetCounters[]
	);

resetCounters[] :=
	(
		xPermCalls = 0;
		niehoffCalls = 0;
		tensorReduceCalls = 0
	);

endCounters[] :=
	(
		Clear[xAct`xPermLibraryLink`Private`$llCanonicalPerm, xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm
			];
		xAct`xPermLibraryLink`Private`$llCanonicalPerm = nativeXPerm;
		xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm = nativeNiehoff
			;
		DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm]
			 = trDefinitions
	);

canonical[method_, p_, n_, g_, f_, s_, opts___] :=
	xAct`xPerm`CanonicalPerm[p, n, g, f, s, Method -> method, opts];

probe[opts___] :=
	xAct`xPerm`CanonicalPerm[xAct`xPerm`Cycles[{1, 2}], 2, xAct`xPerm`GenSet[
		-xAct`xPerm`Cycles[{1, 2}]], {1, 2}, {}, opts];

expectedProbe = -xAct`xPerm`Images[{1, 2}];

assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Method -> "Niehoff"
	] === True, "load named Niehoff backend"];

beginCounters[];

assert[probe[Method -> "xPerm"] === expectedProbe && xPermCalls > 0 &&
	 niehoffCalls === 0 && tensorReduceCalls === 0, "explicit xPerm calls native xPerm only"
	];

resetCounters[];

assert[probe[Method -> "TensorReduce", xAct`xPerm`MathLink -> False] 
	=== expectedProbe && tensorReduceCalls > 0 && xPermCalls === 0 && niehoffCalls
	 === 0, "explicit TensorReduce overrides MathLink False and avoids native canonicalizers"
	];

systemReduceCalls = 0;

(* Force a failure at the built-in boundary to prove this route is evaluated
     and that an unexpected result is not silently sent to a native backend. *)

Internal`InheritedBlock[
	{System`TensorReduce}
	,
	Unprotect[System`TensorReduce];
	System`TensorReduce =
		Function[
			Null
			,
			(
				systemReduceCalls++;
				$Failed
			)
			,
			HoldAll
		];
	assert[Quiet[probe[Method -> "TensorReduce"]] === $Failed, "failure at System TensorReduce is reported without fallback"
		]
];

assert[systemReduceCalls > 0, "the adapter actually evaluates System TensorReduce"
	];

resetCounters[];

assert[probe[] === expectedProbe && niehoffCalls > 0 && xPermCalls ===
	 0 && tensorReduceCalls === 0, "per-call override leaves default Niehoff backend unchanged"
	];

resetCounters[];

assert[probe[Method -> Automatic, xAct`xPerm`MathLink -> False] === -
	xAct`xPerm`Cycles[] && xPermCalls === 0 && niehoffCalls === 0 && tensorReduceCalls
	 === 0, "Automatic with MathLink False retains the original Wolfram implementation"
	];

SetOptions[xAct`xPerm`CanonicalPerm, Method -> "TensorReduce"];

resetCounters[];

assert[probe[] === expectedProbe && tensorReduceCalls > 0 && xPermCalls
	 === 0 && niehoffCalls === 0, "SetOptions selects TensorReduce for CanonicalPerm"
	];

SetOptions[xAct`xPerm`CanonicalPerm, Method -> Automatic];

assert[Quiet[probe[Method -> "not-a-backend"]] === $Failed, "unknown backend rejected"
	];

assert[Quiet[xAct`xPermLibraryLink`LoadxPermLibraryLink[Method -> "not-a-backend"
	]] === $Failed && xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "invalid loader Method preserves active session"
	];

resetCounters[];

assert[probe[] === expectedProbe && niehoffCalls > 0 && tensorReduceCalls
	 === 0, "invalid loader Method preserves selected backend"];

(* Small signed permutation problems cover each independent relation imposed
     by the adapter, including disconnected index spaces and interleaved labels. *)

cases = {{2, xAct`xPerm`GenSet[], {1, 2}, {}}, {2, xAct`xPerm`GenSet[
	xAct`xPerm`Cycles[{1, 2}]], {1, 2}, {}}, {2, xAct`xPerm`GenSet[-xAct`xPerm`Cycles[
	{1, 2}]], {1, 2}, {}}, {3, xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2,
	 3}]], {1, 2, 3}, {}}, {4, xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2
	}], -xAct`xPerm`Cycles[{3, 4}], xAct`xPerm`Cycles[{1, 3}, {2, 4}]], {
	1, 2, 3, 4}, {}}, {4, xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2}, {3,
	 4}]], {1}, {xAct`xPerm`RepeatedSet[{2, 3, 4}]}}, {6, xAct`xPerm`GenSet[
	-xAct`xPerm`Cycles[{1, 2}], -xAct`xPerm`Cycles[{3, 4}], xAct`xPerm`Cycles[
	{1, 3}, {2, 4}]], {1, 4}, {xAct`xPerm`DummySet[spaceA, {{2, 5}}, 1], 
	xAct`xPerm`DummySet[spaceB, {{3, 6}}, -1]}}, {6, xAct`xPerm`GenSet[xAct`xPerm`Cycles[
	{1, 2, 3}], -xAct`xPerm`Cycles[{4, 5}]], {}, {xAct`xPerm`RepeatedSet[
	{1, 3}], xAct`xPerm`DummySet[spaceA, {{2, 6}, {4, 5}}, 0]}}};

Do[cases = Join[cases, {{2, xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2
	}]], {}, {xAct`xPerm`DummySet[spaceA, {{1, 2}}, metric]}}, {2, xAct`xPerm`GenSet[
	-xAct`xPerm`Cycles[{1, 2}]], {}, {xAct`xPerm`DummySet[spaceA, {{1, 2}
	}, metric]}}, {4, xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2}], -xAct`xPerm`Cycles[
	{3, 4}]], {}, {xAct`xPerm`DummySet[spaceA, {{1, 2}, {3, 4}}, metric]}
	}, {6, xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2, 3}]], {2, 5}, {xAct`xPerm`DummySet[
	spaceA, {{1, 4}, {3, 6}}, metric]}}}], {metric, {-1, 0, 1}}];

(* These distinguish symmetric dummy pools: merging both into uncolored
     direct contractions would introduce a forbidden exchange and false zero.
     Include nonconsecutive/reversed labels, partial free/repeated labels, and
     an oriented/antisymmetric-only problem that cannot use direct contractions. *)

cases = Join[cases, {{4, xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 3},
	 {2, 4}]], {}, {xAct`xPerm`DummySet[spaceA, {{1, 2}}, 1], xAct`xPerm`DummySet[
	spaceB, {{3, 4}}, 1]}}, {6, xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2,
	 3}]], {}, {xAct`xPerm`DummySet[spaceA, {{6, 1}, {4, 2}}, 1], xAct`xPerm`DummySet[
	spaceB, {{3, 5}}, 1]}}, {6, xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 
	3}], -xAct`xPerm`Cycles[{3, 5}], xAct`xPerm`Cycles[{2, 6}]], {}, {xAct`xPerm`DummySet[
	spaceA, {{1, 6}, {3, 2}}, -1], xAct`xPerm`RepeatedSet[{4, 5}]}}, {6, 
	xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2, 3}]], {5}, {xAct`xPerm`DummySet[
	spaceA, {{4, 1}}, 1], xAct`xPerm`RepeatedSet[{2, 3, 6}]}}}];

SeedRandom[20261018];

(* Independent finite double-coset oracle. This enumerates the small problems
     using only array composition; it calls none of the three canonicalizers.
     The two final permutation points encode the sign. A tensor is zero precisely
     when the same first n images occur with both orders of these sign points. *)

fullImages[p_, n_] :=
	xAct`xPerm`Private`toimagelist[n][p];

labelGenerators[n_, sets_] :=
	Module[{generators = {}, pairs, labels, metric},
		Do[
			Switch[Head[set],
				xAct`xPerm`DummySet,
					pairs = set[[2]];
					metric = set[[3]];
					If[metric =!= 0,
						generators = Join[generators, (metric xAct`xPerm`Cycles[#]&) /@
							 pairs]
					];
					Do[AppendTo[generators, xAct`xPerm`Cycles[{pairs[[j, 1]], pairs[[
						j + 1, 1]]}, {pairs[[j, 2]], pairs[[j + 1, 2]]}]], {j, Length[pairs] 
						- 1}]
				,
				xAct`xPerm`RepeatedSet,
					labels = First[set];
					Do[AppendTo[generators, xAct`xPerm`Cycles[{labels[[j]], labels[[
						j + 1]]}]], {j, Length[labels] - 1}]
			]
			,
			{set, sets}
		];
		fullImages[#, n]& /@ generators
	];

doubleCoset[p_, n_, group_, sets_] :=
	Module[{slotGenerators, dummyGenerators},
		slotGenerators = fullImages[#, n]& /@ (List @@ group);
		dummyGenerators = labelGenerators[n, sets];
		FixedPoint[
			Function[orbit,
				Union[orbit, Flatten[Table[v[[g]], {v, orbit}, {g, slotGenerators
					}], 1], Flatten[Table[g[[v]], {v, orbit}, {g, dummyGenerators}], 1]]
			]
			,
			{fullImages[p, n]}
		]
	];

matchesCosetQ[result_, orbit_, n_] :=
	If[Length[DeleteDuplicates[Take[#, n]& /@ orbit]] < Length[orbit],
		result === 0
		,
		result =!= 0 && MemberQ[orbit, fullImages[result, n]]
	];

differentialCases = 0;

Do[
	{n, group, frees, sets} = problem;
	permutations =
		If[n <= 3,
			Permutations[Range[n]]
			,
			DeleteDuplicates[Join[{Range[n], Reverse[Range[n]]}, Table[RandomSample[
				Range[n]], {6}]]]
		];
	Do[
		p = sign xAct`xPerm`Images[images];
		reference = canonical["xPerm", p, n, group, frees, sets];
		improved = canonical["Niehoff", p, n, group, frees, sets];
		resetCounters[];
		reduced = canonical["TensorReduce", p, n, group, frees, sets];
		label = "case " <> ToString[{differentialCases + 1, n, images, sign
			}, InputForm];
		assert[tensorReduceCalls > 0 && xPermCalls === 0 && niehoffCalls ===
			 0, label <> ": TensorReduce actually selected"];
		assert[reduced === 0 || xAct`xPerm`PermQ[reduced], label <> ": signed permutation or zero"
			];
		orbit = doubleCoset[p, n, group, sets];
		assert[matchesCosetQ[reference, orbit, n], label <> ": xPerm matches enumerated double coset"
			];
		assert[matchesCosetQ[improved, orbit, n], label <> ": Niehoff matches enumerated double coset"
			];
		assert[matchesCosetQ[reduced, orbit, n], label <> ": TensorReduce matches enumerated double coset"
			];
		assert[
			If[improved === 0,
					0
					,
					canonical["xPerm", improved, n, group, frees, sets]
				] === reference
			,
			label <> ": native representatives are equivalent"
		];
		renormalized =
			If[reduced === 0,
				0
				,
				canonical["xPerm", reduced, n, group, frees, sets]
			];
		assert[renormalized === reference, label <> ": TensorReduce preserves signed equivalence"
			];
		assert[
			If[reduced === 0,
				True
				,
				canonical["TensorReduce", reduced, n, group, frees, sets] === reduced
					
			]
			,
			label <> ": TensorReduce representative is idempotent"
		];
		differentialCases++
		,
		{images, permutations}
		,
		{sign, {1, -1}}
	]
	,
	{problem, cases}
];

Print["Differential signed-permutation cases: ", differentialCases];

assert[canonical["TensorReduce", xAct`xPerm`Cycles[], 2, xAct`xPerm`GenSet[
	-xAct`xPerm`Cycles[{1, 2}]], {}, {xAct`xPerm`RepeatedSet[{1, 2}]}] ===
	 0, "antisymmetric repeated labels vanish"];

assert[canonical["TensorReduce", xAct`xPerm`Cycles[], 3, xAct`xPerm`GenSet[
	-xAct`xPerm`Cycles[]], {1, 2, 3}, {}] === 0, "negative identity in slot group vanishes"
	];

strongGroup = xAct`xPerm`SchreierSims[{}, xAct`xPerm`GenSet[-xAct`xPerm`Cycles[
	{1, 2}]], 2, xAct`xPerm`MathLink -> False];

assert[canonical["TensorReduce", xAct`xPerm`Cycles[{1, 2}], 2, strongGroup,
	 {1, 2}, {}] === expectedProbe, "strong generating set accepted"];

assert[canonical["TensorReduce", xAct`xPerm`Cycles[{1, 2}], 2, Antisymmetric[
	{1, 2}], {1, 2}, {}] === expectedProbe, "Antisymmetric group shorthand accepted"
	];

assert[canonical["TensorReduce", -xAct`xPerm`Cycles[], 0, xAct`xPerm`GenSet[
	], {}, {}] === -xAct`xPerm`Images[{}], "rank-zero scalar retains its sign"
	];

assert[canonical["TensorReduce", xAct`xPerm`Cycles[], 1, xAct`xPerm`GenSet[
	-xAct`xPerm`Cycles[]], {1}, {}] === 0, "rank-one negative identity vanishes"
	];

omittedInput = xAct`xPerm`Images[{2, 1, 4, 3}];

omittedGroup = xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2}]];

omittedResult = canonical["TensorReduce", omittedInput, 4, omittedGroup,
	 {1}, {}];

assert[matchesCosetQ[omittedResult, doubleCoset[omittedInput, 4, omittedGroup,
	 {}], 4], "labels omitted from frees and sets remain fixed under label permutations"
	];

resetCounters[];

Do[assert[Quiet[canonical["TensorReduce", xAct`xPerm`Cycles[], 4, xAct`xPerm`GenSet[
	], {}, invalidSets]] === $Failed, "invalid dummy metric, range or overlap is rejected"
	], {invalidSets, {{xAct`xPerm`DummySet[spaceA, {{1, 2}, {3, 4}}, 2]},
	 {xAct`xPerm`DummySet[spaceA, {{1, 2}, {3, 5}}, 1]}, {xAct`xPerm`DummySet[
	spaceA, {{1, 2}, {2, 3}}, 1]}}}];

assert[xPermCalls === 0 && niehoffCalls === 0, "invalid TensorReduce input never falls back"
	];

assert[
	Block[{$Assumptions = False},
			probe[Method -> "TensorReduce"]
		] === expectedProbe
	,
	"ambient assumptions do not change formal tensor canonicalization"
];

(* Exercise the xTensor facade, including its existing derivative Method. *)

$DefInfoQ = False;

DefManifold[backendTestM, 4, {ma, mb, mc, md, me, mf, mg, mh, mi, mj,
	 mk, ml}];

DefMetric[1, backendMetric[-ma, -mb], backendCD];

DefTensor[backendF[-ma, -mb], backendTestM, Antisymmetric[{-ma, -mb}]
	];

DefTensor[backendV[-ma], backendTestM];

FProduct[n_Integer] :=
	Module[{indices, counter},
		indices = GetIndicesOfVBundle[TangentbackendTestM, n];
		counter = Flatten[Transpose[{indices, -indices}]];
		Times @@ Apply[backendF, Partition[RotateLeft[counter], 2], {1}]
	];

oddRing = FProduct[3]; evenRing = FProduct[4];

resetCounters[];

assert[ToCanonical[oddRing, Method -> "TensorReduce"] === 0 && tensorReduceCalls
	 > 0 && xPermCalls === 0 && niehoffCalls === 0, "xTensor odd ring uses TensorReduce and vanishes"
	];

reducedEven = ToCanonical[evenRing, Method -> "TensorReduce"];

assert[reducedEven =!= 0 && ToCanonical[reducedEven - evenRing, Method
	 -> "xPerm"] === 0, "xTensor even ring is nonzero and equivalent"];

resetCounters[];

assert[ToCanonical[{oddRing, 2 oddRing}, Method -> "TensorReduce"] ===
	 {0, 0} && tensorReduceCalls > 0 && niehoffCalls === 0, "list threading preserves explicit backend"
	];

SetOptions[ToCanonical, Method -> "TensorReduce"];

resetCounters[];

assert[ToCanonical[oddRing] === 0 && tensorReduceCalls > 0 && niehoffCalls
	 === 0, "SetOptions selects TensorReduce for ToCanonical"];

SetOptions[ToCanonical, Method -> {ChangeCovD, "ExpandChristoffel" ->
	 False}];

resetCounters[];

assert[ToCanonical[oddRing, Method -> Implode] === 0 && niehoffCalls 
	> 0 && tensorReduceCalls === 0, "legacy derivative Method preserves the selected default backend"
	];

assert[ToCanonical[oddRing, Method -> {"TensorReduce", "DerivativeMethod"
	 -> Implode}] === 0, "backend and derivative Method can be specified together"
	];

(* This expression actually enters xTensor's metric-incompatible derivative
     branch. Testing Method on an algebraic expression alone cannot catch a
     backend string accidentally being passed to the derivative dispatcher. *)

derivativeExpression = backendV[-ma] PD[mb][backendV[ma]];

derivativeDefinitions = DownValues[xAct`xTensor`Private`ToCanonicalDersImplode
	];

DownValues[xAct`xTensor`Private`ToCanonicalDersImplode] =
	derivativeDefinitions /.
		HoldPattern[RuleDelayed[left_, right_]] :>
			RuleDelayed[
				left
				,
				(
					derivativeCalls++;
					right
				)
			];

derivativeCalls = 0;

resetCounters[];

legacyDerivative = Quiet[ToCanonical[derivativeExpression, Method -> 
	Implode], ToCanonical::cmods];

assert[derivativeCalls > 0 && niehoffCalls > 0 && tensorReduceCalls ===
	 0, "legacy Implode actually handles a metric-incompatible derivative"
	];

derivativeCalls = 0;

resetCounters[];

reducedDerivative = Quiet[ToCanonical[derivativeExpression, Method ->
	 {"TensorReduce", "DerivativeMethod" -> Implode}], ToCanonical::cmods
	];

assert[derivativeCalls > 0 && tensorReduceCalls > 0 && xPermCalls ===
	 0 && niehoffCalls === 0, "explicit TensorReduce is preserved through derivative canonicalization"
	];

(* The expressions are already expanded. Keep metric positions fixed here so
     the comparison itself does not expand derivatives again and create fresh
     dummy names after xTensor's initial SameDummies pass. *)

assert[Quiet[ToCanonical[reducedDerivative - legacyDerivative, Method
	 -> "xPerm", UseMetricOnVBundle -> None], ToCanonical::cmods] === 0, 
	"both backends agree on the derivative expression"];

DownValues[xAct`xTensor`Private`ToCanonicalDersImplode] = derivativeDefinitions;

(* Failed or aborted TensorReduce calls must not be reconstructed as tensors,
     nor leave a dynamic backend selection installed for the following call. *)

savedReduceDefinitions = DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm
	];

DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm] = {
	HoldPattern[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm[___
	]] :> $Failed};

assert[ToCanonical[evenRing, Method -> "TensorReduce"] === $Failed, "TensorReduce failure propagates cleanly through ToCanonical"
	];

DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm] = {
	HoldPattern[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm[___
	]] :> Abort[]};

assert[CheckAbort[ToCanonical[evenRing, Method -> "TensorReduce"], "caught-abort"
	] === "caught-abort", "TensorReduce abort is not swallowed"];

DownValues[xAct`xPermLibraryLink`Private`TensorReduceCanonicalPerm] = savedReduceDefinitions;

resetCounters[];

assert[probe[] === expectedProbe && niehoffCalls > 0 && tensorReduceCalls
	 === 0, "failed and aborted calls restore the default backend"];

(* These are correctness and resource regressions, not optional benchmarks.
     0.3.0 exhausted 512 MiB on nine factors. A timeout or memory-limit result
     must now fail, rather than being accepted as successful validation. The
     memory budget is additional allocation allowed by MemoryConstrained. *)

Do[
	largeRing = FProduct[factorCount];
	resetCounters[];
	largeTiming = AbsoluteTiming[MemoryConstrained[TimeConstrained[ToCanonical[
		largeRing, Method -> "TensorReduce"], 30, "time-limit"], 512 * 1024^2,
		 "memory-limit"]];
	assert[xPermCalls === 0 && niehoffCalls === 0 && tensorReduceCalls >
		 0, "bounded ring uses TensorReduce without native canonicalization"]
		;
	assert[Last[largeTiming] === 0, ToString[factorCount] <> "-factor ring is zero within 30 seconds and 512 MiB"
		];
	Print[<|"Factors" -> factorCount, "Method" -> "TensorReduce", "Seconds"
		 -> First[largeTiming], "Result" -> Last[largeTiming], "AdditionalMemoryBudgetBytes"
		 -> 512 * 1024^2, "TensorReduceCalls" -> tensorReduceCalls|>]
	,
	{factorCount, {9, 125}}
];

(* Nonzero and disconnected contractions exercise the decoder, preventing a
     zero-only shortcut from masquerading as a general representation fix. *)

Do[
	reducedRing = MemoryConstrained[TimeConstrained[ToCanonical[ring, Method
		 -> "TensorReduce"], 30, $Aborted], 512 * 1024^2, $Aborted];
	assert[reducedRing =!= 0 && reducedRing =!= $Aborted && reducedRing 
		=!= $Failed, "even/disconnected ring remains nonzero within resource limits"
		];
	assert[ToCanonical[reducedRing - ring, Method -> "Niehoff"] === 0, "even/disconnected ring preserves tensor equivalence"
		]
	,
	{ring, {FProduct[10], FProduct[4] * ReplaceDummies[FProduct[6]]}}
];

endCounters[];

(* Repeated activation must neither retain old dispatch nor lose definitions. *)

assert[xAct`xPermLibraryLink`UnloadxPermLibraryLink[] === True && restoredQ[
	], "unload restores original definitions, options, attributes and dispatch flag"
	];

Do[
	assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Method -> method] 
		=== True, "load named backend " <> method];
	beginCounters[];
	assert[probe[] === expectedProbe, "default representative after loading "
		 <> method];
	assert[
		Switch[method,
			"xPerm",
				xPermCalls > 0 && niehoffCalls === 0 && tensorReduceCalls === 0
			,
			"Niehoff",
				niehoffCalls > 0 && xPermCalls === 0 && tensorReduceCalls === 0
			,
			"TensorReduce",
				tensorReduceCalls > 0 && xPermCalls === 0 && niehoffCalls === 0
		]
		,
		"default dispatch after loading " <> method
	];
	endCounters[]
	,
	{method, {"TensorReduce", "xPerm", "Niehoff"}}
];

Do[
	assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, legacy]
		 === True, "legacy boolean loader accepted"];
	beginCounters[];
	assert[
		probe[] === expectedProbe &&
			If[legacy,
				niehoffCalls > 0 && xPermCalls === 0
				,
				xPermCalls > 0 && niehoffCalls === 0
			]
		,
		"legacy boolean selects its original algorithm"
	];
	endCounters[]
	,
	{legacy, {True, False}}
];

Get[FileNameJoin[{pacletPath, "Kernel", "init.m"}]];

assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[] && restoredQ[
	], "source reload restores original definitions and leaves library unloaded"
	];

(* Interrupt after CanonicalPerm has been replaced, before ToCanonical is
     captured. Activation must roll back the partial installation as well. *)

snapshotDefinitions = DownValues[xAct`xPermLibraryLink`Private`SnapshotBackendSymbol
	];

xAct`xPermLibraryLink`Private`SnapshotBackendSymbol[HoldComplete[xAct`xTensor`ToCanonical
	]] :=
	Abort[];

activationResult = CheckAbort[xAct`xPermLibraryLink`LoadxPermLibraryLink[
	Method -> "TensorReduce"], $Aborted];

DownValues[xAct`xPermLibraryLink`Private`SnapshotBackendSymbol] = snapshotDefinitions;

assert[activationResult === $Aborted && !xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[
	] && restoredQ[], "aborted activation restores partial public hooks and unloads native functions"
	];

assert[xAct`xPermLibraryLink`UnloadxPermLibraryLink[] === True && restoredQ[
	], "repeated unload is harmless"];

Print["Backend Method regressions passed (", loadOrder, ")"];

Quit[0];
