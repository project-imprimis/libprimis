# The Imprimis Engine Specification

#### Note that this is a draft and subject to expansion and modification.

## Chapters

#### 1. Standards
* 1.1 Coding standards
* 1.2 Default Paths & Libraries

#### 2. World
* 2.1 Octree
* 2.2 Actors
* 2.3 Physics
* 2.4 Static entities
* 2.5 Projectiles

#### 3. Render
* 3.1 Texturing and Shading
* 3.2 Transparency
* 3.3 Postprocessing

#### 4. Netcode
* 4.1 Server
* 4.2 Client
* 4.3 AI

#### 5. User and System Interfaces
* 5.1 Menus
* 5.2 Hudgun
* 5.3 File I/O & Assets
* 5.4 Console

#### 6. Game Implementation
* 6.1 Weapons
* 6.2 Game Variables
* 6.3 Modes

# 1. Standards

## 1.1 Coding Standards

### 1.1.1 This File

This file is written to be interpreted by GitHub Flavored Markdown (GFM) and
must satisfy the standards laid out therein.

Text should institute a line break after 80 characters.

Chapters use `#`; sections use `##`; subsections use `###`.

### 1.1.2 C/C++ Standards

Four spaces per indentation, spaces only.
Opening brackets get their own new line.

## 1.2 Default Paths & Libraries

### 1.2.1 Paths

Linux: `~/.imprimis` is the "home" directory by default.
Windows: `~/My Games/Imprimis` is the "home" directory by default.

### 1.2.2 Libraries

This game requires `libsdl2, libsdl2-mixer, libsdl2-image` to run, including the
`-dev` versions for those package managers which elect to seperate them if
compiling the game. As the main way of getting the game is via Git, the best way
to retrieve the assets for the game is by the command line utility `git`.


# 2. World

## 2.1 The Octree

### 2.1.1 Octree Data Structure & Cube Geometry

Imprimis stores its data in an octal tree, in which each cube of edge length *l*
is divided into eight cubes with edge length *l*/2. This allows for a level to 
be efficiently and recursively divided into smaller and smaller pieces. The
power of this data structure is that large, faraway objects can occupy 
relatively fewer nodes in the data tree than objects in the level have.

The `gridpower` of a particular octree node (henceforth called simply a "cube")
is an indication of what level it is on the octal tree and therefore also its
size. A gridpower 0 cube is 12.5 cm on edge (~5 inches), a gridpower 1 cube is
25cm (~10 inches) on edge, and a gridpower *n* cube is 2^(*n*-3) m on edge.

Level sizes are also defined most conveniently in terms of gridpower; the
default map is 2^10 = 2^7m = 128m on edge. Due to limitations of the renderer's
z-buffer precision, distances beyond about half a kilometer on edge are not
generally recommended.

The recursive nature of octree nodes are most useful for the simplicity of
occlusion queries. Occlusion queries are made significantly faster by being able
to discard members of the tree near the root and therefore skipping all the
lower members of the tree which necessarily are within their parent's node.

Cubes in Imprimis, the most basic form of geometry in the game, therefore occupy
the octree; instead of vertices in other engines being determined by their 3D
vector from the origin, a cube's place in the octal tree determines its
location. 

### 2.1.2 Cube Manipulation

While octree subdivision allows for the inclusion of small pieces of geometry,
this is not on its own adequate due to the fact that octree nodes are, well,
cubes. To allow for maps which have shapes that are not all boxes, Imprimis,
like other games in the Cube family, allows for limited, discrete deformation of
octree nodes.

Each corner, of which there are eight on a cube, can be deformed in steps of
1/8th of the total cube size. This allows for decent approximations of many
curves when done carefully, and using different gridpowers prudently can allow
for some limited compound curvature.

This deformation, when carried out by all four corners of a cube, can allow for
faces which do not align with their boundaries; indeed, all six faces can be
deformed in this way to yield a deformed cube that does not touch its neighbors.
The "integrity" of the octree node, however, remains intact, and no other
geometry can occupy the cube which has been deformed. This set of limitations
can be summarized with the following statement:

* Every octree node is defined to have eight vertices and twelve faces.

Therefore, the only way to increase detail in a given area when using cube
geometry is to increase the octree node density (by using a smaller gridpower).

### 2.1.3 Remipping and Subdivision

#### Subdivision

The engine automatically attempts to subdivide cubes when a user attempts to
place a cube within a node which is of a larger gridpower. This means that many
types of cube deformation are no longer possible, causing erratic and generally
poor approximations of the prior form. It is recommended to take care when
placing new cubes in proximity to existing geometry of a larger gridpower, as
this can inadvertently cause unseemly changes to the level's geometry.

#### Remipping

The engine, when given a `remip` command, attempts to merge nodes with redundant
vertices together such that the renderer has less faces to deal with when later
rendering the scene. This reduces the map size and improves performance.

The amount of remipping intensity is defined by the `maxmerge` command, which
determines the maximum gridpower that can be simplified. Having this value too
high causes large surfaces to occlude poorly, as the entire face has to be
textured.

### 2.1.4 Materials

There are several materials in Imprimis which are capable of modifying their
volume's properties. Materials in general are combinable (though there are many
exceptions) so that multiple effects on the geometry can be combined. Because
materials do not have the same deformation ability as geometry, materials
are restricted to occupying rectangular shapes and cannot approximate the forms
that geometry can.

The most visible apparent materials are the water and lava materials. Water and
lava materials (of which there are eight, four of each type) create rectangular
volumes of water or lava in their selected regions. Water in particular has
features like screenspace reflection/refraction and caustics applied to it in
the renderer; lava, while not as impressive, is an animated flat texture.

Lava is one of the cases wherein materials are required to be combined; lava and
death material are always combined such that contacting lava is instant death.

Nearly as apparent is the array of glass materials, which create environment
mapping windows relatively cheaply. Using the four types of windows, it is
possible to make a variety of transparent geometry with different shapes;
additionally glass is capable of screenspace refraction to simulate the
ray optics of nonflat glass.

A more versatile but slower type of transparency is alpha material, which flags
cube geometry as transparent. The texture's properties determine the effects of
marking geometry alpha as opposed to the variables of the material itself; alpha
merely tells the engine what areas to treat specially as transparent.

The remaining three types of materials have no visual effect and instead only
act on actors and objects in the level. Death material kills all who enter its
volume; clipping keeps players out while letting particles through, and noclip
keeps geometry from hampering the travel of projectiles and players.



