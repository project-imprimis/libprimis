Imprimis is a fork of Tesseract, which is a fork of the Cube 2: Sauerbraten engine. The goal of the Imprimis
engine is to provide a clean, documented and consistent base with which to build first person shooter games on.

No more long calclight pauses... just plop down the light, move it, change its
color, or do whatever else with it. It all happens in real-time now.

Like Tesseract, Imprimis removes the static lightmapping system of Sauerbraten and replaces
it with completely dynamic lighting system based on deferred shading and
shadowmapping.

Imprimis' Tesseract base provides a bunch of new rendering features such as:

* deferred shading
* omnidirectional point lights using cubemap shadowmaps
* perspective projection spotlight shadowmaps
* orthographic projection sunlight using cascaded shadowmaps
* HDR rendering with tonemapping and bloom
* real-time diffuse global illumination for sunlight (radiance hints)
* volumetric lighting
* screen-space ambient occlusion
* screen-space reflections and refractions for water and glass (use as many water planes as you want now!)
* screen-space refractive alpha cubes
* deferred MSAA, subpixel morphological anti-aliasing (SMAA 1x, T2x, S2x, and 4x), FXAA, and temporal AA
* runs on both OpenGL Core (3.0+) and legacy (2.0+) contexts

For documentation on the engine, see `doc/engine.md`.

To get the game, `git` is required. Using `git`, get the repository and its sumodules with

`git clone https://github.com/project-imprimis/imprimis.git --recurse-submodules`

The `imprimis` folder will now be visible in the current directory.

To compile the game, use `make -C src install -jN` from the directory in which this file is located.
Set N to the number of threads to compile with. For example, for a quad-core processor, set -j4.

(to reach this directory use `cd imprimis`)

This game requires `libsdl2`, `libsdl2-image`, `libsdl2-mixer`, and drivers for OpenGL (usually already installed).
To compile the game, the development versions of the libraries are required (on distros that seperate standard and dev packages).
