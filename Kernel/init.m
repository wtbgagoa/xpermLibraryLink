(* Load xPerm before reading the implementation that replaces its private hooks.
   A missing WSTP executable may emit a message while the package still loads. *)
If[!MemberQ[$Packages, "xAct`xPerm`"], Needs["xAct`xPerm`"]];

If[MemberQ[$Packages, "xAct`xPerm`"],
  Get[FileNameJoin[{DirectoryName[$InputFileName], "Implementation.wl"}]],
  xAct`xPermLibraryLink`LoadxPermLibraryLink::xperm =
    "The required package `1` could not be loaded. Install xAct where Needs can find it.";
  Message[xAct`xPermLibraryLink`LoadxPermLibraryLink::xperm, "xAct`xPerm`"];
  $Failed
]
