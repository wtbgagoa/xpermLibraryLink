(* Paths arrive as environment data, never as interpolated Wolfram code. *)
source = Environment["XPERM_PACLET_SOURCE"];
destination = Environment["XPERM_PACLET_OUTPUT_DIRECTORY"];
operation = Environment["XPERM_PACLET_OPERATION"];
targetSystemID = Environment["XPERM_WOLFRAM_SYSTEM_ID"];

If[!StringQ[source] || !DirectoryQ[source] || !StringQ[destination] ||
    !MemberQ[{"paclet", "install-paclet"}, operation],
    Print["Invalid paclet build paths or operation."]; Exit[1]
];
If[operation === "install-paclet" && targetSystemID =!= $SystemID,
    Print["Cannot install a paclet built for ", targetSystemID,
        " into the ", $SystemID, " kernel. Use the paclet target to create a distributable archive."];
    Exit[1]
];

archive = Check[CreatePacletArchive[source, destination], $Failed];
If[!StringQ[archive] || !FileExistsQ[archive],
    Print["Failed to create the xPermLibraryLink paclet archive."]; Exit[1]
];
Print["Created paclet: ", archive];

If[operation === "install-paclet",
    installed = Check[PacletInstall[archive, ForceVersionInstall -> True], $Failed];
    If[!MatchQ[installed, _PacletObject],
        Print["Failed to install xPermLibraryLink."]; Exit[1]
    ];
    Print["Installed xPermLibraryLink in: ", installed["Location"]]
];
Exit[0];
