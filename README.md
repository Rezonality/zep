[Zep](https://github.com/Rezonality/zep) - A Mini Editor
<!-- ALL-CONTRIBUTORS-BADGE:START - Do not remove or modify this section -->
[![All Contributors](https://img.shields.io/badge/all_contributors-7-orange.svg?style=flat-square)](#contributors-)
<!-- ALL-CONTRIBUTORS-BADGE:END -->
[![Builds](https://github.com/Rezonality/zep/workflows/Builds/badge.svg)](https://github.com/Rezonality/zep/actions?query=workflow%3ABuilds)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Resonality/zep/blob/master/LICENSE)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/b14633031dfe49498719ad58ff96328a)](https://www.codacy.com/gh/Rezonality/zep/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Rezonality/zep&amp;utm_campaign=Badge_Grade)
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/b14633031dfe49498719ad58ff96328a)](https://www.codacy.com/gh/Rezonality/zep/dashboard?utm_source=github.com&utm_medium=referral&utm_content=Rezonality/zep&utm_campaign=Badge_Coverage)
[![codecov](https://codecov.io/gh/Rezonality/zep/branch/master/graph/badge.svg?token=sKdLmDPcW7)](https://codecov.io/gh/Rezonality/zep)
[![Gitter](https://badges.gitter.im/Rezonality/Zep.svg)](https://gitter.im/Rezonality/Zep?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

Zep is a simple embeddable editor, with a rendering agnostic design and optional Vim mode.  It is built as a shared modern-cmake library, using a single header include, or as a static library.  The core library is dependency-free (the demo application requires an installed package), and it is possible just to copy the files into your project and build it.  Out of the box Zep can draw to a Qt Widget or an ImGui window - useful for embedding in a game engine.  A simple syntax highlighting engine is provided, and can easily be extended. Basic theming support is included, and window tabs and vertical/horizontal splits are also available.  Zep is 'opinionated' in how it does things, but is easy to modify and supports many common features.  It is heavily influenced by Vim, but has a good notepad-style editing mode too.  A simple search feature (Ctrl+P) is a powerful way to find things, and a Repl mode is useful for implementing a console for game scripting.  Intended to eventually sit inside a live-coding environment, Zep also has a minimal mode and several configuration options which can be set in a simple toml-format file.

For an example of building Zep into a simple ImGui application, see the example project, which shows how to add Zep as a submodule and build it as a library or just include a single header file: [Integrating Zep](https://github.com/cmaughan/zep_imgui)

## Video Overview
[![Zep Overview](screenshots/video.png)](https://youtu.be/T_Kn9VzD3RE "Zep Overview")

## Screenshot
![ImGui](screenshots/sample.png)

Zep supports the standard editing keystrokes you'll find in most editors, along with a reasonable subset of modal Vim editing as an option.  The demo project lets you switch between the editing modes on the fly.  Zep is not meant to replace Vim.  I don't have a lifetime spare to write that, but it has most of the functionality I use day to day, and anything missing gets added over time.  A key-mapper enables configuration of Zep outside the standard modes offered.

Zep is ideally suited to embedding in a game engine, as an in-game editor, or anywhere you need a simple editor without a massive dependency on something more substantial like NeoVim.  The core library is dependency free, small, and requires only a modern C++ compiler.  Zep can be included in your project building a dependency-free modern cmake library, and setting `Zep::Zep` in `target_link_libraries`, or as a single-header include. A header-only implementation of the ImGui and Qt backends is provided as an addendum to the core library; this enables Zep to render in an ImGui or Qt environment.  After building and installing Zep on your system, only 2 lines are required in your CMakeLists to use it.  An alternative would be to just copy and build the source files for the core library into your project.

The demos for Qt and ImGui require dditional packages, but these aren't required to embed Zep in your application.  The ImGui demo builds and runs on Windows, Linux and Mac OS.  If you are a Vim user, you might often suffer the frustration of not being able to use Vim keystrokes in your tools.  Zep solves that.

Key Features:
* Modal 'vim' or modeless 'standard' editing styles; built around a common core of functionality.
* Keymapper for extending existing modes or adding new commands
* Qt or ImGui rendering (and extensible)
* Terminal-style text wrapping and work in progress non-wrapped mode
* Splits and tabs
* A simple syntax highlighting engine with pluggable secondary highlighters
* Theme support
* A Repl for integrating a command/scripting language (the demo project integrates a Janet Lang interpreter - :ZRepl to get into it, try (+ 2 2))
* CTRL+P search for quick searching files with fuzzy matching
* Text Markers for highlighting errors, etc.
* No dependencies, cross platform, small library, single header, static library or installed library option.
* Builds on VC 2017, GCC 6, Clang. C++14 is the basic requirement

New Features, recently added:
* Support for tabs instead of spaces; display of tabs as whitespace arrows.
* UTF8 (Work in progress - not completely done yet).
* More generic keymapping for extensible modes
* /search support

Current Limitations:
* Vim mode is limited to common operations, not the extensive set of commands typical in Neovim/Vim.  There are now a considerable number of commands, but notably ex commands are missing, such as %s///g for find/replace.

Though I have limited time to work on Zep, I do try to move it forward at York Developer's regular Code and Coffee sessions. Zep was my 2018 project but has already proved quite popular, and I try to throw more features in when I can.  There are over 200 unit tests for the editing modes.  This project started mainly as an experiment and a learning exercise.  I like the idea of a programmer building programmer tools for their own use, just as carpenters used to build their toolbox.

One of my targets for Zep is to get it to the point where I can use it as a standalone editor for common tasks.  It is almost equivalent to how I'd use NeoVim day-to-day.  The other target is to use Zep in a live coding environment.

Pull requests are appreciated and encouraged ;)

Screenshots
-----------
Live Coding in 'Minimal' Mode:
![LiveCoding](screenshots/livecode.png)

Using the ImGui Renderer:
![ImGui](screenshots/sample.png)

Using the Qt Renderer:
![Qt](screenshots/sample-qt.png)

A light theme:
![Qt](screenshots/sample-light-qt.png)

Embedded in a Live Coding tool:
![Embedded](screenshots/jorvik.png)

Embedded in a Game Engine:
![Embedded](screenshots/embedded.png)

# Design
## Layers
Zep is built from simple interacting layers for simplicity.

### Text
The text layer manages manipulation of text in a single buffer.  At the bottom level, a gap buffer structure maintains the text information.
The buffer layer is responsible for saving and loading text, and supporting simple search and navigation within the text.  Much of the higher
level mode code uses the buffer commands to move around inside the text.  A GlyphIterator is used within the buffer in order to walk along it in UTF8 code-points.

A command layer supplies functions to add and remove text, and supports undo; all buffer modifications are done with these simple commands.

The Mode layer supports editing text using Vim commands, or using standard notepad-like commands.

A Syntax layer monitors the buffer and provides file-specific syntax coloring. Syntax highlighting can be easily extended

### Display
Tab windows are like workspaces, each containing a set of windows arranged in splits.  The window lass arranges the rendering and calls a thin
display layer to draw the text.  This makes it simple to draw the editor using different rendering code.  Adding Qt took just an hour to do.

### Vim & Standard Modes
Mode plugins provide the editing facility - currently that is Vim & Standard and modes for the Repl and the Search panels.
The Vim mode has most of the usual word motions, visual mode, etc.  The standard mode has the usual shift, select, cut/copy/paste, etc.
See [Vim Mode](https://github.com/Rezonality/zep/wiki/Vim-Mode), or the top of the mode_vim.cpp file for a list of supported operations in Vim

# Building
After every change, Zep is built on Mac, Linux and PC.  You can see the script which does this in https://github.com/Rezonality/zep/.github/workflows/builds.yml.  You can choose to build just a library using modern CMake, or the full demo, or just include the library in a subfolder without building it at all.

Here is an example of building the demo (and hence the library and tests).  The build without Qt is easier, and it is recommended as a starting point, unless you are familiar with Qt and have it already installed.
## Add extra packages
Below is the minimum; your system may indicate it needs extra packages.
### Linux
```
sudo apt install cmake
sudo apt install git
```
### Mac
```
brew install cmake
brew install git
```
## Qt
Qt is only required if you want that version of the demo (there is little difference between the functionality of the Qt and ImGui demos - they are just different ways to achieve the same thing); it is better to start without it if you are just evaluating Zep, since it is just one more complication and installation...
Here is an example of installing Qt on Linux
```bash
# for Qt/Demo support
sudo apt install qt5-default
# Adapt to your installation path - you will need to set this appropriately
set QT_INSTALL_LOCATION="/usr/include/x86_64-linux-gnu/qt5"
```
On Windows you would typically install the Qt library and then set an environment variable to point to it:
```bat
set QT_INSTALL_LOCATION=C:\Qt\5.10.0\msvc2017_64
```
## Buildling the demo
Start by getting the source and running the prebuild.  Ensure the prebuild runs successfully and if not, make sure you have any dependencies on your system that it asks for.  The demo cannot build without the prebuild components.
```
git clone https://github.com/Rezonality/zep zep
cd zep
git submodule update --init
./prebuild.sh or prebuild.bat (select based on your system)
``` 
Change to the build directory:
```
mkdir build
cd build
```
Create the makefiles and build the code:
### Linux/Mac
This will make standard makefiles on linux and Mac.  There may be other options for the generator, such as XCode, but this will work as a starting point:
```
cmake -G "Unix Makefiles" -DBUILD_QT=OFF -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```
### Windows
Assuming you have Visual Studio 17, this will make a solution for you.  Note there is a convenient config_imgui.bat which will do the same thing.
```
cmake -G "Visual Studio 17 2022" -DBUILD_QT=OFF -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

On Windows you will find the built demo in:
```
.\build\demos\demo_imgui\Debug\ZepDemo.exe
```

## Tests
Type `CTest --verbose` in the build folder to run unit tests.

# Integration
If you want to use the Zep library in your own software you have 2 options:
### Option 1: Install the Zep library as a package
Here is a typical build instruction for windows, which just builds the library
```
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -DBUILD_IMGUI=0 -DBUILD_TESTS=0 -DBUILD_DEMOS=0 ..
cmake --build . --target install
```
At this point your system will have installed the zep library.  You can add its paths and library to your project like this, in the standard CMake way.
```
find_package(Zep REQUIRED)
target_link_libraries(MYPROJECT PRIVATE Zep::Zep)
```
### Option 2: Use zep as a single header library
A typical example of including Zep as a single header library (see the sister integration project for an example).  This is a really easy way to get started...
```
git submodule add zep https://github.com/Rezonality/zep

In CMakeLists:
target_include_directories(myapp
    PRIVATE
    zep/include

#include "zep\zep.h"
```
# Libraries used
[SDL2: Media/Window Layer](https://www.libsdl.org/download-2.0.php)
SDL2 is used by the demo to get a window on the screen in a cross platform way, and for OpenGL to generate a Context.

[ImGui: 2D GUI](https://github.com/ocornut/imgui)
ImGui is a great 2D User interface for 3D applications, used in the ImGui version of the demo

[Qt](https://www.qt.io)
Qt is a cross platform GUI.  Zep uses it as a renderer in the Qt Demo.

## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/jacobfriedman"><img src="https://avatars.githubusercontent.com/u/697310?v=4?s=100" width="100px;" alt="Jacob Friedman"/><br /><sub><b>Jacob Friedman</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=jacobfriedman" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/FredrikAleksander"><img src="https://avatars.githubusercontent.com/u/14351056?v=4?s=100" width="100px;" alt="FredrikAleksander"/><br /><sub><b>FredrikAleksander</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=FredrikAleksander" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="http://sam.hocevar.net/"><img src="https://avatars.githubusercontent.com/u/245089?v=4?s=100" width="100px;" alt="Sam Hocevar"/><br /><sub><b>Sam Hocevar</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=samhocevar" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="http://glenfraser.com"><img src="https://avatars.githubusercontent.com/u/306082?v=4?s=100" width="100px;" alt="Glen Fraser"/><br /><sub><b>Glen Fraser</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=totalgee" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://mastodon.world/@nicolas_noble"><img src="https://avatars.githubusercontent.com/u/7281574?v=4?s=100" width="100px;" alt="Nicolas Noble"/><br /><sub><b>Nicolas Noble</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=nicolasnoble" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://schnappy.xyz"><img src="https://avatars.githubusercontent.com/u/1405255?v=4?s=100" width="100px;" alt="Schnappy"/><br /><sub><b>Schnappy</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=ABelliqueux" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/fezjo"><img src="https://avatars.githubusercontent.com/u/26350212?v=4?s=100" width="100px;" alt="Jozef Číž"/><br /><sub><b>Jozef Číž</b></sub></a><br /><a href="https://github.com/Rezonality/zep/commits?author=fezjo" title="Code">💻</a></td>
    </tr>
  </tbody>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind welcome! See [Contributing](CONTRIBUTING.md) for details.
