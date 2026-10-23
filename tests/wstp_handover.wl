(* Exercise replacement of a REAL, connected xPerm WSTP executable.
   Run in a fresh licensed kernel, with local WSTP IPC permitted:
   WolframKernel -noinit -noprompt -pacletbase /tmp/xperm-handover-test \
     -script tests/wstp_handover.wl /absolute/path/staged-paclet /tmp/xperm-handover-test
   The staged directory must contain Kernel, PacletInfo.wl, and the compiled
   LibraryResources/$SystemID library. No paclet is installed by this test.
   This test deliberately fails if xPerm's executable cannot connect; testing
   only absent/sentinel definitions misses WSTP's more-specific dispatch rules.
*)

scriptPosition = FirstPosition[System`$CommandLine, "-script"];

arguments =
	If[MissingQ[scriptPosition],
		Rest[System`$ScriptCommandLine]
		,
		Drop[System`$CommandLine, First[scriptPosition] + 1]
	];

If[Length[arguments] =!= 2,
	Print["Usage: wstp_handover.wl STAGED_PACLET ISOLATED_PACLET_BASE"];
		
	Quit[2]
];

{pacletPath, expectedBase} = ExpandFileName /@ arguments;

assert[condition_, label_] :=
	If[!TrueQ[condition],
		Print["FAIL: ", label];
		Quit[1]
	];

assert[MemberQ[System`$CommandLine, "-pacletbase"], "explicit isolated paclet repository"
	];

assert[ExpandFileName[$UserBasePacletsDirectory] === expectedBase, "kernel uses requested isolated paclet repository"
	];

assert[!StringStartsQ[expectedBase, ExpandFileName[$UserBaseDirectory
	] <> $PathnameSeparator], "test repository is outside real Wolfram user directory"
	];

assert[!MemberQ[$Packages, "xAct`xPermLibraryLink`"], "fresh paclet context"
	];

Print["Kernel: ", $Version];

Needs["xAct`xTensor`"];

assert[TrueQ[xAct`xPerm`$xpermQ], "real xPerm WSTP executable connected"
	];

originalLink = xAct`xPerm`$xpermLink;

assert[Head[originalLink] === LinkObject && MemberQ[Links[], originalLink
	], "original xPerm WSTP link is live"];

assert[!FreeQ[DownValues[xAct`xPerm`Private`MLCanonicalPerm], _ExternalCall
	], "canonicalizer has real installed WSTP definitions"];

PacletDirectoryLoad[pacletPath];

assert[Quiet[FindFile["xAct`xPermLibraryLink`"]] === FileNameJoin[{pacletPath,
	 "Kernel", "init.m"}], "staged test paclet selected"];

Needs["xAct`xPermLibraryLink`"];

assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "native loading is explicit"
	];

(* Count actual ExternalCall rule execution without mocking its result or link.
   The saved definitions include the counters, so restoration is also tested. *)

oldNativeCalls = 0;

countWSTP[HoldComplete[symbol_]] :=
	(
		DownValues[symbol] =
			DownValues[symbol] /.
				HoldPattern[RuleDelayed[left_, right_]] :>
					RuleDelayed[
						left
						,
						(
							oldNativeCalls++;
							right
						)
					]
	);

Scan[countWSTP, xAct`xPermLibraryLink`Private`$wrappedSymbols];

originalDefinitions = xAct`xPermLibraryLink`Private`CaptureDefinition /@
	 xAct`xPermLibraryLink`Private`$wrappedSymbols;

originalNativeFlag = OwnValues[xAct`xPerm`$xpermQ];

canonicalResult[] :=
	xAct`xPerm`CanonicalPerm[xAct`xPerm`Cycles[{1, 2}], 2, xAct`xPerm`GenSet[
		-xAct`xPerm`Cycles[{1, 2}]], {1, 2}, {}, xAct`xPerm`MathLink -> True];

expectedCanonical = -xAct`xPerm`Images[{1, 2}];

assert[canonicalResult[] === expectedCanonical && oldNativeCalls > 0,
	 "baseline canonicalization actually calls original WSTP"];

restoredQ[] :=
	originalDefinitions === (xAct`xPermLibraryLink`Private`CaptureDefinition
		 /@ xAct`xPermLibraryLink`Private`$wrappedSymbols);

noWSTPDefinitionsQ[] :=
	FreeQ[xAct`xPermLibraryLink`Private`CaptureDefinition /@ xAct`xPermLibraryLink`Private`$wrappedSymbols,
		 _ExternalCall];

liveOriginalQ[] :=
	xAct`xPerm`$xpermLink === originalLink && MemberQ[Links[], originalLink
		];

assertOriginalCallable[label_] :=
	Module[{before = oldNativeCalls},
		assert[restoredQ[], label <> ": restores all original definitions"]
			;
		assert[OwnValues[xAct`xPerm`$xpermQ] === originalNativeFlag, label 
			<> ": restores native dispatch flag"];
		assert[liveOriginalQ[], label <> ": preserves live original connection"
			];
		assert[canonicalResult[] === expectedCanonical && oldNativeCalls > 
			before, label <> ": original WSTP canonicalizer is callable"]
	];

assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, True] ===
	 True, "load bundled Niehoff library"];

assert[noWSTPDefinitionsQ[], "active wrappers contain no competing WSTP definitions"
	];

assert[liveOriginalQ[], "loading preserves original connection for unload"
	];

oldNativeCalls = 0;

nativeCanonical = xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm;

Clear[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm];

nativeCalls = 0;

xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm[args___] :=
	(
		nativeCalls++;
		nativeCanonical[args]
	);

assert[canonicalResult[] === expectedCanonical && nativeCalls > 0 && 
	oldNativeCalls === 0, "direct canonicalization calls Niehoff LibraryLink, never original WSTP"
	];

assert[xAct`xPerm`Private`MLOrbit[1, {2, 1, 3}, 3] === {1, 2} && oldNativeCalls
	 === 0, "orbit also replaces original WSTP definition"];

$DefInfoQ = False;

DefManifold[handoverM, 4, {ha, hb, hc, hd, he, hf, hg, hh, hi, hj, hk,
	 hl, hm}];

DefMetric[1, handoverMetric[-ha, -hb], handoverCD];

DefTensor[handoverF[-ha, -hb], handoverM, Antisymmetric[{-ha, -hb}]];

FProduct[n_] :=
	Module[{indices, counter},
		indices = GetIndicesOfVBundle[TangenthandoverM, n];
		counter = Flatten[Transpose[{indices, -indices}]];
		Times @@ Apply[handoverF, Partition[RotateLeft[counter], 2], {1}]
	];

beforeTensorCalls = nativeCalls;

ringTiming = AbsoluteTiming[ToCanonical[FProduct[125]]];

assert[Last[ringTiming] === 0 && nativeCalls > beforeTensorCalls && oldNativeCalls
	 === 0, "125-factor notebook example calls Niehoff LibraryLink, never original WSTP"
	];

assert[ToCanonical[FProduct[4]] =!= 0 && oldNativeCalls === 0, "even-factor ring remains nonzero"
	];

Print[<|"Factors" -> 125, "Seconds" -> First[ringTiming], "NiehoffCalls"
	 -> nativeCalls, "OriginalWSTPCalls" -> oldNativeCalls|>];

Clear[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm];

xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm = nativeCanonical;

assert[xAct`xPermLibraryLink`UnloadxPermLibraryLink[] === True, "unload"
	];

assertOriginalCallable["Unload"];

missingLibrary = FileNameJoin[{pacletPath, "does-not-exist.so"}];

assert[Quiet[xAct`xPermLibraryLink`LoadxPermLibraryLink[missingLibrary,
	 True]] === $Failed, "missing library reports failure"];

assertOriginalCallable["Failed load"];

(* Switching backends must not accumulate rules or reactivate ExternalCall. *)

assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, True] ===
	 True, "reload Niehoff"];

assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, False] ===
	 True, "switch to original LibraryLink canonicalizer"];

assert[noWSTPDefinitionsQ[], "switching backend leaves no WSTP definitions"
	];

nativeOriginalCanonical = xAct`xPermLibraryLink`Private`$llCanonicalPerm;

Clear[xAct`xPermLibraryLink`Private`$llCanonicalPerm];

nativeOriginalCalls = 0;

oldNativeCalls = 0;

xAct`xPermLibraryLink`Private`$llCanonicalPerm[args___] :=
	(
		nativeOriginalCalls++;
		nativeOriginalCanonical[args]
	);

assert[canonicalResult[] === expectedCanonical && nativeOriginalCalls
	 > 0 && oldNativeCalls === 0, "False selects original algorithm through LibraryLink, never WSTP"
	];

Clear[xAct`xPermLibraryLink`Private`$llCanonicalPerm];

xAct`xPermLibraryLink`Private`$llCanonicalPerm = nativeOriginalCanonical;

assert[Quiet[xAct`xPermLibraryLink`LoadxPermLibraryLink[missingLibrary,
	 True]] === $Failed, "failed reload reports failure"];

assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "failed reload leaves native library unloaded"
	];

assertOriginalCallable["Failed reload"];

assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, True] ===
	 True, "native load before source reload"];

Get[FileNameJoin[{pacletPath, "Kernel", "init.m"}]];

assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "source reload unloads native library"
	];

assertOriginalCallable["Source reload"];

assert[xAct`xPermLibraryLink`UnloadxPermLibraryLink[] === True, "repeated unload"
	];

assertOriginalCallable["Repeated unload"];

Print["Live WSTP to LibraryLink handover regressions passed"];

Quit[0];
