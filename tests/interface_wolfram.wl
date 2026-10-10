(* Run manually with a licensed Wolfram kernel:
   WolframKernel -noinit -noprompt -script tests/interface_wolfram.wl /absolute/path/libxpermLL.so
   xAct integration is included when xAct`xPerm` is installed. *)
scriptArguments = If[MemberQ[$CommandLine, "-script"],
  Drop[$CommandLine, First[FirstPosition[$CommandLine, "-script"]]], $ScriptCommandLine];
If[Length[scriptArguments] =!= 2,
  Print["Usage: WolframKernel -script tests/interface_wolfram.wl /absolute/path/libxpermLL.so"];
  Quit[2]
];
library = Last[scriptArguments];
package = FileNameJoin[{DirectoryName[DirectoryName[$InputFileName]], "xPermLibraryLink.m"}];
assert[condition_, label_] := If[!TrueQ[condition], Print["FAIL: ", label]; Quit[1]];
Print["Kernel: ", $Version];
hasXPerm = StringQ[Quiet[FindFile["xAct`xPerm`"]]];
If[hasXPerm, Quiet[Needs["xAct`xPerm`"]]];
Get[package];
(* A sentinel verifies restoration even if xAct's WSTP program is unavailable. *)
xAct`xPerm`Private`MLOrbit[1000, {}, 1000] := "original definition";
originalDefinitions = xPermLibraryLink`Private`CaptureDefinition /@
  xPermLibraryLink`Private`$wrappedSymbols;
assert[xPermLibraryLink`LoadxPermLibraryLink[library, True] === True, "load"];
assert[xPermLibraryLink`xPermLibraryLinkLoadedQ[], "loaded flag"];
assert[xAct`xPerm`Private`MLOrbit[1, {2, 1, 3}, 3] === {1, 2}, "orbit"];
assert[xPermLibraryLink`Private`$llBaseChange[3, {}, {}, {2}] === {3, 1, 0, 2},
  "identity group base change"];
assert[xPermLibraryLink`Private`$llStabilizerSGS[3, {1}, {2, 1, 3}, {1}] === {3, 0, 0},
  "full-base stabilizer"];
assert[xPermLibraryLink`Private`$llNiehoffCanonicalPerm[
  {1, 2, 3, 4}, 4, 0, {}, {}, {1, 2}, {}, {}, {}, {}, {}] === {1, 2, 3, 4},
  "canonical identity"];
assert[MatchQ[Quiet[xPermLibraryLink`Private`$llCanonicalPerm[
  {1, 2, 3, 4}, 4, 0, {}, {}, {}, {2}, {1, 2}, {}, {}, {}]], _LibraryFunctionError],
  "malformed legacy input"];
stage6 = LibraryFunctionLoad[library, "LL_niehoff_propagated_symmetry_search",
  {{Integer, 2}, {Integer, 2}, {Integer, 1}, {Integer, 2}, {Integer, 2},
   {Integer, 1}, {Integer, 1}, {Integer, 2}, {Integer, 2}, Integer}, {Integer, 2}];
result = stage6[{{3, 2, 1}}, {{1, 2, 3}}, {1}, {{7}}, {{2, 1, 3}}, {1}, {1},
  {{0, 1, 1}, {0, 1, 2}, {0, 1, 3}}, {{1, 2, 1, 3}}, 100];
assert[MatrixQ[result, IntegerQ] && Last[Dimensions[result]] === 16,
  "propagated symmetry output width"];
assert[Sort[result[[1, 7 ;; 9]]] === Range[3] &&
       Sort[result[[1, 10 ;; 12]]] === Range[3] && result[[1, 13]] === 7,
  "propagated symmetry output fields"];
LibraryFunctionUnload[stage6];
If[hasXPerm,
  assert[xAct`xPerm`CanonicalPerm[xAct`xPerm`Cycles[{1, 2}], 2,
    xAct`xPerm`GenSet[xAct`xPerm`Cycles[{1, 2}]], {1, 2}, {},
    xAct`xPerm`MathLink -> True] === xAct`xPerm`Images[{1, 2}],
    "xAct symmetric free indices"];
  assert[xAct`xPerm`CanonicalPerm[xAct`xPerm`Cycles[{1, 2}], 2,
    xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2}]], {1, 2}, {},
    xAct`xPerm`MathLink -> True] === -xAct`xPerm`Images[{1, 2}],
    "xAct antisymmetric free indices"];
  assert[xAct`xPerm`CanonicalPerm[xAct`xPerm`Cycles[], 2,
    xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2}]], {},
    {xAct`xPerm`DummySet[{1, 2}, {{1, 2}}, 1]}, xAct`xPerm`MathLink -> True] === 0,
    "xAct vanishing contraction"];
];
assert[xPermLibraryLink`UnloadxPermLibraryLink[], "unload"];
assert[originalDefinitions === (xPermLibraryLink`Private`CaptureDefinition /@
  xPermLibraryLink`Private`$wrappedSymbols), "restore definitions"];
assert[xAct`xPerm`Private`MLOrbit[1000, {}, 1000] === "original definition",
  "restored definition is callable"];
assert[xPermLibraryLink`UnloadxPermLibraryLink[], "repeated unload"];
assert[Quiet[xPermLibraryLink`LoadxPermLibraryLink["/nonexistent/xpermLL.so"]] === $Failed,
  "load failure"];
assert[originalDefinitions === (xPermLibraryLink`Private`CaptureDefinition /@
  xPermLibraryLink`Private`$wrappedSymbols), "load failure preserves definitions"];
Print["LibraryLink Wolfram regressions passed", If[hasXPerm, " (including xAct)", ""]];
Quit[0];
