(* ::Package:: *)

BeginPackage["xPermLibraryLink`"];

LoadxPermLibraryLink::usage =
  "LoadxPermLibraryLink[library, niehoff] loads the plain-LibraryLink xPerm functions. " <>
  "library may be an absolute library path or a name accepted by LibraryFunctionLoad."<>
  "If niehoff is True, the new algorithm is used for CanonicalPerm.";
UnloadxPermLibraryLink::usage =
  "UnloadxPermLibraryLink[] unloads all native functions loaded by LoadxPermLibraryLink.";
xPermLibraryLinkLoadedQ::usage =
  "xPermLibraryLinkLoadedQ[] returns True when the native functions are loaded.";

Begin["`Private`"];

$loadedFunctions = {};
$library = None;
$originalDefinitions = {};
$wrappedSymbols = {
  HoldComplete[xAct`xPerm`Private`MLCanonicalPerm],
  HoldComplete[xAct`xPerm`Private`MLSchreierSims],
  HoldComplete[xAct`xPerm`Private`MLOrbit],
  HoldComplete[xAct`xPerm`Private`MLSetStabilizer],
  HoldComplete[xAct`xPerm`Private`MLBaseChange],
  HoldComplete[xAct`xPerm`Private`MLBaseChangeStabilizerChain],
  HoldComplete[xAct`xPerm`Private`MLStabilizerSGS]
};

CaptureDefinition[HoldComplete[symbol_]] :=
  {OwnValues[symbol], DownValues[symbol], SubValues[symbol], UpValues[symbol]};
RestoreDefinition[HoldComplete[symbol_], values_List] := (
  OwnValues[symbol] = values[[1]];
  DownValues[symbol] = values[[2]];
  SubValues[symbol] = values[[3]];
  UpValues[symbol] = values[[4]];
);

LoadxPermLibraryLink::load = "Could not load the xPerm LibraryLink library `1`.";
LoadxPermLibraryLink::format = "Unexpected structured result returned by native function `1`.";

IntegerVector = {Integer, 1};

ClearAll[LibLoad];
LibLoad[name_, arguments_, result_] := Module[{function},
  function = LibraryFunctionLoad[$library, name, arguments, result];
  AppendTo[$loadedFunctions, function];
  function
];

ClearAll[DecodeStrongGenSet];
DecodeStrongGenSet[encoded_List] := Module[
  {degree, baseLength, generatorCount, expectedLength, base, generators},

  If[Length[encoded] < 3,
    Message[LoadxPermLibraryLink::format, "StrongGenSet"];
    Return[$Failed]
  ];

  {degree, baseLength, generatorCount} = Take[encoded, 3];
  expectedLength = 3 + baseLength + degree generatorCount;

  If[degree <= 0 || baseLength < 0 || generatorCount < 0 ||
      Length[encoded] =!= expectedLength,
    Message[LoadxPermLibraryLink::format, "StrongGenSet"];
    Return[$Failed]
  ];

  base = Take[Drop[encoded, 3], baseLength];
  generators = Drop[encoded, 3 + baseLength];
  xAct`xPerm`StrongGenSet[base, generators, degree]
];

ClearAll[DecodeStabilizerChain];
DecodeStabilizerChain[encoded_List] := Module[
  {streamCount, streamLengths, payload, streams, decoded},

  If[encoded === {} || First[encoded] < 0,
    Message[LoadxPermLibraryLink::format, "StrongGenSet chain"];
    Return[$Failed]
  ];

  streamCount = First[encoded];
  If[Length[encoded] < 1 + streamCount,
    Message[LoadxPermLibraryLink::format, "StrongGenSet chain"];
    Return[$Failed]
  ];

  streamLengths = Take[Rest[encoded], streamCount];
  payload = Drop[encoded, 1 + streamCount];
  If[AnyTrue[streamLengths, # < 3 &] || Total[streamLengths] =!= Length[payload],
    Message[LoadxPermLibraryLink::format, "StrongGenSet chain"];
    Return[$Failed]
  ];

  streams = TakeList[payload, streamLengths];
  decoded = DecodeStrongGenSet /@ streams;
  If[MemberQ[decoded, $Failed], $Failed, decoded]
];

EncodeTotalSymmetrySubsets[subsets_List]:=Module[{slots,lengths},slots=If[subsets==={},{},Join@@subsets[[All,2]]];
lengths=If[subsets==={},{},Length/@subsets[[All,2]]];
Developer`ToPackedArray[#,Integer]&/@{FoldList[Plus,0,lengths],slots,If[subsets==={},{},subsets[[All,1]]]}];

ClearAll[LoadxPermLibraryLink];
LoadxPermLibraryLink[library_,Niehoff_:False] := Module[{},
  If[TrueQ[xPermLibraryLinkLoadedQ[]], UnloadxPermLibraryLink[]];
  $library = library;
  $loadedFunctions = {};
  Quiet[Check[
    $llCanonicalPerm = LibLoad["LL_canonical_perm",
      {IntegerVector, Integer, Integer, IntegerVector, IntegerVector,
       IntegerVector, IntegerVector, IntegerVector, IntegerVector,
       IntegerVector, IntegerVector}, IntegerVector];
    $llNiehoffCanonicalPerm = LibLoad["LL_niehoff_canonical_perm",
      {IntegerVector, Integer, Integer, IntegerVector, IntegerVector,
       IntegerVector, IntegerVector, IntegerVector, IntegerVector,
       IntegerVector, IntegerVector}, IntegerVector];
    $llSchreierSims = LibLoad["LL_schreier_sims",
      {IntegerVector, IntegerVector, Integer}, IntegerVector];
    $llOrbit = LibLoad["LL_orbit", {Integer, IntegerVector, Integer}, IntegerVector];
    $llSetStabilizer = LibLoad["LL_set_stabilizer",
      {IntegerVector, Integer, IntegerVector, IntegerVector}, IntegerVector];
    $llBaseChangeStabilizerChain = LibLoad["LL_basechangestabchain",
      {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerVector];
    $llBaseChange = LibLoad["LL_basechange",
      {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerVector];
    $llStabilizerSGS = LibLoad["LL_stabsgs",
      {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerVector];,
    UnloadxPermLibraryLink[];
    Message[LoadxPermLibraryLink::load, library];
    Return[$Failed]
  ]];
  
  (* Preserve xPerm's existing WSTP definitions for unload and failed reloads. *)
  $originalDefinitions = CaptureDefinition /@ $wrappedSymbols;
  (* Compatibility wrappers expected by xAct/xPerm. *)
  If[Niehoff,
  xAct`xPerm`Private`MLCanonicalPerm[perm_List, degree_Integer, rest__] := 
    xAct`xPerm`Private`ToSign[xAct`xPerm`Images[$llNiehoffCanonicalPerm[perm, degree, rest]], degree - 2],
    xAct`xPerm`Private`MLCanonicalPerm[perm_List, degree_Integer, rest__] := 
    xAct`xPerm`Private`ToSign[xAct`xPerm`Images[$llCanonicalPerm[perm, degree, rest]], degree - 2];
    ];
  xAct`xPerm`Private`MLSchreierSims[base_List, generators_List, degree_Integer] := 
  DecodeStrongGenSet[$llSchreierSims[base, generators, degree]];
  xAct`xPerm`Private`MLOrbit[point_Integer, generators_List, degree_Integer] := $llOrbit[point, generators, degree];
  xAct`xPerm`Private`MLSetStabilizer[points_List, degree_Integer, base_List, generators_List] := 
  DecodeStrongGenSet[$llSetStabilizer[points, degree, base, generators]];
  xAct`xPerm`Private`MLBaseChange[degree_Integer, base_List, generators_List, newBase_List] := 
  DecodeStrongGenSet[$llBaseChange[degree, base, generators, newBase]];
  xAct`xPerm`Private`MLBaseChangeStabilizerChain[degree_Integer, base_List, generators_List, newBase_List] := 
  DecodeStabilizerChain[$llBaseChangeStabilizerChain[degree, base, generators, newBase]];
  xAct`xPerm`Private`MLStabilizerSGS[degree_Integer, base_List, generators_List, points_List] := 
  DecodeStrongGenSet[$llStabilizerSGS[degree, base, generators, points]];
  True
];

ClearAll[xPermLibraryLinkLoadedQ];
xPermLibraryLinkLoadedQ[] := Length[$loadedFunctions] == 8;

ClearAll[UnloadxPermLibraryLink];
UnloadxPermLibraryLink[] := Module[{},
  Scan[Quiet[LibraryFunctionUnload[#]] &, $loadedFunctions];
  $loadedFunctions = {};
  $library = None;
  If[Length[$originalDefinitions] === Length[$wrappedSymbols],
    MapThread[RestoreDefinition, {$wrappedSymbols, $originalDefinitions}]
  ];
  $originalDefinitions = {};
  True
];

End[];
EndPackage[];
