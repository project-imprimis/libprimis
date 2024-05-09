# Libprimis

[![windows](https://github.com/project-imprimis/libprimis/actions/workflows/msbuild.yml/badge.svg)](https://github.com/project-imprimis/libprimis/actions/workflows/msbuild.yml)
[![ubuntu](https://github.com/project-imprimis/libprimis/actions/workflows/makefile.yml/badge.svg)](https://github.com/project-imprimis/libprimis/actions/workflows/makefile.yml)
[![ubuntu](https://github.com/project-imprimis/libprimis/actions/workflows/clang.yml/badge.svg)](https://github.com/project-imprimis/libprimis/actions/workflows/clang.yml)
[![ubuntu](https://project-imprimis.github.io/libprimis/lcov/coverage.svg)](https://project-imprimis.github.io/libprimis/lcov/)

## Documentation

  - [API Documentation](https://project-imprimis.github.io/libprimis/)
  - [Test Coverage](https://project-imprimis.github.io/libprimis/lcov/)
  - [Tesseract Renderer Overview](doc/tesseract-renderer.md)

## An Open Source Engine Library

Libprimis is a 3D game engine, based on Tesseract and the Cube 2 family of programs.
Unlike the Cube/Cube 2/Tesseract games, which featured tightly integrated rendering
and game code, however, Libprimis is an engine without accompanying game code,
providing developers the freedom to develop games without being forced into the
same design paradigms of the original Cube games.

Libprimis' world uses octree subdivision to recursively subdivide the world into
an efficient, sparsely populated tree of cubes. This representation provides numerous
advantages over "polygon soup" type vertex representations, especially for performance
in the critical occlusion and physics calculations required for many applications.

With many modern features, including realtime deferred shading, volumetric lighting, and
tone mapping support, Libprimis' core is fast, capable, and modern, and fully open sourced.
All this combines to make an engine that allows for an unprecedented ability to manipulate
a vibrant and dynamic world using simple, accessible semantics.

## Key Features

Libprimis' Tesseract base provides a bunch of rendering features such as:

* deferred shading
* omnidirectional point lights using cubemap shadowmaps
* perspective projection spotlight shadowmaps
* orthographic projection sunlight using cascaded shadowmaps
* HDR rendering with tonemapping and bloom
* real-time diffuse global illumination for sunlight (radiance hints)
* volumetric lighting
* screen-space ambient occlusion
* screen-space reflections and refractions for water and glass
* screen-space refractive alpha cubes
* deferred MSAA, subpixel morphological anti-aliasing (SMAA 1x, T2x, S2x, and 4x), FXAA, and temporal AA
* support for OpenGL 4.0+ contexts
* support for Windows and Linux-based operating systems
* support for realtime geometry modification during gameplay
* Unicode UTF-8 text support
* GLTF, MD5, and OBJ model support

For documentation on the engine, see [engine](doc/engine.md).

## Quick Windows Install Instructions

To get the source code, use your prefered `git` client (git for Windows, Visual Studio, gitkraken, etc.).
Be sure to get the submodules as well.

The headers required to build the library are located in `libprimis-headers`, one of the
submodules.

The library has compilation semantics for MSVC/Visual Studio. Opening the Visual Studio project
located in `src/vcpp` will allow you to build the project. The created library will be located
in the `bin64/` folder.

To build a game on libprimis, you will need to get the required headers (located in a separate
repository, `libprimis-headers`) and build your game against the compiled library and the headers.

## Quick Linux Install Instructions

To get the source code, `git` is required. Using `git`, get the repository and its submodules with

`git clone https://github.com/project-imprimis/libprimis.git --recurse-submodules`

The `libprimis` folder will now be visible in the current directory.

To compile the library, use `make -C src -jN` from the directory in which this file is located.
Set N to the number of threads to compile with. For example, for a quad-core processor, set -j4.

(to reach this directory use `cd libprimis`)

This library requires `libsdl2`, `libsdl2-image`, `libsdl2-mixer`, `libsdl2-ttf`, `libglew`,
and drivers for OpenGL (usually already installed). To compile the library, the development
versions of the libraries are required (on distros that seperate standard and dev packages).

Once the library has been compiled, it should be placed the standard shared library folder
(usually `/usr/lib/` or `/usr/local/lib`) where it can be linked to. Alternatively, use
`make -Csrc install` to automatically compile and install the library to `/usr/lib/`. Distros
without `sudo` or which do not have their `ld` library path at `/usr/lib` can configure the
Makefile to point to the appropriate location or copy the file manually to their `ld` library path.

To build a game on libprimis, you will then need to get the required headers (located in
a separate repository) and build your game against these headers and the shared library.

## Join Us

Libprimis is an open source project created by volunteers who work on the game as
a hobby, and we'd love for it to be your hobby too! The Libprimis project tries
to be well documented and transparent in its decision making so as to make
outside participation fruitful. If you'd like to express your opinions on the
engine's decision, modify the engine, participate on the engine code, or just say
hello to the developers, that's great! We have a Discord server where you may
interact with us at https://discord.gg/WVFjtzA.

To facilitate getting started working on Libprimis, there are several issues posted
on the "issues" board. Whether you're a longtime open source contributor or you
need to create a GitHub account to start participating, feel free to use issues
labeled as "good first issue" to ask whatever questions you have about Git semantics
or quirks about our specific codebase in order to get comfortable!
