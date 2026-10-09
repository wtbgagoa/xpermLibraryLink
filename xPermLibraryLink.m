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

LoadxPermLibraryLink::load = "Could not load the xPerm LibraryLink library `1`.";
LoadxPermLibraryLink::format = "Unexpected structured result returned by native function `1`.";

IntegerVector = {Integer, 1};
IntegerMatrix = {Integer, 2};

ClearAll[LibLoad];
LibLoad[name_, arguments_, result_] := Module[{function},
  function = LibraryFunctionLoad[$library, name, arguments, result];
  AppendTo[$loadedFunctions, function];
  function
];

ClearAll[RowData];
RowData[row_List] := Take[row, {4, 3 + row[[3]]}];

ClearAll[DecodeStrongGenSet];
DecodeStrongGenSet[encoded_?MatrixQ] := Module[{degreeRows, baseRows, generatorRows},
  degreeRows = Select[encoded, #[[1]] == 0 &];
  baseRows = Select[encoded, #[[1]] == 1 &];
  generatorRows = Select[encoded, #[[1]] == 2 &];
  If[Length[degreeRows] != 1 || Length[baseRows] != 1 ||
      Length[generatorRows] != 1,
    Message[LoadxPermLibraryLink::format, "StrongGenSet"];
    Return[$Failed]
  ];
  xAct`xPerm`StrongGenSet[
    RowData[First[baseRows]],
    RowData[First[generatorRows]],
    First[RowData[First[degreeRows]]]
  ]
];

ClearAll[DecodeStabilizerChain];
DecodeStabilizerChain[encoded_?MatrixQ] := Module[{indices},
  indices = Sort[DeleteDuplicates[encoded[[All, 2]]]];
  Map[
    Function[index,
      DecodeStrongGenSet[Map[ReplacePart[#, 2 -> 0] &,
        Select[encoded, #[[2]] == index &]]]
    ],
    indices
  ]
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
      {IntegerVector, IntegerVector, Integer}, IntegerMatrix];
    $llOrbit = LibLoad["LL_orbit", {Integer, IntegerVector, Integer}, IntegerVector];
    $llSetStabilizer = LibLoad["LL_set_stabilizer",
      {IntegerVector, Integer, IntegerVector, IntegerVector}, IntegerMatrix];
    $llBaseChangeStabilizerChain = LibLoad["LL_basechangestabchain",
      {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerMatrix];
    $llBaseChange = LibLoad["LL_basechange",
      {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerMatrix];
    $llStabilizerSGS = LibLoad["LL_stabsgs",
      {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerMatrix];,
    UnloadxPermLibraryLink[];
    Message[LoadxPermLibraryLink::load, library];
    Return[$Failed]
  ]];
  
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
  Clear[
    xAct`xPerm`Private`MLCanonicalPerm,
    xAct`xPerm`Private`MLSchreierSims,
    xAct`xPerm`Private`MLOrbit,
    xAct`xPerm`Private`MLSetStabilizer,
    xAct`xPerm`Private`MLBaseChange,
    xAct`xPerm`Private`MLBaseChangeStabilizerChain,
    xAct`xPerm`Private`MLStabilizerSGS
  ];
  True
];

End[];
EndPackage[];
