VSNvim
======

[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/neovim/VSNvim?branch=master&svg=true)](https://ci.appveyor.com/project/neovim/VSNvim/branch/master)

[Neovim](https://github.com/neovim/neovim) extension for Visual Studio 2017

Status
------

This extension is still in the early stages of development. Some
Neovim features have not been fully integrated into Visual Studio
including:
 - Highlights
 - Completion menus for insert mode and command line mode
 - Output from command line mode
 - Opening buffers
 - Line numbers and signs in the margin
 - Status lines
 - Window size and layout

Pre-releases can be downloaded from the AppVeyor build artifacts.

Build
-----

1. Install Visual Studio 2017 with the extension development workload.
1. Clone and build the Neovim fork for VSNvim.
	```
	git clone -b vsnvim https://github.com/b-r-o-c-k/neovim.git
	```
1. Change the `NvimSrcDir`, `NvimDepsDir`, and `NvimBuildDir` properties in
   the `VSNvim\VSNvim.vcxproj` to the correct paths of the fork.
1. Open `VSNvim.sln`, restore NuGet packages, and build.
