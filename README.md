## Imprimis
the destroyable realtime 3d game engine

Imprimis is a fork of Tesseract, which is a fork of the Cube 2: Sauerbraten engine. The goal of the Imprimis
engine is to provide a clean, documented and consistent base with which to build games, particularly
first person shooter games, upon.

The engine is completely free and the engine code, game code, and assets are all licensed
under permissive licenses.

Like Tesseract, Imprimis removes the static lightmapping system of Sauerbraten and replaces
it with completely dynamic lighting system based on deferred shading and shadowmapping.

The Imprimis engine, however, focuses on providing universally destroyable
geometry by leveraging the octree geometry system to conduct geometry modification on
the fly, enabling gameplay mechanics not readily available to widely available engines
such as realtime world construction and destruction.

#### Key Features

Imprimis' Tesseract base provides a bunch of new rendering features such as:

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
* runs on OpenGL 4.0+ contexts
* support for Windows and Linux operating systems

For documentation on the engine, see `doc/engine.md`.

#### Quick Install Instructions

To get the game, `git` is required. Using `git`, get the repository and its sumodules with

`git clone https://github.com/project-imprimis/imprimis.git --recurse-submodules`

The `imprimis` folder will now be visible in the current directory.

To compile the game, use `make -C src install -jN` from the directory in which this file is located.
Set N to the number of threads to compile with. For example, for a quad-core processor, set -j4.

(to reach this directory use `cd imprimis`)

This game requires `libsdl2`, `libsdl2-image`, `libsdl2-mixer`, and drivers for OpenGL (usually already installed).
To compile the game, the development versions of the libraries are required (on distros that seperate standard and dev packages).

The game can then be run with `./imprimis_unix` or `./imprimis.bat` scripts, located in the same
directory as this readme file.
