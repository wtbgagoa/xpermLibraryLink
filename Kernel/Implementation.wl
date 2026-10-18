(* ::Package:: *)

(* Restore active wrappers before clearing private bookkeeping on source reload. *)
If[TrueQ[xAct`xPermLibraryLink`xPermLibraryLinkLoadedQ[]],
    xAct`xPermLibraryLink`UnloadxPermLibraryLink[]
];

xAct`xPermLibraryLink`$Version = {"0.3.1", {2026, 10, 18}};

With[
	{
		xAct`xPermLibraryLink`Private`xIdealSymbols = 
			DeleteCases[
				Join[Names["xAct`xPermLibraryLink`*"], Names["xAct`xPermLibraryLink`Private`*"]], 
				"$Version" | "xAct`xPermLibraryLink`$Version" | "$xTensorVersionExpected" | 
				"xAct`xPermLibraryLink`$xTensorVersionExpected"
			]
	},
	Unprotect /@ xAct`xPermLibraryLink`Private`xIdealSymbols;
	Clear /@ xAct`xPermLibraryLink`Private`xIdealSymbols;
]

If[Unevaluated[xAct`xCore`Private`$LastPackage] === xAct`xCore`Private`$LastPackage,
	
	xAct`xCore`Private`$LastPackage = "xAct`xPermLibraryLink`"
];
BeginPackage["xAct`xPermLibraryLink`"];

Print[xAct`xCore`Private`bars]

Print["Package xAct`xPermLibraryLink`  version ", xAct`xPermLibraryLink`$Version[[1]], ", ",
	 xAct`xPermLibraryLink`$Version[[2]]];

Print["Copyright (C) 2026, Thomas B\[ADoubleDot]ckdahl and Alfonso Garcia-Parrado, under the General Public License."];

Off[General::shdw]

xAct`xPermLibraryLink`Disclaimer[] :=
	Print[                                                   "These are points 11 and 12 of the General Public License:\n\n
BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW. 
EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM `AS IS\.b4 
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU. 
SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.\n\n
IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO 
MAY MODIFY AND/OR REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, 
INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO LOSS OF 
DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM 
TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES."
		]

On[General::shdw]

LoadxPermLibraryLink::usage = "LoadxPermLibraryLink[] loads the bundled xPerm functions and selects the original xPerm canonicalizer. " <>
    "LoadxPermLibraryLink[Method -> method] selects \"xPerm\", \"Niehoff\", or \"TensorReduce\" as the default backend. " <>
    "LoadxPermLibraryLink[library, options] accepts an explicit library path or name. " <>
    "The older LoadxPermLibraryLink[library, niehoff] form is retained: True selects Niehoff and False selects xPerm. " <>
    "CanonicalPerm and ToCanonical accept per-call Method overrides while the library is loaded.";

UnloadxPermLibraryLink::usage = "UnloadxPermLibraryLink[] unloads all native functions loaded by LoadxPermLibraryLink.";

xPermLibraryLinkLoadedQ::usage = "xPermLibraryLinkLoadedQ[] returns True when the native functions are loaded.";

Begin["`Private`"];

(* Get can reload the package during development. Restore xPerm's definitions
   before resetting the bookkeeping for an already active native library. *)

If[TrueQ[xPermLibraryLinkLoadedQ[]],
	UnloadxPermLibraryLink[]
];

$packageDirectory = DirectoryName[ExpandFileName[$InputFileName]];

$loadedFunctions = {};

$library = None;

$originalDefinitions = {};

$originalXPermQ = None;

$wrappedSymbols = {HoldComplete[xAct`xPerm`Private`MLCanonicalPerm], 
	HoldComplete[xAct`xPerm`Private`MLSchreierSims], HoldComplete[xAct`xPerm`Private`MLOrbit
	], HoldComplete[xAct`xPerm`Private`MLSetStabilizer], HoldComplete[xAct`xPerm`Private`MLBaseChange
	], HoldComplete[xAct`xPerm`Private`MLBaseChangeStabilizerChain], HoldComplete[
	xAct`xPerm`Private`MLStabilizerSGS]};

CaptureDefinition[HoldComplete[symbol_]] :=
	{OwnValues[symbol], DownValues[symbol], SubValues[symbol], UpValues[
		symbol]};

RestoreDefinition[HoldComplete[symbol_], values_List] :=
	(
		OwnValues[symbol] = values[[1]];
		DownValues[symbol] = values[[2]];
		SubValues[symbol] = values[[3]];
		UpValues[symbol] = values[[4]];
	);

LoadxPermLibraryLink::load = "Could not load the xPerm LibraryLink library `1`.";

LoadxPermLibraryLink::format = "Unexpected structured result returned by native function `1`.";

LoadxPermLibraryLink::nolib = "No bundled xPerm LibraryLink library was found for `1` in `2`. Build and install the paclet, or supply an explicit library path.";

DefaultLibrary[] :=
	Module[{directory, names, library},
		directory = FileNameJoin[{DirectoryName[$packageDirectory], "LibraryResources",
			 $SystemID}];
		names =
			Switch[$OperatingSystem,
				"Windows",
					{"xpermLL.dll", "libxpermLL.dll"}
				,
				"MacOSX",
					{"libxpermLL.dylib"}
				,
				_,
					{"libxpermLL.so"}
			];
		library = SelectFirst[FileNameJoin[{directory, #}]& /@ names, FileExistsQ,
			 $Failed];
		If[library === $Failed,
			Message[LoadxPermLibraryLink::nolib, $SystemID, directory]
		];
		library
	];

IntegerVector = {Integer, 1};

ClearAll[LibLoad];

LibLoad[name_, arguments_, result_] :=
	Module[{function},
		function = LibraryFunctionLoad[$library, name, arguments, result];
		AppendTo[$loadedFunctions, function];
		function
	];

ClearAll[DecodeStrongGenSet];

DecodeStrongGenSet[encoded_List] :=
	Module[{degree, baseLength, generatorCount, expectedLength, base, generators
		},
		If[Length[encoded] < 3,
			Message[LoadxPermLibraryLink::format, "StrongGenSet"];
			Return[$Failed]
		];
		{degree, baseLength, generatorCount} = Take[encoded, 3];
		expectedLength = 3 + baseLength + degree generatorCount;
		If[degree <= 0 || baseLength < 0 || generatorCount < 0 || Length[encoded
			] =!= expectedLength,
			Message[LoadxPermLibraryLink::format, "StrongGenSet"];
			Return[$Failed]
		];
		base = Take[Drop[encoded, 3], baseLength];
		generators = Drop[encoded, 3 + baseLength];
		xAct`xPerm`StrongGenSet[base, generators, degree]
	];

ClearAll[DecodeStabilizerChain];

DecodeStabilizerChain[encoded_List] :=
	Module[{streamCount, streamLengths, payload, streams, decoded},
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
		If[AnyTrue[streamLengths, # < 3&] || Total[streamLengths] =!= Length[
			payload],
			Message[LoadxPermLibraryLink::format, "StrongGenSet chain"];
			Return[$Failed]
		];
		streams = TakeList[payload, streamLengths];
		decoded = DecodeStrongGenSet /@ streams;
		If[MemberQ[decoded, $Failed],
			$Failed
			,
			decoded
		]
	];

EncodeTotalSymmetrySubsets[subsets_List] :=
	Module[{slots, lengths},
		slots =
			If[subsets === {},
				{}
				,
				Join @@ subsets[[All, 2]]
			];
		lengths =
			If[subsets === {},
				{}
				,
				Length /@ subsets[[All, 2]]
			];
		Developer`ToPackedArray[#, Integer]& /@
			{
				FoldList[Plus, 0, lengths]
				,
				slots
				,
				If[subsets === {},
					{}
					,
					subsets[[All, 1]]
				]
			}
	];

Clear[LoadxPermLibraryLink];

Options[LoadxPermLibraryLink] = {Method -> Automatic};

LoadxPermLibraryLink[options:OptionsPattern[]] :=
    LoadSelectedBackend[Automatic, False, OptionValue[Method]];

LoadxPermLibraryLink[library:(Automatic | _String), options:OptionsPattern[]] :=
    LoadSelectedBackend[library, False, OptionValue[Method]];

LoadxPermLibraryLink[library:(Automatic | _String), niehoff:(True | False), options:OptionsPattern[]] :=
    LoadSelectedBackend[library, niehoff, OptionValue[Method]];

LoadSelectedBackend[library_, niehoff_, requestedMethod_] :=
    Module[{method},
        method = If[requestedMethod === Automatic, If[niehoff, "Niehoff", "xPerm"], requestedMethod];
        If[!MemberQ[{"xPerm", "Niehoff", "TensorReduce"}, method],
            Message[LoadxPermLibraryLink::method, requestedMethod];
            Return[$Failed]
        ];
        LoadNativeLibrary[library, method]
    ];

LoadNativeLibrary[library_, method_] :=
	Module[{resolvedLibrary, loaded},
		resolvedLibrary =
			If[library === Automatic,
				DefaultLibrary[]
				,
				library
			];
		If[resolvedLibrary === $Failed,
			Return[$Failed]
		];
		If[TrueQ[xPermLibraryLinkLoadedQ[]],
			UnloadxPermLibraryLink[]
		];
		$library = resolvedLibrary;
		$loadedFunctions = {};
		loaded =
			Quiet[
				Check[
					$llCanonicalPerm = LibLoad["LL_canonical_perm", {IntegerVector, 
						Integer, Integer, IntegerVector, IntegerVector, IntegerVector, IntegerVector,
						 IntegerVector, IntegerVector, IntegerVector, IntegerVector}, IntegerVector
						];
					$llNiehoffCanonicalPerm = LibLoad["LL_niehoff_canonical_perm", {
						IntegerVector, Integer, Integer, IntegerVector, IntegerVector, IntegerVector,
						 IntegerVector, IntegerVector, IntegerVector, IntegerVector, IntegerVector
						}, IntegerVector];
					$llSchreierSims = LibLoad["LL_schreier_sims", {IntegerVector, IntegerVector,
						 Integer}, IntegerVector];
					$llOrbit = LibLoad["LL_orbit", {Integer, IntegerVector, Integer},
						 IntegerVector];
					$llSetStabilizer = LibLoad["LL_set_stabilizer", {IntegerVector, 
						Integer, IntegerVector, IntegerVector}, IntegerVector];
					$llBaseChangeStabilizerChain = LibLoad["LL_basechangestabchain",
						 {Integer, IntegerVector, IntegerVector, IntegerVector}, IntegerVector
						];
					$llBaseChange = LibLoad["LL_basechange", {Integer, IntegerVector,
						 IntegerVector, IntegerVector}, IntegerVector];
					$llStabilizerSGS = LibLoad["LL_stabsgs", {Integer, IntegerVector,
						 IntegerVector, IntegerVector}, IntegerVector];
					True
					,
					False
				]
			];
		If[!TrueQ[loaded],
			UnloadxPermLibraryLink[];
			Message[LoadxPermLibraryLink::load, resolvedLibrary];
			Return[$Failed]
		];
		(* Preserve xPerm's existing WSTP definitions for unload and failed reloads.
		Do not leave those rules active: the installed, fixed-arity MLCanonicalPerm rule is more 
		specific than the LibraryLink rest__ rule *)
		$originalDefinitions = CaptureDefinition /@ $wrappedSymbols;
		$originalXPermQ = OwnValues[xAct`xPerm`$xpermQ];
		Scan[RestoreDefinition[#, {{}, {}, {}, {}}]&, $wrappedSymbols];
		(* Compatibility wrappers expected by xAct/xPerm. *)
		If[method === "Niehoff",
			xAct`xPerm`Private`MLCanonicalPerm[perm_List, degree_Integer, rest__] := 
				xAct`xPerm`Private`ToSign[
					xAct`xPerm`Images[$llNiehoffCanonicalPerm[perm, degree, rest]], degree - 2]
			,
			xAct`xPerm`Private`MLCanonicalPerm[perm_List, degree_Integer, rest__] := 
				xAct`xPerm`Private`ToSign[xAct`xPerm`Images[
					$llCanonicalPerm[perm, degree, rest]], degree - 2];
		];
		xAct`xPerm`Private`MLSchreierSims[base_List, generators_List, degree_Integer] := 
			DecodeStrongGenSet[$llSchreierSims[base, generators, degree]];
		xAct`xPerm`Private`MLOrbit[point_Integer, generators_List, degree_Integer] := 
			$llOrbit[point, generators, degree];
		xAct`xPerm`Private`MLSetStabilizer[points_List, degree_Integer, base_List, generators_List] := 
			DecodeStrongGenSet[$llSetStabilizer[points, degree, base, generators]];
		xAct`xPerm`Private`MLBaseChange[degree_Integer, base_List, generators_List, newBase_List] := 
			DecodeStrongGenSet[$llBaseChange[degree, base, generators, newBase]];
		xAct`xPerm`Private`MLBaseChangeStabilizerChain[degree_Integer, base_List, generators_List, newBase_List] := 
			DecodeStabilizerChain[$llBaseChangeStabilizerChain[degree, base, generators, newBase]];
		xAct`xPerm`Private`MLStabilizerSGS[degree_Integer, base_List, generators_List, points_List] := 
			DecodeStrongGenSet[$llStabilizerSGS[degree, base, generators, points]];
		
		(* xTensor uses this flag to choose the native xPerm path, even when xPerm's
			
   original WSTP executable could not connect. Restore it on unload. 
	*)
		xAct`xPerm`$xpermQ = True;
        CheckAbort[InstallBackendMethods[method],
            UnloadxPermLibraryLink[]; Abort[]];
		True
	];

Clear[xPermLibraryLinkLoadedQ];

xPermLibraryLinkLoadedQ[] :=
	Length[$loadedFunctions] == 8;

Clear[UnloadxPermLibraryLink];

UnloadxPermLibraryLink[] :=
	Module[{},
        RestoreBackendMethods[];
		Scan[Quiet[LibraryFunctionUnload[#]]&, $loadedFunctions];
		$loadedFunctions = {};
		$library = None;
		If[Length[$originalDefinitions] === Length[$wrappedSymbols],
			MapThread[RestoreDefinition, {$wrappedSymbols, $originalDefinitions
				}]
		];
		$originalDefinitions = {};
		If[ListQ[$originalXPermQ],
			OwnValues[xAct`xPerm`$xpermQ] = $originalXPermQ
		];
		$originalXPermQ = None;
		True
	];

(* These files share this private context; loading them does not activate hooks. *)
Get[FileNameJoin[{$packageDirectory, "TensorReduce.wl"}]];
Get[FileNameJoin[{$packageDirectory, "BackendMethods.wl"}]];

End[];

EndPackage[];
