(* Run each case in a fresh kernel, with an isolated paclet repository:
   WolframKernel -noinit -noprompt -pacletbase /tmp/xperm-paclet-test \
     -script tests/paclet_wolfram.wl PACLET_PATH MODE /tmp/xperm-paclet-test [PRELOAD]
   MODE is staged (directory), install (archive), or installed.
   PRELOAD is auto (default) or preload, to cover both dependency states.
   The installed mode tests discovery in a fresh process after PacletInstall.
   xAct`xPerm` must be available through the normal $Path.
*)
scriptPosition = FirstPosition[System`$CommandLine, "-script"];
arguments = If[MissingQ[scriptPosition], Rest[System`$ScriptCommandLine],
  Drop[System`$CommandLine, First[scriptPosition] + 1]];
If[!MemberQ[{3, 4}, Length[arguments]],
  Print["Usage: paclet_wolfram.wl PACLET_PATH MODE ISOLATED_PACLET_BASE [auto|preload]"];
  Quit[2]
];
{pacletPath, mode, expectedBase} = Take[arguments, 3];
preload = If[Length[arguments] == 4, arguments[[4]], "auto"];
assert[condition_, label_] := If[!TrueQ[condition], Print["FAIL: ", label]; Quit[1]];
insideDirectoryQ[path_String, directory_String] := With[
  {parts = FileNameSplit[ExpandFileName[path]],
   prefix = FileNameSplit[ExpandFileName[directory]]},
  Length[parts] >= Length[prefix] && Take[parts, Length[prefix]] === prefix
];
assert[MemberQ[{"staged", "install", "installed"}, mode], "valid test mode"];
assert[MemberQ[{"auto", "preload"}, preload], "valid dependency mode"];
assert[MemberQ[System`$CommandLine, "-pacletbase"], "explicit isolated paclet repository"];
assert[ExpandFileName[$UserBasePacletsDirectory] === ExpandFileName[expectedBase],
  "kernel uses requested isolated paclet repository"];
assert[!insideDirectoryQ[expectedBase, $UserBaseDirectory],
  "test repository is outside the real Wolfram user directory"];
assert[!MemberQ[$Packages, "xAct`xPermLibraryLink`"], "fresh xPermLibraryLink context"];
assert[!MemberQ[$Packages, "xAct`xPerm`"], "fresh xPerm dependency"];
assert[StringQ[Quiet[FindFile["xAct`xPerm`"]]], "xAct`xPerm` dependency is installed"];
Print["Kernel: ", $Version, "; mode: ", mode, "; dependency: ", preload];

Switch[mode,
  "staged", PacletDirectoryLoad[ExpandFileName[pacletPath]],
  "install",
    installed = PacletInstall[ExpandFileName[pacletPath], ForceVersionInstall -> True];
    assert[Head[installed] === PacletObject, "PacletInstall succeeds"],
  "installed", Null
];
paclets = PacletFind["xPermLibraryLink"];
assert[Length[paclets] === 1, "exactly one test paclet is discoverable"];
pacletRoot = First[paclets]["Location"];
assert[StringQ[pacletRoot] && DirectoryQ[pacletRoot], "paclet has an on-disk location"];
If[mode === "staged",
  assert[ExpandFileName[pacletRoot] === ExpandFileName[pacletPath], "staged paclet selected"],
  assert[insideDirectoryQ[pacletRoot, expectedBase], "installed paclet is isolated"]
];
assert[Quiet[FindFile["xAct`xPermLibraryLink`"]] ===
  FileNameJoin[{pacletRoot, "Kernel", "init.m"}], "context resolves to guarded loader"];

If[preload === "preload",
  Quiet[Needs["xAct`xPerm`"]];
  assert[MemberQ[$Packages, "xAct`xPerm`"], "explicit xPerm preload"];
  xAct`xPerm`Private`MLOrbit[1000, {}, 1000] := "preloaded definition";
  definitionsBeforeNeeds = DownValues[xAct`xPerm`Private`MLOrbit],
  (* A missing dependency must leave the paclet retryable. Paclet discovery
     works independently of $Path; xAct's conventional package does not. *)
  Block[{$Path = {}}, Quiet[Needs["xAct`xPermLibraryLink`"]]];
  assert[!MemberQ[$Packages, "xAct`xPermLibraryLink`"],
    "missing dependency does not mark the paclet loaded"];
  assert[!MemberQ[$Packages, "xAct`xPerm`"], "missing dependency was not loaded"];
  assert[DownValues[xAct`xPermLibraryLink`LoadxPermLibraryLink] === {},
    "missing dependency does not evaluate implementation"]
];

(* Needs itself must load xPerm when it was not preloaded. *)
Quiet[Needs["xAct`xPermLibraryLink`"]];
assert[MemberQ[$Packages, "xAct`xPermLibraryLink`"], "paclet context loaded with Needs"];
assert[MemberQ[$Packages, "xAct`xPerm`"], "xPerm dependency loaded"];
assert[!MemberQ[$Packages, "xPermLibraryLink`"], "obsolete context is not registered"];
assert[And @@ (StringQ /@ {
  xAct`xPermLibraryLink`LoadxPermLibraryLink::usage,
  xAct`xPermLibraryLink`UnloadxPermLibraryLink::usage,
  xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ::usage}), "public usage messages retained"];
assert[And @@ (StringQ /@ {
  xAct`xPermLibraryLink`LoadxPermLibraryLink::load,
  xAct`xPermLibraryLink`LoadxPermLibraryLink::nolib,
  xAct`xPermLibraryLink`LoadxPermLibraryLink::format}), "load diagnostics retained"];
assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "Needs leaves native loading explicit"];
If[preload === "preload",
  assert[DownValues[xAct`xPerm`Private`MLOrbit] === definitionsBeforeNeeds,
    "Needs preserves a preloaded xPerm definition"]
];

xAct`xPerm`Private`MLOrbit[1000, {}, 1000] := "original definition";
xAct`xPerm`$xpermQ = (preload === "preload");
originalNativeFlag = OwnValues[xAct`xPerm`$xpermQ];
originalDefinitions = xAct`xPermLibraryLink`Private`CaptureDefinition /@
  xAct`xPermLibraryLink`Private`$wrappedSymbols;
(* An empty external search path ensures default loading uses the paclet's
   own LibraryResources/$SystemID binary. *)
$LibraryPath = {};
assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[] === True, "default bundled native load"];
assert[xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "native loaded flag"];
assert[TrueQ[xAct`xPerm`$xpermQ], "native load enables xPerm dispatch"];
assert[!FreeQ[DownValues[xAct`xPerm`Private`MLCanonicalPerm],
  HoldPattern[xAct`xPermLibraryLink`Private`$llCanonicalPerm]] &&
  FreeQ[DownValues[xAct`xPerm`Private`MLCanonicalPerm],
  HoldPattern[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm]],
  "default canonicalizer remains original xPerm"];
assert[insideDirectoryQ[xAct`xPermLibraryLink`Private`$library,
  FileNameJoin[{pacletRoot, "LibraryResources", $SystemID}]], "bundled binary selected"];
assert[xAct`xPerm`Private`MLOrbit[1, {2, 1, 3}, 3] === {1, 2}, "native orbit result"];
assert[xAct`xPermLibraryLink`Private`$llCanonicalPerm[
  {1, 2, 3, 4}, 4, 0, {}, {}, {1, 2}, {}, {}, {}, {}, {}] === {1, 2, 3, 4},
  "native canonical identity"];
assert[xAct`xPermLibraryLink`UnloadxPermLibraryLink[] === True, "unload"];
assert[OwnValues[xAct`xPerm`$xpermQ] === originalNativeFlag,
  "unload restores xPerm dispatch flag"];
assert[originalDefinitions === (xAct`xPermLibraryLink`Private`CaptureDefinition /@
  xAct`xPermLibraryLink`Private`$wrappedSymbols), "unload restores original definitions"];
assert[xAct`xPerm`Private`MLOrbit[1000, {}, 1000] === "original definition",
  "restored xPerm definition is callable"];
assert[Quiet[xAct`xPermLibraryLink`LoadxPermLibraryLink[
  FileNameJoin[{pacletRoot, "does-not-exist.so"}]]] === $Failed, "failed native load"];
assert[OwnValues[xAct`xPerm`$xpermQ] === originalNativeFlag,
  "failed load preserves xPerm dispatch flag"];
assert[originalDefinitions === (xAct`xPermLibraryLink`Private`CaptureDefinition /@
  xAct`xPermLibraryLink`Private`$wrappedSymbols), "failed load preserves definitions"];

assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[Automatic, True] === True,
  "bundled Niehoff load"];
assert[xAct`xPerm`CanonicalPerm[xAct`xPerm`Cycles[], 2,
  xAct`xPerm`GenSet[-xAct`xPerm`Cycles[{1, 2}]], {},
  {xAct`xPerm`DummySet[{1, 2}, {{1, 2}}, 1]}, xAct`xPerm`MathLink -> True] === 0,
  "xPerm vanishing antisymmetric contraction"];
(* Loading xTensor after the paclet must still use the native canonicalizer. *)
Quiet[Needs["xAct`xTensor`"]];
$DefInfoQ = False;
DefManifold[pacletTestM, 3, {pa, pb, pc, pd, pe, pf}];
DefMetric[1, pacletTestMetric[-pa, -pb], pacletTestCD];
DefTensor[pacletTestF[-pa, -pb], pacletTestM, Antisymmetric[{-pa, -pb}]];
nativeCanonical = xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm;
Clear[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm];
nativeCalls = 0;
xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm[args___] :=
  (nativeCalls++; nativeCanonical[args]);
contraction = ToCanonical[pacletTestF[-pa, pb] pacletTestF[-pb, pc] pacletTestF[-pc, pa]];
Clear[xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm];
xAct`xPermLibraryLink`Private`$llNiehoffCanonicalPerm = nativeCanonical;
assert[contraction === 0 && nativeCalls > 0, "xTensor loaded later uses native canonicalization"];
Get[FileNameJoin[{pacletRoot, "Kernel", "init.m"}]];
assert[!xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[], "source reload unloads native functions"];
assert[OwnValues[xAct`xPerm`$xpermQ] === originalNativeFlag,
  "source reload restores xPerm dispatch flag"];
assert[originalDefinitions === (xAct`xPermLibraryLink`Private`CaptureDefinition /@
  xAct`xPermLibraryLink`Private`$wrappedSymbols), "source reload restores definitions"];
assert[xAct`xPermLibraryLink`LoadxPermLibraryLink[] === True, "native load after source reload"];
assert[xAct`xPermLibraryLink`UnloadxPermLibraryLink[] === True, "final unload"];
assert[OwnValues[xAct`xPerm`$xpermQ] === originalNativeFlag,
  "final unload restores xPerm dispatch flag"];
assert[originalDefinitions === (xAct`xPermLibraryLink`Private`CaptureDefinition /@
  xAct`xPermLibraryLink`Private`$wrappedSymbols), "final unload restores definitions"];
Print["Paclet integration regressions passed (", mode, ", ", preload, ")"];
Quit[0];
