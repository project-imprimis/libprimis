## Imprimis
*the destroyable realtime 3d game engine*

Imprimis is a fork of Tesseract, which is a fork of the Cube 2: Sauerbraten engine. The goal of the Imprimis
engine is to provide a clean, documented and consistent base with which to build games, particularly
first person shooter games, upon. **It is currently under development and does not necessarily
currently accomplish all the goals it has established here.**

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

#### Imprimis' Niche

Imprimis is primarily a first-person shooter engine, though it can plausibly be
used for other 3D cases. Its recursive octree subdivision geometry method and
realtime rendering method is a very powerful generalized way of implementing realtime
general (destroy *any* part of the world and have it be modified procedurally) world
destruction. As a result, it is especially strong at:

* Realtime, generalized, procedural geometry modification
* Realtime modification of world lighting
* Good scalability to medium-sized maps (up to ~1km*1km)
* Performance on moderate-performance hardware such as modern integrated graphics
* Small, managable codebase
* Straightforward, integrated level editor

The Imprimis engine, however, may not be as suitable for other roles commonly targeted
by other available engines:

* Distance culled, very large world rendering
* Large amounts of complex geometry detail or compound curvature
* Largely static levels without much geometry modification
* Low performance or legacy graphics support

In general, Imprimis is a relatively specialized engine which is useful largely for
a class of game mechanics not accessible to other available engines. In the goals
which Imprimis specifically targets (realtime geometry modification), there are
not many available solutions that can replicate Imprimis' utility.

#### Quick Install Instructions

To get the game, `git` is required. Using `git`, get the repository and its sumodules with

`git clone https://github.com/project-imprimis/imprimis.git --recurse-submodules`

The `imprimis` folder will now be visible in the current directory.

To compile the game, use `make -C src -jN` from the directory in which this file is located.
Set N to the number of threads to compile with. For example, for a quad-core processor, set -j4.

(to reach this directory use `cd imprimis`)

This game requires `libsdl2`, `libsdl2-image`, `libsdl2-mixer`, and drivers for OpenGL (usually already installed).
To compile the game, the development versions of the libraries are required
(on distros that seperate standard and dev packages).

The game can then be run with `./imprimis_unix` or `./imprimis.bat` scripts, located in the same
directory as this readme file.

Unlike other Cube based games, this client does not include a dedicated server.
For this functionality, see the imprimis-gameserver repository.

#### Join Us

Imprimis is an open source project created by volunteers who work on the game as
a hobby, and we'd love for it to be your hobby too! The Imprimis project tries
to be well documented and transparent in its decision making so as to make
outside participation fruitful. If you'd like to express your opinions on the
engine's decision, modify the engine, participate on the engine code, or just say
hello to the developers, that's great! We have a Discord server where you may
interact with us at https://discord.gg/WVFjtzA.

To facilitate getting started working on Imprimis, there are several issues posted
on the "issues" board. Whether you're a longtime open source contributor or you
need to create a GitHub account to start participating, feel free to use issues
labeled as "good first issue" to ask whatever questions you have about Git semantics
or quirks about our specific codebase in order to get comfortable!
