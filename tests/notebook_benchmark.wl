(* Reproduce the timed antisymmetric-ring workload in xpermLibraryLinkSample.nb.
   Use a fresh kernel for each library; load/build time is excluded.
   WolframKernel -noinit -noprompt -script tests/notebook_benchmark.wl \
     /absolute/path/libxpermLL.so 125 3
*)
scriptPosition = FirstPosition[System`$CommandLine, "-script"];
arguments = If[MissingQ[scriptPosition], Rest[System`$ScriptCommandLine],
  Drop[System`$CommandLine, First[scriptPosition] + 1]];
If[Length[arguments] < 1 || Length[arguments] > 3,
  Print["Usage: notebook_benchmark.wl LIBRARY [FACTORS=125] [REPEATS=3]"]; Quit[2]];
library = First[arguments];
factors = If[Length[arguments] >= 2, ToExpression[arguments[[2]]], 125];
repeats = If[Length[arguments] >= 3, ToExpression[arguments[[3]]], 3];
If[!IntegerQ[factors] || factors < 1 || !IntegerQ[repeats] || repeats < 1, Quit[2]];
Print["Kernel: ", System`$Version];
Needs["xAct`xTensor`"];
Get[FileNameJoin[{DirectoryName[DirectoryName[$InputFileName]], "xPermLibraryLink.m"}]];
If[xAct`xPermLibraryLink`LoadxPermLibraryLink[library, True] =!= True, Quit[2]];
(* A successful LibraryLink load must enable xPerm native dispatch even
   when its original WSTP executable is unavailable. *)
If[!TrueQ[xAct`xPerm`$xpermQ], Print["FAIL: native dispatch disabled"]; Quit[1]];
$DefInfoQ = False;
DefManifold[M, 4, IndexRange[a, m]];
DefMetric[1, metricg[-a, -b], CD, PrintAs -> "g"];
DefTensor[T[-a, -b], M, Antisymmetric[{-a, -b}]];
FProduct[n_] := Module[{indices, counter},
  indices = GetIndicesOfVBundle[TangentM, n];
  counter = Flatten[Transpose[{indices, -indices}]];
  Times @@ Apply[T, Partition[RotateLeft[counter], 2], {1}]
];
(* Assert that the native function was actually called during warmup. *)
native = xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm;
Clear[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm];
nativeCalls = 0;
xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm[args___] := (nativeCalls++; native[args]);
warmup = AbsoluteTiming[ToCanonical[FProduct[factors]]];
Clear[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm];
xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm = native;
If[nativeCalls == 0 || (Last[warmup] === 0) =!= OddQ[factors],
  Print["FAIL: missing native call or wrong odd/even result"]; Quit[1]];
runs = Table[AbsoluteTiming[ToCanonical[FProduct[factors]]], {repeats}];
If[!AllTrue[runs[[All, 2]], SameQ[#, Last[warmup]] &],
  Print["FAIL: inconsistent results"]; Quit[1]];
Print[<|"Library" -> library, "Factors" -> factors, "WarmupSeconds" -> First[warmup],
  "Seconds" -> runs[[All, 1]], "MedianSeconds" -> Median[runs[[All, 1]]],
  "Zero" -> (Last[warmup] === 0), "NativeCallsDuringWarmup" -> nativeCalls|>];
Quit[0];
