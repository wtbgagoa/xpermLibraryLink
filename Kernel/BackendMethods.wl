(* Method selection for the public xPerm/xTensor entry points.

   xTensor already uses Method to choose how incompatible derivatives are
   expanded. Backend strings extend that interface; legacy Method values keep
   their original meaning. A dynamic backend scope carries the selection into
   recursive ToCanonical calls without changing the session default.

   All changes to the external packages are captured before activation and
   restored on unload. The aliases change only each definition's left-hand
   side, so recursive calls still pass through the public adapter. *)

LoadxPermLibraryLink::method = "Unknown canonicalization method `1`. Use Automatic, \"xPerm\", \"Niehoff\", or \"TensorReduce\".";

LoadxPermLibraryLink::dermethod = "Invalid combined Method specification `1`. Use {backend, \"DerivativeMethod\" -> method}.";

$backendMethodsInstalled = False;

$backendSnapshots = {};

$defaultCanonicalizationMethod = "xPerm";

$scopedCanonicalizationMethod = Automatic;

$forceCanonicalizationMethod = False;

$insideToCanonical = False;

$legacyDerivativeMethod = {xAct`xTensor`ChangeCovD, "ExpandChristoffel"
	 -> False};

BackendMethodQ[method_] :=
	MemberQ[{"xPerm", "Niehoff", "TensorReduce"}, method];

(* Keep Options and Attributes as well as values: ToCanonical is Protected and
   CanonicalPerm gains an option only for the duration of the native session. *)

SnapshotBackendSymbol[held : HoldComplete[symbol_]] :=
	{held, CaptureDefinition[held], Options[symbol], Attributes[symbol]};

RestoreBackendSymbol[{held : HoldComplete[symbol_], definitions_, options_,
	 attributes_}] :=
	(
		Unprotect[symbol];
		RestoreDefinition[held, definitions];
		Options[symbol] = options;
		Attributes[symbol] = attributes;
	);

InstallBackendMethods[method_] :=
	Module[{toCanonicalOptions},
		$defaultCanonicalizationMethod = method;
		$backendSnapshots = {SnapshotBackendSymbol[HoldComplete[xAct`xPerm`CanonicalPerm
			]]};
		(* Make partial installation restorable if loading xTensor is interrupted. 
			*)
		$backendMethodsInstalled = True;
		Options[OriginalCanonicalPerm] = Options[xAct`xPerm`CanonicalPerm];
			
		(* Rename only RuleDelayed's first part. HoldPattern is transparent during
			
   pattern matching, so an unrestricted rule could also rewrite recursive
	
   calls on the right-hand side and leave them unevaluated. *)
		DownValues[OriginalCanonicalPerm] =
			Map[
				Function[rule,
					MapAt[
						Function[lhs,
							lhs /. HoldPattern[xAct`xPerm`CanonicalPerm] -> OriginalCanonicalPerm
								
						]
						,
						rule
						,
						1
					]
				]
				,
				DownValues[xAct`xPerm`CanonicalPerm]
			];
		Unprotect[xAct`xPerm`CanonicalPerm];
		Options[xAct`xPerm`CanonicalPerm] = Append[Options[xAct`xPerm`CanonicalPerm
			], Method -> Automatic];
		(* Keep xPerm's arity diagnostics; replace its one OptionsPattern rule. 
			*)
		DownValues[xAct`xPerm`CanonicalPerm] = Select[DownValues[xAct`xPerm`CanonicalPerm
			], FreeQ[#, _OptionsPattern]&];
		xAct`xPerm`CanonicalPerm[perm_?xAct`xPerm`PermQ, degree_Integer, group_,
			 frees_List, sets : {(_xAct`xPerm`DummySet | _xAct`xPerm`RepeatedSet)
			...}, options : OptionsPattern[]] := DispatchCanonicalPerm[perm, degree,
			 group, frees, sets, {options}];
		Attributes[xAct`xPerm`CanonicalPerm] = $backendSnapshots[[1, 4]];
		(* Load xTensor, when installed, before capturing its definitions. This also
			
   makes loading the paclet before xTensor deterministic. xPerm-only use
	
   remains supported on installations where xTensor is unavailable. *)
	
		If[!MemberQ[$Packages, "xAct`xTensor`"] && StringQ[Quiet[FindFile["xAct`xTensor`"
			]]],
			Quiet[Needs["xAct`xTensor`"]]
		];
		If[MemberQ[$Packages, "xAct`xTensor`"],
			AppendTo[$backendSnapshots, SnapshotBackendSymbol[HoldComplete[xAct`xTensor`ToCanonical
				]]];
			toCanonicalOptions = Options[xAct`xTensor`ToCanonical];
			$legacyDerivativeMethod = Method /. toCanonicalOptions;
			Options[OriginalToCanonical] = toCanonicalOptions;
			DownValues[OriginalToCanonical] =
				Map[
					Function[rule,
						MapAt[
							Function[lhs,
								lhs /. HoldPattern[xAct`xTensor`ToCanonical] -> OriginalToCanonical
									
							]
							,
							rule
							,
							1
						]
					]
					,
					DownValues[xAct`xTensor`ToCanonical]
				];
			Unprotect[xAct`xTensor`ToCanonical];
			DownValues[xAct`xTensor`ToCanonical] = {};
			xAct`xTensor`ToCanonical[] := OriginalToCanonical[];
			xAct`xTensor`ToCanonical[expression_, options___] := DispatchToCanonical[
				expression, {options}];
			Attributes[xAct`xTensor`ToCanonical] = $backendSnapshots[[2, 4]];
		];
		$backendMethodsInstalled = True;
	];

RestoreBackendMethods[] :=
	(
		If[TrueQ[$backendMethodsInstalled],
			Scan[RestoreBackendSymbol, Reverse[$backendSnapshots]]
		];
		$backendSnapshots = {};
		$backendMethodsInstalled = False;
		$scopedCanonicalizationMethod = Automatic;
		$forceCanonicalizationMethod = False;
		$insideToCanonical = False;
		Clear[OriginalCanonicalPerm, OriginalToCanonical];
	);

(* Explicit backend selections override MathLink. Automatic retains the old
   transport choice: MathLink -> False still calls xPerm's WL implementation.
   Backend Method is never passed to SchreierSims, where Method has a different
   meaning (for example "Butler3"). *)

DispatchCanonicalPerm[perm_, degree_, group_, frees_, sets_, rawOptions_
	] :=
	Module[{options = Flatten[rawOptions], requested, method, force, clean,
		 native, result},
		requested = OptionValue[xAct`xPerm`CanonicalPerm, options, Method];
			
		If[$insideToCanonical && requested =!= Automatic && !BackendMethodQ[
			requested],
			requested = Automatic
		];
		If[requested =!= Automatic && !BackendMethodQ[requested],
			Message[LoadxPermLibraryLink::method, requested];
			Return[$Failed]
		];
		force = requested =!= Automatic || TrueQ[$forceCanonicalizationMethod
			];
		method =
			Which[
				requested =!= Automatic,
					requested
				,
				$scopedCanonicalizationMethod =!= Automatic,
					$scopedCanonicalizationMethod
				,
				True,
					$defaultCanonicalizationMethod
			];
		native = TrueQ[OptionValue[xAct`xPerm`CanonicalPerm, options, xAct`xPerm`MathLink
			]];
		clean = DeleteCases[options, HoldPattern[(Method -> _) | (Method :>
			 _)]];
		If[!force && !native,
			Return[OriginalCanonicalPerm[perm, degree, group, frees, sets, Sequence
				 @@ clean]]
		];
		If[method === "TensorReduce",
			result = TensorReduceCanonicalPerm[perm, degree, group, frees, sets
				];
			If[result === $Failed && TrueQ[$insideToCanonical],
				Throw[$Failed, BackendFailure]
			];
			Return[result]
		];
		clean = DeleteCases[clean, HoldPattern[(xAct`xPerm`MathLink -> _) |
			 (xAct`xPerm`MathLink :> _)]];
		Internal`InheritedBlock[
			{xAct`xPerm`Private`MLCanonicalPerm}
			,
			Clear[xAct`xPerm`Private`MLCanonicalPerm];
			If[method === "Niehoff",
				xAct`xPerm`Private`MLCanonicalPerm[p_List, n_Integer, rest__] := 
					xAct`xPerm`Private`ToSign[xAct`xPerm`Images[$llNiehoffCanonicalPerm[p,
					 n, rest]], n - 2]
				,
				xAct`xPerm`Private`MLCanonicalPerm[p_List, n_Integer, rest__] := 
					xAct`xPerm`Private`ToSign[xAct`xPerm`Images[$llCanonicalPerm[p, n, rest
					]], n - 2]
			];
			OriginalCanonicalPerm[perm, degree, group, frees, sets, xAct`xPerm`MathLink
				 -> True, Sequence @@ clean]
		]
	];

(* Resolve the two meanings of ToCanonical's Method. A backend alone uses the
   derivative method captured at activation; the explicit pair form permits
   both to be chosen in one call. Legacy derivative forms remain unchanged. *)

ToCanonicalMethods[requested_] :=
	Which[
		requested === Automatic,
			{Automatic, $legacyDerivativeMethod}
		,
		BackendMethodQ[requested],
			{requested, $legacyDerivativeMethod}
		,
		ListQ[requested] && Length[requested] >= 1 && BackendMethodQ[First[
			requested]],
			If[Length[requested] === 2 && MatchQ[Last[requested], HoldPattern[
				("DerivativeMethod" -> _) | ("DerivativeMethod" :> _)]],
				{First[requested], "DerivativeMethod" /. Rest[requested]}
				,
				Message[LoadxPermLibraryLink::dermethod, requested];
				$Failed
			]
		,
		StringQ[requested],
			Message[LoadxPermLibraryLink::method, requested];
			$Failed
		,
		True,
			{Automatic, requested}
	];

DispatchToCanonical[expression_, rawOptions_] :=
	If[TrueQ[$insideToCanonical],
		DispatchToCanonicalScoped[expression, rawOptions]
		,
		Catch[DispatchToCanonicalScoped[expression, rawOptions], BackendFailure
			]
	];

DispatchToCanonicalScoped[expression_, rawOptions_] :=
	Module[{options = Flatten[rawOptions], resolved, method, derivativeMethod,
		 force, clean},
		resolved = ToCanonicalMethods[OptionValue[xAct`xTensor`ToCanonical,
			 options, Method]];
		If[resolved === $Failed,
			Return[$Failed]
		];
		{method, derivativeMethod} = resolved;
		force = method =!= Automatic || TrueQ[$forceCanonicalizationMethod]
			;
		If[method === Automatic,
			method =
				If[$scopedCanonicalizationMethod === Automatic,
					$defaultCanonicalizationMethod
					,
					$scopedCanonicalizationMethod
				]
		];
		clean = DeleteCases[options, HoldPattern[(Method -> _) | (Method :>
			 _)]];
		Block[
			{$scopedCanonicalizationMethod = method, $forceCanonicalizationMethod
				 = force, $insideToCanonical = True}
			,
			(* xTensor's derivative expansion reads the default Method directly
				
   as well as through OptionValue. Scope both reads consistently.
   InheritedBlock restores options/definitions even after Abort. *)
			Internal`InheritedBlock[
				{xAct`xTensor`ToCanonical}
				,
				Unprotect[xAct`xTensor`ToCanonical];
				SetOptions[xAct`xTensor`ToCanonical, Method -> derivativeMethod];
					
				Attributes[xAct`xTensor`ToCanonical] = $backendSnapshots[[2, 4]];
					
				OriginalToCanonical[expression, Method -> derivativeMethod, Sequence
					 @@ clean]
			]
		]
	];
