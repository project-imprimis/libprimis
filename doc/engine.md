# The Imprimis Engine

#### This is a work in progress and subject to modification and additions.

Written and © Alex "molexted" Foster, 2020; released under the WTFPL v2.

Preface
---

This text is an expository one, describing the *what* and some of the *why*
things are implemented as they are in the engine. This document does not attempt
to explain how the game is implemented.

Note that this text is written by someone who did not write the actual code for
the engine: as a result, it is possible that some of this document is
misconstrued from the original intent of the code writers. As those people are
not particularly available, this text represents the best guess of the author.

This text is an attempt to describe the engine in such a way that previous
Cube-based engines were not. For nearly two decades, the Cube series of engines,
including the original Cube as well as Cube 2 and Tesseract, have been among the
most technologically advanced first-person shooter engines available to the open
source community, but their adoption has been limited by the relative inability
to understand the source code due to a lack of comprehensive documentation and
often opaque coding styles. This text is an attempt to lay down the architecture
of the engine, and provide the skeleton required to understand specific code
that is written in the engine.

To fully understand this text, it is expected that readers understand the basics
of functional and object-oriented programming paradigms, as well as have a basic
knowledge of linear and vector algebra; these mathematical concepts are the core
of how 3D engines like Imprimis' are designed. A few rendering techniques, such
as global illumination, additionally use more complex mathematics borrowed from
linear analysis, including multipole expansions and Fourier series.

## Chapters

#### 1. Standards
* 1.1 Coding Standards
* 1.2 Default Paths & Libraries
* 1.3 Conventions and Units
* 1.4 Program Structure

#### 2. World
* 2.1 Octree
* 2.2 Materials
* 2.3 Textures
* 2.4 Global Properties

#### 3. Entities
* 3.1 Static Entities
* 3.2 Projectiles
* 3.3 Bouncers
* 3.4 Stains
* 3.5 Particles
* 3.6 Physics

#### 4. Sound
* 4.1 Sound Interface
* 4.2 HUD Sounds
* 4.3 World Sounds

#### 5. Render
* 5.1 Texturing
* 5.2 Lighting
* 5.3 Transparency
* 5.4 Screenspace Postfx
* 5.5 Antialiasing

#### 6. Actors
* 6.1 Actor Objects
* 6.2 Actor Rendering
* 6.3 AI

#### 7. Netcode
* 7.1 Topology
* 7.2 Server
* 7.3 Client
* 7.4 Master Server

#### 8. User and System Interfaces
* 8.1 Menus
* 8.2 Hudgun
* 8.3 File I/O & Assets
* 8.4 Console

#### 9. Game Implementation
* 9.1 Weapons
* 9.2 Game Variables
* 9.3 Modes

#### 10. Internal Objects
* 10.1 Vector Objects

# 1. Standards
---

## 1.1 Coding Standards
---

### 1.1.1 This File
---

This file is written to be interpreted by GitHub Flavored Markdown (GFM) and
must satisfy the standards laid out therein.

Text should institute a line break after 80 characters.

Chapters use `#`; sections use `##`; subsections use `###`.

### 1.1.2 C/C++ Standards
---

Four spaces per indentation, spaces only.
Opening brackets get their own new line; this is called "Allman style".

#### Symbol Names

Macros are always fully capitalized and seperated with underscores:

`#define MACRO_NAME`
`#define MACRO_FUNCTION(a, b)`

Variables are always lowercase:

`int varname`

Functions are always lowercase:

`void functionname()`

Enum elements are in PascalCase:

```
enum
{
    Name_ElementOne   = 1,
    Name_ElementTwo   = 2,
    Name_ElementThree = 3,
};
```

#### Enums

Enums also are always expanded with a single element per line. For aesthetics,
it is best to place all the equals in the same location (as above). Imprimis
uses no named enums besides those inhereted from ENet.

#### `for` loops

The accepted conventional variables for loops are the single letter `i`,`j`,`k,
`l` temp vars. Use later loop variables only if the earlier ones are already
present (don't use a loop over `j` unless it's inside a loop over `i`).

The convention for incrementing loops (counting upwards) is as follows:

```
for(int i = 0; i < N; ++i)
```
Change the value passed to `N` rather than using a less than or equals sign `<=`
so as to keep all for() statements clear as to their termination point
immediately.

For decrementing loops (counting downwards):

```
for(int i = N; --i >= 0;)
```

For loops over the length of a vector, use

```
for(int j = 0; j < N.length(); j++) //forward iteration
for(int v = m; --v >= 0;) //reverse iteration

```
#### Indentation and Bracing

The codebase uses the Allman style; that is, statements are enclosed in brackets
on newlines. Case statements are indented one tab past their opening switch
statement.

```
namespace MyNamespace
{
    int var1, var2, var3;

    int var4 = foo(myfunction()),
        var5 = bar(foo()),
        var6 = foo(foo() || foo(bar) || foo(baz));

    enum
    {
        Name_ElementOne   = 1,
        Name_ElementTwo   = 2,
        Name_ElementThree = 3,
    };

    struct baz
    {
        int var1;
        char var2;
        union
        {
            char var3[8];
            int var4;
        };
    }

    if(foo())
    {
        int a;
        int b;
        doStuff;
        doMoreStuff;
    }
    else
    {
        doStuff;
    }

    do
    {
        doStuff;
    } while(bar)

    while(foo)
    {
        doStuff;
    }
//=====================================================================MACRONAME
#define MACRO_NAME(a,b) stuff(a,b)
    int myfunction(int a, int b = 0)
    {
        for(int i; i < N; ++N)
        {
            if(bar)
            {
                switch(n)
                {
                    case 1:
                    {
                        doStuff;
                    }
                    case 2:
                    {
                        doStuff;
                    }
                    case 3:
                    case 4:
                    case N:
                    {
                        doStuff;
                    }
                }
            }
        }
        MACRO_NAME(a,b)
        return a;
    }
}
#undef MACRONAME
//==============================================================================
```

Control flow statements (if/while/do-while etc.) should get their own line;
don't do stuff like

```
if(foo) for(int i; i < N; ++i) { doStuff; doMoreStuff }
```

In addition, always delimit statements after control expressions with curly
braces, even if there is only one expression:

```
//do this
if(foo)
{
    doStuff;
}

//not this
if(foo)
    doStuff;
```

#### Spacing

Ternaries and boolean operators should be spaced out between each element:

```
foo ? bar : baz
foo >= bar
foo || bar
```

Arithmetic can be done without spaces, however:

```
foo+1
3*bar
```

Operators should remain packed against their parentheses:

```
if(foo)
{
    stuff
}

while(bar)
{
    stuff
}
```

## 1.2 Default Paths & Libraries
---

### 1.2.1 Paths
---

Linux: `~/.imprimis` is the "home" directory by default.
Windows: `~/My Games/Imprimis` is the "home" directory by default.

### 1.2.2 Libraries
---

This game requires `libsdl2, libsdl2-mixer, libsdl2-image` to run, including the
`-dev` versions for those package managers which elect to seperate them if
compiling the game. As the main way of getting the game is via Git, the best way
to retrieve the assets for the game is by the command line utility `git`.

### 1.3 Conventions and Units
---

#### 1.3.1 Distances
---

Distance is always in the unit of cube units ("cubits"), which is the size of a
gridpower 0 cube, when not specified. This distance is equal to an eighth of a
meter, 12.5 centimeters, or approximately five inches (to within a couple %).

#### 1.3.2 Coordinates
---

Octree nodes, in particular, are always positive and the origin is located at
the bottom northwest corner of the map; the coordinates count upwards as you
move more southerly or easterly from that point.

The engine uses a **left-handed** coordinate system, which is the opposite
chirality from typical 3D coordinate systems used in math and engineering. This
means that the positive orientation is clockwise, and cross products follow the
left hand rule. This makes the game's coordinates follow the compass orientation
well and is perhaps more intuitive for those used to maps where degrees count
clockwise, but be aware that the usual identities for right handed systems are
mirrored in this coordinate system.

Zero yaw degrees in the engine's coordinate system is facing along the +y axis;
the +x axis is at 270 degrees (or to the left side of the y axis, hence the name
of the coordinate system); pitch is measured as an altitude where 0 degrees is
the horizon, the +z axis is at 90 degrees, and the -z axis is at -90 degrees.

Position coordinates for valid geometry is always positive, as all cubes must be
placed NE of the origin, located in the lower SE corner of the map. This should
somewhat simplify coordinate calculations, but is mostly there because the root
octree node is most easily represented with a corner at (0,0,0) rather than
trying to be centered. As a result, larger maps will have their greater extent
at larger coordinates; a smaller map will occupy the SE corner of a larger map.

```
   +z                   +z
    |   +x               |
    |  /                 |
    | /                  |______+y
    |/_____+y           /
                       /
                      /
                    +x
  LH Coordinates    RH Coordinates
    Clockwise      Counterclockwise
```

#### 1.3.3 Mathematical Notes
----

As a 3D engine is largely defined in terms of vectors and linear algebra, some
understanding of these concepts is very helpful in understanding positions and
transformations in the engine as well as the rendering machinery that renders
images to the screen.

A vector is a set of multiple scalars (standard numbers) which together
represent a multidimensional location in some space. This space does not have to
be positional, as concepts like colors are usually also represented as a vector
(components are `R`ed, `G`reen, and `B`lue). Most vectors in Imprimis are three
dimensional, but second dimensional, fourth dimensional, complex (quaternions)
and dual (dualquats) are also represented.

A particular notational standard which will always be implicit is the difference
between *speed* and *velocity*. Velocity is a vector, which means it carries a
direction along with its size. On the other hand, speed is merely the size of
the velocity vector, and does not imply a particular direction.

The complex objects, quaternions and dual quaternions, are not strictly vectors,
but exist in a higher dimensional space than standard scalars in a similar way.
The structure of the spaces in which these values live is particularly
convenient for the constrained values that kinematics problems have, and as such
are particularly useful in animation and rotation transformations.

Quaternions have four values, designated x, y, z, w; dual quaternions have
similarly their eight values seperated into dual numbers which are each
designated x, y, z, w.

#### 1.3.4 Colors
---

Colors which are defined past 0xFFFFFF (hex color for white) are generally
passed as a set of three paramaters `R G B` where `1.0 1.0 1.0` is 0xFFFFFF.
These colors tend to have `1.0 1.0 1.0` as the default and are expected to vary
upwards as much as downwards in practice.

## 1.4 Program Structure
---

The Imprimis project is, at its highest level, organized into four main projects
as well as a pair of utilities which are perhaps not considered direct members
of the engine.

```
    Serverside    .                     Clientside
------------------+-------------------------------------------------------------
                  .
+--------+  (A)   .
| Master |_____   .
| Server |     \  .  +--------+   +--------+   +------+
+--------+      \-.->|        |   |        |   |      |
    ^             .  |        |   |        |   |      |(F)+--------------+
    | +--------+  .  |        |   |        |   |      |-->| Window Output|
    \_|  Game  |<-.->|        |   |        |   |      |   +--------------+
    | | Server |  .  |        |   |        |   |Simple|   +--------------+
    | +--------+  .  |  Game  |(D)|  Game  |(E)|Direct|-->| Sound Output |
 (B)| +--------+ (C) |  Code  |<->| Engine |<->|Media |   +--------------+
    \_|  Game  |<-.->|        |   |        |   |Layer |
    | | Server |  .  |        |   |        |   |(SDL) |   +--------------+
    | +--------+  .  |        |   |        |   |      |<--|  User Input  |
    | +--------+  .  |        |   |        |   |      |   +--------------+
    \_|  Game  |<-.->|        |   |        |   |      |
      | Server |  .  +--------+   +--------+   +------+
      +--------+  .      ^
                  .      |(G)
                  .      v
                  .  +--------+
                  .  | Local  |
                  .  | Server |
                  .  +--------+
```

* A: Master server provides a list of game servers to the game code via enet.
* B: Game servers register to a master server via enet.
* C: Game servers can be connected to a client's game via enet.
* D: The game engine's behavior is controlled by the game code.
* E: The Simple DirectMedia layer library handles input/output.
* F: Display, sound, and user inputs get handled by SDL.
* G: Local gameplay can be run through a game server hosted clientside.

The components of the system handled by the Imprimis project is the:

* Game engine: core routines needed for the gamecode to run a game
* Game code: an actual game written using the facilities written in the engine
* Game server: a locally or remotely hosted server that manages game clients
* Master server: a service that provides a list of game server names to clients

# 2. World
---

The *world* is the name for the level that the game is played on, including the
octree geometry, materials, and cloud/skyboxes. The world does include the
static entities as well which are placed upon it, but those are instead
described in the third chapter, which covers entities and actors in more detail.

The world's biggest feature is the octal tree that makes up the primary level
geometry. This geometry has the advantage of being easily occludable and simple
to modify on the fly, as opposed to models which are placed on the world as
static entities.

## 2.1 Octree Geometry
---

The Imprimis engine's geometry system is very different than most engines and
has different strengths and weaknesses with respect to typical polygon soup
engines such as Unreal or the Quake family of engines. Imprimis' octal tree
geometry does not record map vertices in terms of typical positon vectors, as
the vast majority of 3D rendering software uses, instead opting to use an octal
tree format.

### 2.1.1 Octree Data Structure & Cube Geometry
---

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

Cubes in Imprimis, the most basic form of geometry in the game, therefore occupy
the octree; instead of vertices in other engines being determined by their 3D
vector from the origin, a cube's place in the octal tree determines its
location.

### 2.1.2 Child Nodes
---

As mentioned above, each node can have *children* defined for them which
themselves can have their own child nodes. The maximum depth of this tree is
equal to the `mapsize`: a map of size 2^10 = 1024 cubes (128m square) can have
child nodes up to 10 deep.

```
               ^ +z
               |
             __________
            /  4 /  5 /
           /____/____/.
          /  6 /  7 / .
         /____/____/  .
        .    _____.___.
        .   /  0 /. 1 /     +x
        .  /____/_.__/    ->
        . /  2 / 3. /
        ./____/___./


            / +y
           |/
```

The assignment of child nodes is outlined above. The node with the lowest x,y,z
coordinates is assigned as child node #0, and counts upwards to node 7, which is
at the largest x,y,z coordinates. A child node might be found in this way within
the engine:

```
level 0 128m                   worldroot
                 __________________|_________________
                 |    |    |    |    |    |    |    |
level 1 64m      0    1    2    3    4    5    6    7
                             __________________|_________________
                             |    |    |    |    |    |    |    |
level 2 32m                  0    1    2    3    4    5    6    7
                                    __________________|_________________
                                    |    |    |    |    |    |    |    |
level 3 16m                         0    1    2    3    4    5    6    7
                       __________________|_________________
                       |    |    |    |    |    |    |    |
level 4 8m             0    1    2    3    4    5    6    7
                                                     ^
                                                     | (6,5,1,6)
                                                This is an 8*8*8m cube node.
```

The "gridpower" of a cube is related to the distance down the tree that a cube
is, and therefore its size as a power of two. The bottom of the tree is always
at gridpower 0, and is located eleven rungs down (0,1,2,3,4,5,6,7,8,9,10) the
world octree on a standard power 10 map (128m on edge).

In this view, the ease of discarding smaller nodes is apparent: the cube
selected (7,5,1,6) could be discarded easily if it was found that cube (7,5) was
occluded, since its "address" includes all the larger cubes that occupy its
volume. This is very fast compared to a pile of vectors which need to be
individually treated in order to ensure they can be excluded from the render
process.

### 2.1.3 World Root
---

The worldroot, indicated in the octal tree diagram above at the top, is the
master cube inside which all geometry fits. Geometry may not leave the area
bounded by the worldroot cube, as all geometry is carved out of the worldroot's
children nodes (all cubes in a level are child cubes of the worldroot cube).

Because of this, maps always have the following properties:

* Maps are always square. Cube nodes are all sqaure, and thus the largest cube
node is as well.
* Maps are of fixed size. The size of the worldroot defines the map's size.
* Maps' range of gridpowers allowed is determined by the size of the worldroot
cube.

#### Map Expansion

There is a command, `mapenlarge`, that can grow a map such that the worldroot
takes up a larger volume. The existing map, accordingly, is placed as child 0 of
the new world root, leading to the map expansition occupying space in the +x,
+y, and +z direction from the location of the existing map. This may be slightly
inconvenient for those seeking to expand the scenery bounds of their level
uniformly; a copy and paste of the geometry is needed to re-center the old level
if desired.

### 2.1.4 Cube Manipulation
---

While octree subdivision allows for the inclusion of small pieces of geometry,
this is not on its own adequate due to the fact that octree nodes are, well,
cubes. To allow for maps which have shapes that are not all boxes, Imprimis,
like other games in the Cube family, allows for limited, discrete deformation of
octree nodes.

Each corner, of which there are eight on a cube, can be deformed along all three
directions in steps of 1/8th of the total cube size. This allows for decent
approximations of many curves when done carefully, and using different
gridpowers prudently can allow for some limited compound curvature.

This deformation, when carried out by all four corners of a cube, can allow for
faces which do not align with their boundaries; indeed, all six faces can be
deformed in this way to yield a deformed cube that does not touch its neighbors.
The "integrity" of the octree node, however, remains intact, and no other
geometry can occupy the cube which has been deformed. This set of limitations
can be summarized with the following statement:

* Every octree node is defined to have eight vertices and twelve faces.
* Each vertex can only be found at one of 512 discrete points within the node.
* Textures always get an entire face of an octree node.
* Textures are projected from the node normal, not the deformed surface normal.
* Cubes' deformed shapes may not extend past their original undeformed volume.

Therefore, the only way to increase detail in a given area when using cube
geometry is to increase the octree node density (by using a smaller gridpower).

For more information on texture projection, see §2.3.3.

### 2.1.5 Remipping and Subdivision
---

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

#### Commands

`remip`: performs a remip calculation on the level
`maxmerge N`: sets the maximum merge gridpower to N

## 2.2 Materials
---

There are several materials in Imprimis which are capable of modifying their
volume's properties. Materials in general are combinable (though there are many
exceptions) so that multiple effects on the geometry can be combined. Because
materials do not have the same deformation ability as geometry, materials are
restricted to occupying rectangular shapes and cannot approximate the forms that
geometry can.

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

### 2.2.1 Air
---

Air, the name for the lack of materials, unsurprisingly is the default
"material" for the level. Air can be "placed" by `/editmat air` or `/air`
whereupon it removes all previously existing materials flagged for that
selection.

The name "air" does not imply that there is any oxygen mechanics in the game
and there is no material representing the lack of air.

### 2.2.2 Water
---

Water, the material with the largest change with respect to prior engines, has
four types that can be modified seperately to apply in different situations
on the map. As water is a particularly complex material to physically represent,
there are many visual effects provided in the engine which can be tuned for
particular situations.

Water is the only place in which screenspace reflection is used in game;
water materials also have options for caustics, reflection, fog, and
environment mapping to complement this. The sides of water material have
different properties (such as lacking screenspace reflection) and are controlled
by a differnt set of variables (the `water<N>fall` set).

Commands which apply to the four water types seperately are designated
`water<N>` where N is the water material being edited. There are four water
materials, and the first one does *not* get appended with a "1" like 2/3/4 all
do.

#### Commands

* `causticscale`
* `causticmillis`
* `causticcontrast`
* `causticoffset`
* `caustics`
* `water<N>color`
* `water<N>deepcolor`
* `water<N>deepfade`
* `water<N>refractcolor`
* `water<N>fog`
* `water<N>deep`
* `water<N>spec`
* `water<N>refract`
* `water<N>fallcolor`
* `water<N>fallrefractcolor`
* `water<N>fallspec`
* `water<N>fallrefract`
* `waterreflect`
* `waterreflectstep`
* `waterlod`
* `watersubdiv`

### 2.2.3 Glass
---

Another one of the four-variant materials, and the second most interesting
(after water) with respect to engine features, glass is a cheap and effective
way to create transparent geometry. As opposed to alpha material, glass is
restricted to rectangular volumes (as with all materials) and therefore is
somewhat less flexible; additionally glass material is not capable of backface
transparency like glass is. However, because glass material does not require
rerendering of parts of the level like alpha does.

Because glass material is expected to be used largely in vertical situations
where screenspace reflection is a poor choice, it does not have screenspace
reflection support and instead relies entirey on environment mapping and
specular mapping for its reflective appearence. This does mean that glass has to
be careful about how it is placed such that it falls within the radius of an
environment map entity. Like water and alpha materials, however, glass is
capable of screenspace refraction, useful for nonflat materials.

In itself, glass material does not block movement, but in practice it always
does because clipping material is mandatory for all types of glass. The engine
will automatically place clip wherever glass is placed.

#### Commands

* `glass<N>color`
* `glass<N>spec`
* `glass<N>refract`

### 2.2.4 Lava
---

The third and final four-variant material, lava offers an animated and solid
liquid surface that kills all who enter it. This is actually not a property of
lavamat itself but the required death material that it always comes with.

Lava traces its origins back to the Quake style maps which prominently featured
it as part of the aesthetic, but is troublesome to use in a modern engine like
Tesseract as a result of its demand for diffuse lighting. Lava, furthermore,
does not support many of the interesting reflective features that water and
glass possess (though, to be fair, lava largely does not need advanced
reflections); it is therefore advisable to use water in its place if practical.

#### Commands

* `lava<N>color`
* `lava<N>fog`
* `lava<N>glowmin`
* `lava<N>glowmax`
* `lava<N>spec`

### 2.2.5 Clip
---

An entirely transparent material, clip, unlike the materials prior, does not
affect the rendering of the scene in any way. Clip, instead, impedes the ability
for actors (players, bots) to enter their volumes; it allows for the map to be
securely blocked off from leaving even without geometry placed. Clip material
is also recommended for cases where the existing geometry has troublesome
collision, such as trellis or crosshatched geometry. Clip is also always placed
wherever glass is, to make it impossible to phase through.

Clip, however, notably does *not* impede the progress of particles or
projectiles, which allows it to be used for map boundaries without fear of
random projectiles bouncing or dying on collision with its bounds.
If it is required that projectiles be deleted upon contact, using death material
in tandem with clip is a viable solution.

### 2.2.6 Noclip
---

The opposite material to clip (unsurprisingly), noclip instead permits the
passage of actors through otherwise impenetrable geometry. Noclip additionally
allows projectiles through surfaces, making the enclosed volume get treated
essentially as air instead of having whatever collision the geometry within has.

As a result, noclip is of no effect for volumes not containing geometry; there
must be geometry within the noclip's volume for it to take effect.

Using noclip effectively and not having immersion-breaking visibility problems
generally limits its use to relatively small applications, like flattening out a
floor or smoothing a wall out so that it is not obstructing travel. When placing
noclip, it is advisable to check it by spawning into the level and making sure
that there is no unseemly visibility issues (where you can see through the wall)
that would break immersion.

### 2.2.7 Death
---

Death material forces the suicide of those players who enter its bounds,
instantly killing them. Death material is automatically applied wherever lava
is and is also usable by itself or with other materials like water.

Death material also destroys particles and projectiles which enter its midst,
which is potentially useful for culling unnecessary particles that enters its
region.

The bottom of the map (z < 0) also acts as death material and players who leave
the bottom of the map automatically are killed. No such effect takes place on
the sides or top of the map volume.

### 2.2.8 No GI
---

No GI material flags its volume as not being lit by global illumination. This
does not have a material impact (or improvement) in performance but may be
useful to combat artifacts in the low-resolution global illumination algorithm.

Note that those regions beyond the radiance hints far plane (`rhfarplane`) will
be lit regardless of their No GI status. Keep this in mind whenever placing
long-distance radiance hints.

### 2.2.9 Alpha
---

Alpha material is the more versatile but slower companion to glass for the
creation of transparency. Alpha material draws cube geometry within its bounds
as transparent, allowing for transparent shapes in forms other than rectangular
boxes.

Alpha material has its material properties dependent upon the flagged settings
of the geometry contained therein; using `valpha` allows for changing a
texture's transparent-ness. The material on its own has no flags or other
related commands to modify its behavior.

Alpha material does not support continuous variable opacity, and the opacity
is always constant for a given texture. While this is certainly a limitation,
typical transparent objects like windows generally have constant opacity across
their full area.

## 2.3 Textures
---

The faces of cubes within the game can be given textures on a cube face by cube
face basis, allowing for immersive, complete scenes to be generated via cube
geometry.

Textures are applied to the six faces of the cube with a simple planar
projection; as a result, there is distortion when cubes themselves are
distorted. This can be allieviated with the more expensive `triplanar` shader,
but that is beyond the scope of this section.

Each cube has a texture defined for each of its six faces; this means that
"buried" geometry will after revision cause the storage of meaningless texture
information for invisible geometry. For this reason, there is a command
`fixinsidefaces` which can set all invisible faces to the default texture.

#### Commands

* `fixinsidefaces [vslot]` Sets all invisible faces to the vslot given.

### 2.3.1 Texture Slots
---

Textures are registered to a file that accompanies the map, generally named
the same as the map file and with the `.cfg` extension. Texture slots are for
unique shader combinations (for a description of the shaders see the section on
texture rendering) and as a result at least one physical slot is required for
each distinct texture in the game. A texture slot declares the following:

* Shaders used in the texture
* Modifications to shader paramaters
* Relevant texture maps

An example of a typical texture declaration is shown below.

```
setshader bumpenvspecmapglowworld
setshaderparam envscale  0.7 0.7 0.7
   texture 0 "nieb/complex/light01_c.png"
   texture n "nieb/complex/light01_n.png"
   texture s "nieb/complex/light01_s.png"
   texture g "nieb/complex/light01_g.png"
```

This shader is set to `bumpenvspecmapglowworld`, meaning it uses bump (normal),
environment mapping, specular mapping, and glow mapping. The `world` on the end
is to declare it as a cube geometry shader as opposed to a decal. Afterwards,
the four relevant maps are provided for the different shaders that are to be
applied with the `texture` command;

* 0/c declares a diffuse map
* n declares a normal map
* s declares a specular map
* z declares a height (parallax) map
* g declares a glow map

These definitions of textures are largely set beforehand and then called with
`texload` upon running the map's config (automatically run at map load); most
textures have only a couple of possible appropriate shader combinations anyways.

### 2.3.2 Virtual Slots
---

Virtual slots encode simple manipulation of textures, such as coloration, scale,
rotation, and orientation. These do not require declaration upon map generation
and are generally created dynamically ingame after the execution of a `v`
command. Virtual slots (vslots) then save only the parent texture index and the
modifications done to it; they save significant memory space by not requiring
a modified copy to be stored in video memory.

The modifications that a vslot can store are described below in the V-command
section.

### 2.3.3 Texture Projection
---

The standard scaling of textures is such that there are 512 linear pixels per
gridpower 5 cube, leading to a density of 512/32 = 16 pixels per power 0 cube
or 512/4 = 128 pixels per meter. This is somewhat low but largely sufficient
for generic areas which the player does not find themselves particularly close
to, but may be insufficient for areas the player is near to; for this, the
V-command `vscale` is very useful.

Textures which are not square are projected faithfully and there is no
stretching of the shorter axis; this means that trim textures can be made
skinny and narrow if desired to save space.

Textures are projected onto the parent node normals and not the deformed surface
normals, which causes distortion of the texture when it is heavily distorted.
This effect additionally causes attempted blending of faces (like when trying to
make rounded organic geometry) to have an unseemly seam along these boundaries,
as these boundaries delimit different texture projection differences.

This problem can be solved with the `triplanar` shader, which forces textures to
be projected in three different directions such that the true normal caused by
the distorted cube can be found accurately.

### 2.3.4 Texture Slot Properties
---

The V-commands are a set of texture modification commands that allow for
textures to be flipped, rotated, scaled, tinted, and offset as necessary.
These commands, when run while in the editor, create new vslots with the
modified behavior.

The tex-commands are the corresponding commands for standard texture slots and
have the same effect; these are declared for physical texture slots as opposed
to being dynamically assigned to virtual slots (vslots).

Other than their means of assigning texture behavior to slots, however, the two
commands are otherwise identical in their behavior. V-commands are the ones used
ingame; tex-commands are generally placed in texture definitions.

#### `texalpha <front> <back>`, `valpha <front> <back>`: transparency modifiers

`alpha` sets the amount of transparency to render the texture if it is within
the volume of placed alpha material. The property has no effect otherwise, and
so modifying this should only be done while the texture is inside alpha material
such that the effects of changing `alpha` are apparent.

Textures are set to `0.5 0` alpha by default, meaning they are halfway opaque.
Notably, the second parameter is zeroed out, such that the backface alpha
feature of the engine is not enabled. Backface alpha is the closest thing the
deferred renderer of Tesseract (deferred renderers in general have trouble
dealing with alpha in expedient ways) to multiple layers of alpha, though it
is a somewhat limited approach which does not lend itself to wide applicability.

Backface alpha allows the far side of the geometry to also be rendered along
with the standard front face that is rendered by default. Backface alpha
requires another geometry pass in the renderer and *is* measurably slower than
leaving it off, but also is the only way to simulate glass-behind-glass in
levels.

#### `texangle <index>`, `vangle <index>` : fine texture rotation

`angle` rotates the texture by a given angle; capable of rotating textures
by arbitrary amounts through 360 degrees. If needed, this can be combined with
`rotate` which works by a different mechanism (and is the only way to get
flipped/transposed textures).

#### `texcolor <R> <G> <B>`, `vcolor <R> <G> <B>`: texture tinting

`color` changes the color of the texture evenly through the values of the three
parameters passed to it. `color 1 1 1` is the identity operator (has no effect)
and values above and below this will change the colors accordingly. As the
combination of red, green, and blue is the standard basis for additive colors
(like on a monitor), any color can be created by using the three channels
appropriately.

#### `texoffset <x> <y>`, `voffset <x> <y>`: translational texture offset

`offset` offsets a texture by a given number of pixels; this means that higher
resolution textures need larger offsets, and that for standard textures,
fractional offsets are in powers of 2 (a 1024x texture needs to be offset by
512 to be shifted by half a texture).

#### `texrefract <scale> <R> <G> <B>`, `vrefract <scale> <R> <G> <B>`: refract

`refract` modifies the refractive behaviors of materials that are within alpha
material. Refraction is the distortion of rays or light traveling through a
material due to the change in the speed in light at material boundaries, and is
handled in Imprimis via screenspace effects. The intensity of the refraction
is handled by the `scale` parameter and the color of the refraction is
controlled by the `R G B` parameters; `1 1 1` is the default white color.

#### `texrotate <index>` `vrotate <index>`: coarse texture rotations/transforms

`rotate` transforms a texture by the possible "simple" 2x2 matrix transforms,
of which there are seven.

* **1** rotate 90
* **2** rotate 180
* **3** rotate 270
* **4** flip x
* **5** flip y
* **6** transpose
* **7** flip and transpose

Note that flipping and transposing are the same regardless of whether the flip
is over the x or y axis.

These transforms are basic means of getting new orientations for situations
which do not require the more granular `vrotate` command and is the only way
to flip/transpose textures.

#### `texscale <scale>`, `vscale <scale>`: texture scaling

`scale` changes the size of the texture linearly along its axes. As a result,
a texture at `vscale 4` takes up four times the area as `vscale 2` while having
linear dimensions twice as great.

The identity setting for `scale` is `scale 1`; the default scale factor,
perhaps not surprisingly, is unity. The engine does low for scales smaller than
unity, which corresponds to downscaling the texture to increase its density.
The limits for scaling correspond to powers of two; the minimum is 2^-3 (1/8)
and the maximum is 2^3 (8). At these extremes, textures are either way
overdetailed (1024 pixels per meter) or way underdetailed (16 pixels per meter).

For most applications, it is recommended that the scaling be kept to powers of 2
such that the texture tiles in sync with the cube grid. Exceptions where other
scales may be appropriate include instances where 3/2 scaling is desired for a 3
cube wide area or organic textures which are not intended to noticibly tile.

#### `texscroll <x> <y>`, `vscroll <x> <y>`: time-varying translational offset

`scroll` causes the texture to take on a linear time-varying offset such that
the texture appears to move with respect to the surface it is applied to. The
scale for this scrolling effect is such that `scroll 1` is 1 texture per second;
this is usually too large for common scrolling objects (like banners or
conveyor belts) and as such fractional values here are most commonly employed.

## 2.4 Global Properties
---

The world in Imprimis has many global variables that affect the entire level
evenly. These include ambient lighting, fog, and skybox settings, as well as
more technical aspects such as mipping intensity. This section does not include
the global settings for individual materials, as is covered in §2.2.

### 2.4.1 Sunlight
---

Sunlight, the cheapest form of bulk lighting in the game, is a dynamic light
which casts shadows like any other light, but from a projection at infinity.
Sunlight therefore projects perfect quadrilaterals from rectangular objects
(as opposed to the trapezoids of point lights on the level).

Sunlight is also the only type of light that takes advantage of Imprimis' global
illumination capability, as enabling it for standard lights is too expensive.
Global illumination by sunlight is capable of providing ambient lighting to
partially lit rooms and is faster than using large numbers of on-level point
lights.

Sunlight has just four variables controlling its behavior, which set its size,
color, and location.

#### Commands

* `sunlight <color>` Sets the color of the sunlight, passed as a hex color.
* `sunlightpitch <angle>` Sets the sun's inclination angle above the horizon.
* `sunlightyaw <angle>` Sets the yaw angle (about z axis) of the sunlight.
* `sunlightscale <scale>` Sets the intensity scale of the sunlight.

### 2.4.2 Fog
---

*Note: This is entirely distinct from the *fogdome*, a sky property.*

Fog is an effect that fades objects to a particular color as the distance to
that object grows larger. Fog is useful for creating a closed, damp ambiance,
but be aware that it can be easily disabled on client machines, making its use
as a balance technique inadvisable in light of its lack of security with respect
to its implementation.

Fog also culls the rendering of entities once they pass into the realm of
complete obscurance, which is set by the `fogcullintensity` variable.

#### Commands

* `fog <dist>` The characteristic distance for the onset of fog effects.
* `fogcolor <color>` The color of the fog, as a hex color.
* `fogcullintensity <scale>` The intensity by which fog culls entity rendering.
* `fogintensity <scale>` The fog effect intensity (lower values -> more fog).
* `fogoverlay`

### 2.4.3 Ambient Lighting
---

The cheapest type of global lighting, changing the ambient light level for the
map makes everything at least as bring as the set ambient level. This reduces
contrast however and makes the shadows less deep, so generally using at least
global illumination combined with sunlight is preferable whenever possible to
high ambient settings. A little ambient, however, is advisible to make the scene
not pitch dark anywhere.

Ambient lighting does not require shading resources like sunlight or point
lights and so is not a performance issue like the other types of lighting can
be.

#### Commands

* `ambient` Hex color for ambient color, typically dimmer than `0x333333`
* `ambientscale` Multiplier for ambient color (usually left at 1)

### 2.4.4 Skybox
---

The skybox is a static rendering of a scene surrounding the map which provides
a backdrop to the level. The skybox is a cubemap, a type of environment
projection wherein the viewable area around a point (a full sphere) is seperated
and projected onto the six faces on a cube. The cubemap projection is
particularly convenient for its simplicity of projection for the engine and its
relatively low distortion (as opposed to a single-face projection like Mercator)
while remaining fairly simple to comprehend.

Skyboxes in Imprimis are passed as a set of six images:

* **bk**: the backside texture (normal facing south)
* **dn**: the bottom texture (normal facing upwards)
* **ft**: the front texture (normal facing north)
* **lf**: the left texture (normal facing west)
* **rt**: the right texture (normal facing right)
* **up**: the top texture (normal facing downwards)

Note that these are defined for cases in which the yaw of the skybox is 0; that
is to say that it it has not been rotationally translated at all. Rotating the
sky about the z-axis is possible and changes the orientation of the side faces
of the skybox accordingly.

These six textures are loaded whenever the skybox is set: setting `skybox
foo/bar` will automatically load `foo/bar_bk`, `foo/bar_dn`, etc. as the skybox.
Implicit in the path is the location of skyboxes in `/media/sky`.

#### Commands:

* `skybox <path>` Sets the path of the skybox, excluding the _XX and format.
* `skyboxcolor <color>` Tints the skybox to the given hex color.
* `skyboxoverbright <scale>` Controls how bright highlights in the skybox are.
* `skyboxoverbrightmin <scale>` Sets the overbrightness overall of the skybox.
* `skyboxoverbrightthreshhold <scale>` Sets min brightness for highlighting.
* `yawsky <angle>` Sets the overall orientation of the skybox in the world.
* `spinsky <angular vel>` Sets the rotation speed of the sky in deg/s.

### 2.4.5 Cloudbox
---

The cloudbox takes a standard skybox and renders it inside the standard skybox.
The cloudbox accepts all six standard faces that a skybox does (bk,dn,ft,lf,rt,
up) but only renders the top face and the top half of the sides (the bottom face
specified is not rendered). This means that for a cloudbox to be seamlessly
integrated with the skybox, it should have a smooth transition at the horizon;
this generally makes typical skyboxes inappropriate when pushed into the
cloudbox role with no tweaking.

The most useful way to use a cloudbox is, unsurprisingly, to simulate clouds.
This is best accomplished by giving the cloudbox an alpha setting such that the
skybox can be seen behind the cloudbox.

#### Commands:

* `cloudbox <path>` Sets the path of the cloudbox, excluding the _XX and format.
* `cloudboxcolor <color>` Tints the cloudbox to the given hex color.
* `cloudboxalpha <value>` Sets the opacity of the cloudbox (0..1, 1 for opaque).
* `yawclouds <angle>` Sets the overall orientation of the cloudbox in the world.
* `spinclouds <angular vel>` Sets the rotation speed of the sky in deg/s.

### 2.4.6 Cloud Layer
---

Additionally, the engine supports a single planar layer of clouds. The "height"
of this layer is adjustable, but there is no parallax regardless of height:
moving around on the map will not change the relative position of the cloud
layer. The cloud layer instead gains its apparent closeness from the rate at
which low-inclination clouds become apparently smaller: lower cloud layer
heights mean that the center is relatively larger compared to higher cloud
layer heights. Additionally, lower cloud layers appear to extend closer to the
horizon than higher ones.

Cloud layers are, unlike the cubemap projections that cloudboxes and skyboxes
use, able to scroll (have a translational movement with respect to time) in
addition to being able to spin about the z-axis. This allows for somewhat
realistic cloud movement when done in moderation.

#### Commands:

* `cloudalpha <value>` Sets the opacity of the cloudlayer (0..1, 1 for opaque).
* `cloudclip <value` Sets level of clipping passed to env box draw.
* `cloudcolor <color>` Tints the cloudbox to the given hex color.
* `cloudfade <value>` Sets the fade rate of the cloudbox.
* `cloudheight <value>` Sets the apparent height of the clouds in the sky.
* `cloudlayer <path>` Sets the path to the cloud layer, excluding extension.
* `cloudoffsetx <value>` Sets the x offset amount, in pixels.
* `cloudoffsety <value>` Sets the y offset amount, in pixels.
* `cloudscale <value>` Sets the scale factor of the clouds (1 by default).
* `cloudscrollx <value>` Sets the x scroll amount, in pixels/s.
* `cloudscrolly <value>` Sets the y scroll amount, in pixels/s.
* `cloudsubdiv <value>` Sets the number of edges the cloud perimeter has.
* `spincloudlayer <value>` Sets the spin rate of the clouds in the CW direction.
* `yawcloudlayer <value>` Sets the yaw angle of the cloud layer.

### 2.4.7 Atmo
---

Atmo is the way that the game can create a procedural skybox such that the sky
follows the sun's position and lights itself according to the sun's position.
The sun (which as noted in §2.4.1 is set by sunlightpitch/sunlightyaw)
determines the location of the atmo sun in the sky, making the atmo skybox
always line up with the sun's position and the shadows it throws.

The atmo layer can be blended with the skybox layer with `atmoalpha` and
as such can be blended with complex scenes while still being capable of dynamic
positioning whenever needed. Standard skyboxes with fixed light sources are
restricted to whatever pitch the light source is placed on the skybox (though
the skybox can be rotated about the z direction) and this limits the ability
of sunlight to properly light a scene.

Atmo takes many physical parameters which affect the simulated atmosphere that
is created. These include the opacity of the air, the size of the planet, the
apparent intensity of the light source, and the characteristic color of the sky.

#### Commands:

* `atmoalpha <value>` Sets the opacity of the atmo layer (0..1, 1 for opaque)
* `atmobright <value>` Sets the overall brightness of the atmo sky.
* `atmodensity <value>` Sets the diffusion amount of the atmo air.
* `atmohaze <value>` Sets the scale of haze at the horizon.
* `atmohazefade <value>` Sets the color that the horizon haze fades to.
* `atmohazefadescale <value>` Sets the haze transition scale.
* `atmoheight <value>` Sets the height of the horizon atmospheric effect.
* `atmoplanetsize <value>` Sets the hardness of the atmo transition.
* `atmosundiskbright <value>` Sets the brightness of the atmo sun.
* `atmosundisksize <value>` Sets the diameter of the atmo sun.
* `atmosunlight <value>` Sets the color of the atmo sun & overall sky color.
* `atmosunlightscale <value>` Sets the ratio of the sunlight brightness vs atmo.

### 2.4.8 Fogdome

The fogdome is a sky effect (rather than one that applies to the world like
regular fog does) and as such operates much like the `atmo` skybox. The fogdome,
however, is simpler, and only has one overall color with intensities and limits
set by the controlling variables below. The fogdome can be cut off at the bottom
if desired or allowed to smoothly close, and its centering location (the area
where the fogdome effect is strongest) can be chosen as well.

The fogdome works to create a homogeneous color effect that gradually blends
itself over the skybox or atmo layer, allowing possible matching with standard
world fog for a seamless appearance if done carefully.

#### Commands:

* `fogdomecap <boolean>` Toggles enclosement of the bottom of the fogdome.
* `fogdomeclip <value>` Sets the size at which to cut off the fogdome.
* `fogdomecolour <color>` Sets the hex color of the fogdome.
* `fogdomeheight <value>` Sets the height to which the fogdome is centered.
* `fogdomemax <value>` Sets the max opacity of the fogdome (0..1, 1 is opaque).
* `fogdomemin <value>` Sets the min opacity of the fogdome (0..1, 1 is opaque).

# 3. Entities
---

## 3.1 Static Entities
---

The static entities are world elements which get saved to the level and are
loaded on game start. These entities have the following types:

* light: a point light source
* mapmodel: a contained piece of geometry
* playerstart: a location where players spawn from
* particles: a location where the engine generates dynamic particles
* sound: a point sound source
* spotlight: a linkable modifier for light entities
* decal: texture application onto a geometry face
* teleport: a location that moves the player to a teledest
* teledest: the output location of a teleporter
* jumppad: object that pushes actors around
* flag: flag for capture-the-flag gameplay

Entities themselves are point objects which can be placed at any arbitrary
location within the level. Their effects generally extend to a radius beyond
just the point that the occupy, including defined radii for entities like lights
or sounds or simply having a defined structure as mapmodels do. Entities are
always themselves static on the level and do not move unless manipulated by an
editor; however they can have dynamic effects which may make them appear to
move.

Entities in Imprimis all have five attributes each, though not all attributes
are necessarily defined for a given model. The consistency of ent attributes is
designed to make user interface design as straightforward as possible such that
complex dynamic methods for menus are not necessary to edit entities.

There is no practical limit to the quantity of entities a level can have; for
many common types of entities, performance concerns make any physical limit
moot. There exists issues with very excessive numbers of lights overfilling the
light buffer or very excessive numbers of sounds causing sound issues, but these
are not a concern until the level is already unplayable.

### 3.1.1 Lights
---

Lights are entities where light appears to eminate from. Lights are point
entities and the light they cast is as from a perfect point source. Because
the engine is deferred and dynamically lit, light counts are one of the largest
influencers of performance, and the engine automatically occludes lights by a
tile-based algorithm to not render those lights which do not contribute to the
scene. Light entities' performance is highly related to its radius, and
therefore use of large light entities for bulk lighting is not recommended and
use of sunlight and global illumination is recommended in its place.

Light entities do not benefit from the enhancements that screenspace ambient
occlusion and global illumination have on the sunlight, as these features are
too expensive to enable on a light-by-light basis. For similar reasons, only
point lights are supported, as it is in fact *very difficult* to create lights
with configurations more complex than ideal point radiators.

As lights are one of the key cogs of the deferred renderer used in Imprimis,
a more technical discussion of their behavior with respect to the rendering
pipeline can be found in that section.

#### Attributes

Lights have five attributes, the last of which itself has a set of flags
which control the light's technical behavior.

#### 0: `radius`
The maximum distance the light entity can cast light; strongly related to
performance impact of the light and shadow map usage

The radius of the light is, as with other distances, defined in cubits.

#### 1: `red`

The intensity of the red channel of the light's output. Nominally, 255 is "full"
red, but this can be exceeded for an overbright light.

#### 2: `green`

The intensity of the green channel of the light's output. Nominally, 255 is
"full" green, but this can be exceeded for an overbright light.

#### 3: `blue`

The intensity of the blue channel of the light's output. Nominally, 255 is
"full" blue, but this can be exceeded for an overbright light.

#### 4: `flags`

Lights support four flags which can be combined to achieve particular effects.

* 1 `noshadow`: treats geometry/models as transparent, allowing light through
* 2 `static`: disables shadow map updates, causing static shadows
* 4 `volumetric`: simulates light reflection off of dust in the air
* 8 `nospec`: disables specular highlights

### 3.1.2 Mapmodels
---

Mapmodels are entities which represent a 3D model. While this object can be
animated (e.g. a fan or reciprocating device) it cannot undergo reactive or
scripted behavior and the entity itself remains at one point at all times.

Mapmodels are usually supplied in the md3 (Quake III), obj (Wavefront), or iqm
(Inter-Quake Model) formats; they additionally need one or more u,v maps (UV
maps) to define their surface normals, speculars, and specularity. Unlike level
geometry, mapmodels do not support parallax mapping.

Mapmodels in Imprimis have support for hitboxes which closely mirror that of the
physical model; however, mapmodels do not support decals and as a result weapons
hitting mapmodels do not leave bullet marks like ordinary geometry does.

#### Attributes

Mapmodels have controllable attributes for their size and orientation; the
particular mapmodel to be used is given by an index. Note that the possession of
only three degrees of freedom means that the model can become gimbal locked if
orientation values are chosen poorly.

#### 0: `index`

Selects the index of the passed models which are loaded into the map to display.
As usual, this index begins at 0 and counts upwards; the engine will simply
display nothing if there is no valid model at the index.

Mapmodels are generally defined in map configuration files and therefore the
specific model assigned to each index is not enforced game-wide.

#### 1: `yaw`

The yaw (azimuthal) angle of the model, in left-handed (clockwise) degrees.
Values larger than 360 can be passed but are identical to passing in their
modulus 360.

#### 2: `pitch`

The pitch (altitude) angle of the model, expressed as an inclination from the
horizon. Negative values can be used to pitch the model towards the -z axis.

#### 3: `roll`

The roll angle of the model, expressed as a right-handed rotation about the
axis set by the `pitch`/`roll` attributes.

#### 4: `scale`

The scale factor of the model. Scaling is always isotropic (no distortion) and
the identity point is at 100 (100 is "normal" scale) as opposed to 1 for many
other engine features; this is because the arguments passed to entities are
always integers (and obviously setting 1 as unity w/ only integral steps would
be not quite optimal).

### 3.1.3 Playerstarts
---

The playerstarts define where players respawn after they are killed.
Unsurprisingly, playerstarts have a team associated with them which determines
the group of players who are allowed to spawn there; team spawns only can spawn
the players belonging to that team.

Playerstarts have attributes which define the orientation of players who spawn
at them. The playerstart entity has only two relevant attributes; the last three
attributes have no effect on the behavior of the entity.

#### Attributes

#### 0: `team`

0 for FFA, 1/2 for teams blue and red respectively. Available spawns for any
arbitrary player are limited to playerstarts who share the same team index.

#### 1: `yaw`

The yaw (azimuthal) angle of the player when they spawn, in left-handed
(clockwise) degrees. Setting the yaw of the playerstart is important to prevent
players from spawning facing the wrong way, such as towards a wall.

### 3.1.4 Particles
---

The six types of implemented particles use their five attributes differently.
As a result, this section is has its last four attributes' descriptions
seperated into sections by the type of particle in use (which is set by
parameter 0 `type`).

Particles do not collide with players or are dynamically manipulated by physics;
they are, however, culled upon collision with cube geometry to minimize wasteful
rendering of particles which cannot be seen.

Particles are billboards, meaning that they are 2d objects which always have
their normal vector pointed at the camera. For this reason, particles'
orientation is determined by the camera, not the scene, and for objects that are
usually anisotropic (like fire, which only makes sense in one direction), this
approximation can yield some poor results. For this reason, maximum particle
size is generally kept fairly small to prevent the obviousness of billboarding.

Additionally, particles are client side effects, meaning that one person's view
of a particle is not representative of the effect rendered on other people's
machines.

#### Attributes

Easily the most complex entity with respect to its attributes, particles have
unique specifications for each value passed to its first attribute `type`. This
means that particles cannot have their `type` changed and have attributes
consistently transfer.

#### 0: `type`

The type of particle for the game to render. There are six types implemented:

* 0 fire
* 1 smoke
* 2 water
* 3 plasma
* 4 tape
* 5 status

Fire particles create a vertical plume of fire, with customizable footprint,
particle size/total height, and color. Fire particles also have nonoptional
smoke which appears at the top of the plume.

Smoke particles create a slightly directional gray cloud of smoke, with a
direction selectable along six directions (the three coordinate axis directions
and their negatives). Smoke cannot have its color changed, and is always a
moderate shade of gray.

Water particles create a small fountain effect which can, like smoke, have its
direction selectable from the aformentioned six directions. Water can have its
color changed to suit the liquid being represented, but the physics of the
particle movement are fixed except for their orientation.

Plasma particles are a large, brightly colored ball of gas which hovers about
the entity point. Unlike other particles, plasma does not spawn in multitudes
and then fade; it remains a hovering entity at all times, with its only
variation being a pulsing effect.

Tape particles are a raylike particle type that creates light beams along a
specified direction. While they are restricted to the same six directions as
water and smoke, there is a large variety of modes which they can occupy for
each direction, including plane, sphere, cone, circle, and ray configurations.

Status particles display a billboard displaying a bar which can be configured
to be filled between 1 and 100. Through scripting, it is possible to dynamically
change this value (by manually editing the entity attributes e.g.) to make it
dynamically display some desired gameplay aspect. Status particles can have
their bar color set, but the background and outline colors are fixed.

Each of these has different attributes 1-4 and obviously shows the particle type
aformentioned.

#### 1: `radius` (fire, plasma); `dir` (smoke, water, tape); `fill` (status)

For fire and plasma, the radius paramater controls how large the particle can
be. For fire, this is the areal size; the size that the "base" of the flame
occupies. This area is always a perfect square (meaning many entities are
required for a potential rectangular shape).

Likewise, for plasma, this parameter controls the radius of the sphere bounded
by the plasma effect.

For smoke, water, and tape, this parameter defines the orientation of the
particle. Only tape takes into account values above 5 or the directionality of
the particle; water and smoke are restricted to "ray" type configurations only.

The lowest valid index for each particle configuration is listed in the table
below:

* The value is the number put in the `type` attribute of the entity.
* The type is the geometrical shape of the configuration.
* The orientation describes the way the whole configuration points.
* The direction is the orientation of individual rays relative to the origin.

| Value | Type       | Orientation | Direction |
|-------|------------|-------------|-----------|
| 0     | Ray        | +z          | Away      |
| 1     | Ray        | +x          | Away      |
| 2     | Ray        | +y          | Away      |
| 3     | Ray        | -z          | Away      |
| 4     | Ray        | -x          | Away      |
| 5     | Ray        | -y          | Away      |
| 256   | Circle     | xy plane    | Away      |
| 257   | Circle     | yz plane    | Away      |
| 258   | Circle     | xz plane    | Away      |
| 259   | Ring       | +z          | Normal    |
| 260   | Ring       | +x          | Normal    |
| 261   | Ring       | +y          | Normal    |
| 262   | Cone       | +z          | Away      |
| 263   | Cone       | +x          | Away      |
| 264   | Cone       | +y          | Away      |
| 265   | Cone       | -z          | Away      |
| 266   | Cone       | -x          | Away      |
| 267   | Cone       | -y          | Away      |
| 268   | Plane      | +z          | Normal    |
| 269   | Plane      | +x          | Normal    |
| 270   | Plane      | +y          | Normal    |
| 271   | Line       | xz (+z)     | Away      |
| 272   | Line       | xy (+x)     | Away      |
| 273   | Line       | yz (+y)     | Away      |
| 274   | Line       | yz (+z)     | Away      |
| 275   | Line       | xz (+x)     | Away      |
| 276   | Line       | xy (+y)     | Away      |
| 277   | Sphere     | +z          | Away      |
| 278   | Sphere     | +x          | Away      |
| 279   | Sphere     | +y          | Away      |
| 288   | Circle     | xy plane    | Towards   |
| 289   | Circle     | yz plane    | Towards   |
| 290   | Circle     | xz plane    | Towards   |
| 291   | Ring       | -z          | Normal    |
| 292   | Ring       | -x          | Normal    |
| 293   | Ring       | -y          | Normal    |
| 294   | Cone       | +z          | Towards   |
| 295   | Cone       | +x          | Towards   |
| 296   | Cone       | +y          | Towards   |
| 297   | Cone       | -z          | Towards   |
| 298   | Cone       | -x          | Towards   |
| 299   | Cone       | -y          | Towards   |
| 300   | Plane      | -z          | Normal    |
| 301   | Plane      | -x          | Normal    |
| 302   | Plane      | -y          | Normal    |
| 303   | Line       | xz (+z)     | Towards   |
| 304   | Line       | xy (+x)     | Towards   |
| 305   | Line       | yz (+y)     | Towards   |
| 306   | Line       | yz (+z)     | Towards   |
| 307   | Line       | xz (+x)     | Towards   |
| 308   | Line       | xy (+y)     | Towards   |
| 309   | Sphere     | +z          | Towards   |
| 310   | Sphere     | +x          | Towards   |
| 311   | Sphere     | +y          | Towards   |

For a status particle, this parameter defines the particle's fullness, as a
range between 0 and 100. At 100, the bar is full; values above this have no
additional effect.

#### 2: `color` (water, plasma, status); `size` (tape, fire); `null` (smoke)

For water, plasma, and status entities, parameter 2 specifies the particle's
color. This is passed as a hexadecimal triple (`0x000`...`0xFFF`) which
specifies the color of the particle to four-bit precision. While four bits per
channel is indeed very low compared to common pictographic formats (typically
256 colors per channel), in practice particles (which are essentially
monochromatic) do not need a gigantic amount of distinct colors (there is
essentially no difference between 0xF00 and 0xE00 that matters when picking a
slab of color).

For tape particles, this parameter specifies the length of each tape particle.
This does not meaningfully affect the width of the tape particles, only its
length, and so the only way to reduce the apparent thickness is instead by its
`color` parameter.

For fire particles, this parameter defines the height and size of the fire
particles. Larger fire particles last proportionally longer (and this ratio is
fixed) and therefore rise to a greater height before fading.

This parameter has no effect on smoke particles and any value specified will be
ignored.

#### 3: `color` (tape, fire); `null` (smoke, plasma, water, status)

Tape has its color parameter on the third attribute, and it works in the same
hexadecimal triple form as the above explaination of color for the other
particle types. In the same way, the color of the fire particles' flames can be
set. The smoke that fire particles release cannot be modified.

None of the other entities take this attribute into account and setting a value
for any of them will be ignored.


#### 4: `null` (fire, plasma, smoke, status, water); `fade` (tape)

Only tape particles take this parameter into account; fade sets the time in
milliseconds for the particle to delete itself once it has been spawned. Tape
particles fade out gradually and do not get jarringly deleted at the specified
time. Note that the fade out rate is a fixed ratio of the total particle
lifetime and therefore quicker when the tape particle is set to die out
relatively quickly.

For all other particle types, this parameter can be set to any value but will be
ignored.

### 3.1.5 Sound
---

Sound entities place a static, looping sound effect at the point where the
entity is placed. Sound entities can only have their volume modulated globally
and by radius. Sound entities take a sound index which defines the entry in the
engine's mapsound list which is played; this is generally set in the map's
configuration file which is executed at map start. This sound entity is then
triggered upon a player entering its characteristic radius, playing the set
sound (locally) at them. Sounds are not ensured to sync perfectly across
different clients.

Sound playback from entities does take into account location of the entity and
therefore plays back the sound in stereo, with the sound intensity per channel
defined by the location of the entity.

#### Attributes

There are two attributes for the sound entity, the index of the sound and radius
at which it starts playing.

#### 0: `index`

The index of the sound entity indicates to the engine which of the set sounds
declared in the map configuration file is to be played by the entity. This index
counts up from zero (negative values never index a valid sound) and values
beyond the number of indexed sounds simply fail silently (heh) and no sound
effect is played.

#### 1: `radius`

The radius within which the game will play the index-defined sound at the
player. This radius, as always, is in cube units of 1/8m, and the bounding
surface for sound entities is always a simple sphere, represented in edit mode
by a wireframe bounding indicator. At distances beyond this radius, sound
playback from the entity is automatically rejected and the sound entity does not
playback in any form.

### 3.1.6 Spotlights
---

Spotlight entities require linking to standard entities, such as by `entlink`.
Once attached to a light, the spotlight acts as a modifier to the light entity,
creating a directed cone of light. The spotlight's lone attribute controls the
spread of the cone, and the vector from the light entity to the spotlight sets
the axis which the spotlight is oriented along. The location of the spotlight
otherwise has no effect upon the behavior of the spotlight's cast beam.

Notably, spotlights cause the light entity in question to switch from
cubemapping to planar mapping, resulting in issues when surfaces are normal to
the plane of the mapping (which is tangent to the point of the cone and normal
to the axial vector); since the mapping has no detail along the normal
direction, there exists aliasing issues along that face, and as a result large
cone sizes which project onto faces parallel with the spotlight vector is not
recommended.

#### Attributes

The lone attribute for the spotlight entity determines the spread of the cone of
light. An implicit attribute, the position of the entity, sets the direction of
the beam (measured relative to the location of the linked light.

#### 0: `angle`

This attribute sets the spread of the cone of light set by the spotlight entity.
The overall inside angle of the cone is equal to twice the value of this
attribute; the attribute measures the angle between the edge and center of the
beam. This attribute is capped at 90 degrees: as a result of single-plane
mapping, "spotlights" with a cone of light beyond 180 degrees is not
representable with the projection.

#### 3.1.7 Decals
---

Decals are static entities which act to project an image (specified by an index)
onto a surface of cube geometry. The limitation to cube geometry is an important
one, one that precludes the usage of mapmodels that can be manipulated easily
within the engine. Stains, which are used by weapons, can place their images
upon any type of geometry, including mapmodels.

Decals are loaded into the map's configuration file in the same manner that
textures are. The order in which the decals are defined sets their map-specific
index, which is then referenced when specifying the image used by the decal
entity.

#### Attributes

Decal entities have all five attributes contribute to the entity's behavior: the
first is the decal index, declared in the map configuration, while the last four
determine the orientation (1,2,3) and scale (4) of the decal. This is the same
set and order of attributes used for the `mapmodel` entity.

#### 0: `index`

Selects the index determining which of the decals that are loaded into the map
are to be displayed. As usual, this index begins at 0 and counts upwards; the
engine will simply display nothing if there is no valid decal at the index.

Decals are generally defined in map configuration files and therefore the
specific decal assigned to each index is not enforced game-wide. The decal's
index is simply allocated by the position of the decal's reference relative to
other decal references (the first mapmodel declared in the configuration file is
indexed 0, the second one 1, etc.).

#### 1: `yaw`

The yaw (azimuthal) angle of the decal, in left-handed (clockwise) degrees.
Values larger than 360 can be passed but are identical to passing in their
modulus 360.

#### 2: `pitch`

The pitch (altitude) angle of the decal, expressed as an inclination from the
horizon. Negative values can be used to pitch the decal towards the -z axis.

#### 3: `roll`

The roll angle of the decal, expressed as a right-handed rotation about the
axis set by the `pitch`/`roll` attributes.

#### 4: `scale`

The scale factor of the decal. Scaling is always isotropic (no distortion) and
the identity point is at 100 (100 is "normal" scale) as opposed to 1 for many
other engine features; this is because the arguments passed to entities are
always integers (and obviously setting 1 as unity w/ only integral steps would
be not quite optimal).

### 3.1.8 Teleporters
---

Teleporters are entities which serve to move players and actors located within
their vicinity to a linked `teledest` entity. Teleporters are not linked by the
usual method of entity linking and instead rely on their first attribute to
link. As teleporters are single-directional, a pair of teleporters are needed
for two way teleportation.

Teleporters additionally can have a fixed-size model rendered at their location,
with its index set by the same means as standard mapmodels. However, this entity
cannot be used to create models with nonstandard orientations or scales, and as
a result using a standard mapmodel in tandem with a no-entity teleporter is
generally more flexible.

#### Attributes

Teleporters have three attributes, the first of which sets the teleporter
channel to use (should generally be unique between pairs of teleporter/teledest
entities). The other two control the index of the representing mapmodel and the
team that is allowed to use the teleporter in team-based modes.

#### 0: `channel`

Sets the channel which the teleporter should connect to teledests through. The
teledest will need to be of the same channel as the teleporter entity; if not,
the teleporter entity will link to multiple teledests with predictibly
unpredictable results.

The teleporter entity will send a message indicating that there is `no teleport
destination for tag N` if no valid teledest entities are connected when a player
enters its trigger region. To remedy this, add a teledest with the same channel
number as the teleporter entity.

#### 1: `model`

Sets the index for the mapmodel used to represent the teleporter. As with
standard mapmodel entities, this index counts up from 0 through the number of
entities listed in the map configuration file, and values that are negative or
larger than are indexed in the map configuration file will not be represented.

#### 2: `team`

Sets the team which can use the teleporter when playing in a team mode. This
only applies when in a team mode and is ignored otherwise. The teams are indexed
as 0 for no restriction, 1 for just the first team (blue) and 2 for just the
second team (red).

### 3.1.9 Teledests
---

The complement to the teleporter entity, teledests are the location where a
player appears after going through a teleporter. Teledests need to be assigned
to a channel with the same index as the corresponding teleporter in order to
work; this means that a single pair of teleporter/teledest entities should be
the exclusive members of a channel. Players and actors who enter a teleporter
will then reappear instantaneously at the point where the teledest is placed;
in this respect it behaves somewhat similarly to a playerstart entity.

#### Attributes

Teledests have two attributes, corresponding to the channel that they operate on
and the yaw orientation of the player as they leave the teledest.

#### 0: `yaw`

The `yaw` attribute of the teledest sets the orientation about the z-axis where
the player spawns. This orientation is always enforced: players coming through
the teleporter and appearing at the teledest will always face in this yaw
direction. As with the rest of the vectors in the game, this angle counts
clockwise as the degrees increment.

#### 1: `channel`

As with the corresponding attribute in the `teleport` entity, the `channel`
attribute sets the channel that the teledest will "listen" on to link with a
teleporter entity. A pair of teleporter/teledest entities should have exclusive
use of a single channel, but as there is essentially no limit on the size of the
`channel` index, this should be of little concern.

### 3.1.10 Jumppads
---

Jumppad entities act to push players and actors with a velocity and direction
specified in its attributes. Jumppads impart a velocity to the players that
enter its physical radius, which can be any direction but are generally expected
to be upwards (hence the name). Jumpers, in a manner somewhat similar to
teleporters, has the option of taking a sound index which is played when the
jumper is triggered.

#### Attributes

Jumppads take four attributes, the first three of which set the *components* of
the push that is delivered when triggered, and the fourth of which optionally
sets a sound index to be triggered when the jumppad is triggered.

Use of the distance formula in three dimensions to determine overall magnitude
may be useful: `|r| = sqrt(|x|^2 + |y|^2 + |z|^2)`. The || bars indicate to use
the magnitudes of the x,y,z component vectors.

#### 0: `z`

The first ennumerated attribute component of jump direction is naturally the
last component of the Cartesian triple, and corresponds to the vertical
direction. This controls the extent to which the jumper pushes a player upwards.

#### 1: `y`

The second ennumerated attribute component of jump direction is the y-direction,
which controls the strength of the push in the y-direction. This corresponds to
a yaw of 0 degrees.

#### 2: `x`

The third ennumerated attribute component of jump direction is the x direction,
which controls the strength of the push in the x-direction. This corresponds to
a yaw of 90 degrees.

#### 3: `sound`

This attribute sets a sound that is played once when the jumppad is triggered;
like the corresponding mapmodel option for teleporters, this attribute acts much
like the index attribute for sound entities. The emitted sound is not otherwise
controllable.

### 3.1.11 Flags
---

Flags are the objective point for capture-the-flag gameplay. A working CTF game
requires that a flag for each team be placed, as it is not possible to capture
a flag without a flag point on one's own team to return the flag to. Like the
other player-interacting entities (jumppads, teleporters), flags do not have a
modifiable interaction distance and is hard-set for all flag entities.

It is possible to place multiple flag entities for a given team onto a map, but
players cannot possess more than one opponent's flag entity at a time.

#### Attributes

Flag entities have two attributes controlling their yaw orientation and team.

#### 0: `yaw`

Sets the yaw direction of the flag, in CW degrees from the y-axis, as with other
yaw parameters. The flag object is fairly anisotropic, so the orientation of the
flag entity is not particularly important compared to entities such as player
starts.

#### 1: `team`

Sets the team that owns the flag. Use 1/2 for the two teams (red/blue), as 0 is
internally set as team neutral.

## 3.2 Projectiles
---

Unlike static entities, projectiles are not created directly by mappers and are
instead created primarily by weapons as their fired projectiles. Projectiles
are synced across the server (as befitting their usually deadly nature) and
carry a number of properties befitting this which are distinct from static
entities.

### 3.2.1 Projectile Attribute Overview
---

Projectiles have eleven attributes encoded within them which define their entire
behavior from birth to death. Projectiles are created when a weapon fires, and
as a result they have owners and attack data.

* vec `dir` direction that the projectile is pointed
* vec `o` location of the projectile currently
* vec `from` world coordinates where the projectile starts
* vec `to` world coordiantes where the projectile ends
* vec `offset` displacement from from/to vec path defined
* float `speed` speed of projectile in cubits/s
* gameent `*owner` player who created the projectile
* int `atk` attack type of the projectile
* bool `local` multiplayer sync flag for the projectile
* int `offsetmillis` time delay for the projectile
* int `id` unique identifier for the projectile

### 3.2.2 Projectile Vector Attributes
---

There are five vector attributes which describe the orientation and velocity of
the projectile. These are the `dir`ection, l`o`cation, `from` originating
position, `to` destination position, and `offset` displacement from path.

#### `dir`ection

The first attribute of these is the `dir`ection, which determines the
orientation of the projectile. This vector determines the orientation of the
projectile in space, and is set to point along the direction of the target
(parallel to the velocity). As this vector does not imply a magnitude, it should
be a normalized (magnitude of 1) vector. This vector is in world coordinates, as
there is no independent coordinate system for projectiles defined by their
movement. As a result, corrections to a projectile entity must come from manual
changes to the direction of this parameter.

As this attribute is a 3 dimensional vector, there is no way of controlling
the roll of the projectile. As a result, spiraling projectiles are not possible.

#### l`o`cation

The second attribute is the l`o`cation of the projectile currently. At t=0 this
parameter is set to be the same as the `from` parameter, and the l`o`cation at
later times evolves towards the `to` location with respect to time. This vector
is expressed in the world coordinate system, which is the same for every
projectile.

#### `from`

The third attribute is the place where the vector is `from`. This is, for a
weapon-generated projectile, the location that the gun is when the projectile is
created. This vector is also identical to the l`o`cation vector at the time when
the projectile is created.

As with the l`o`cation vector, this vector is expressed in terms of world
coordinates, and all projectiles share the same coordinate system.

#### `to`

The point on the map where the projectile is pointed towards. This is, for a
weapon-generated projectile, the location at a distance given in the `range`
parameter; this point may be inside geometry, in which case the collision
checker will destroy the projectile at that point.

#### `offset`

The `offset` vector determines the location the projectile appears to originate,
as a distance away from the actual start position. This is used to make the
projectile appear from the gun rather than from the player's coordinate, and as
a result this attribute, unsurprisingly, is a displacement rather than a
position vector.

### 3.2.3 Other Projectile Attributes
---

The non-vector attributes that projectiles posess include the projectile's speed
(which does contribute to the not-explicitly-defined velocity vector), the owner
of the projectile, the attack type of the projectile, the locality flag, the
delay time for

#### `speed`

The `speed` of the projectile is the rate, in cubits, that the projectile moves
through the world. This, along with the direction determined by the difference
of the `to` and `from` vectors, defines the projectile's velocity vector.

#### `*owner`

The pointer to the `gameent` object that the projectile is credited to.
Projectiles are created by players, so to track accuracy and kills, projectiles
are always associated with a player object.

#### `atk`

The index for the attack type the projectile embodies. This attack type
corresponds to a particular weapon's attack, and therefore serves as a proxy for
the properties that particular weapon attacks have, such as the projectile's
visible type, size, and damage. As a result, this value can only correspond to
defined weapon attack types and encodes specific, specified sets of values.

#### `local`

This attribute flags the projectile as being local. Projectiles created by the
player will have this flag set as true, and projectiles that are created as
representations of other client's projectiles will have this set to false. This
flag affects whether the game uses this projectile's location to determine hits
(as other client's projectiles are not necessarily synced with the root client's
machine, the game relies on those other clients to hit check their own
projectiles. While this has some issues with abuse, this does mean that players
who see their bullets hit a target will always be credited with those hits, even
if other clients did not see the bullet hit them.

#### `offsetmillis`

As the name indicates, this attribute sets the offset time for the projectile to
start moving. This attribute is expressed in milliseconds, and needs to always
be positive to have any meaning.

#### `id`

Each projectile is given a unique tracking id by the engine, which is set by the
time at which the projectile spawns. This is the handle by which the projectile
can later be identified (such as to find its owner).

### 3.2.4 Projectile Time Evolution
---

Projectiles in the engine have simple kinematics, as neither gravity nor drag
act upon them. As such, projectiles move in straight lines at constant speed,
and do not drop or slow down over the course of their travel. This movement
continues until the projectile reaches its `to` point defined in the object or
collides with something, after which point it is destroyed.

Every physics frame, the projectiles owned by the player's own client are
checked for collision with players before being checked against the world
geometry. Those projectiles that collide with players are then eligible to deal
damage; projectiles that collide with world geometry are discarded after
potentially throwing decals onto nearby surfaces (like burn marks or bullet
holes).

As projectile speeds are defined in cubits per second, multiplication by 8
yields a speed in meters per second. As a result, for reference, the speed of
sound at standard temperature and pressure (343 m/s) is equivalent to about 2750
cubits/s.

Since the trajectory of the projectile is parameterized in terms of its end
location and its speed, the maximum time in flight must be calculated by the
range by the speed.

## 3.3 Bouncers
---

Bouncers are entirely unrelated to jumppad entities and are the name given to
the particles which can bounce off of surfaces. Bouncers do not interfere with
players in any way, and are capable of bouncing off of geometry for an arbitrary
number of times before being deleted and replaced with a decal.

Bouncers have two implemented types, gibs (bits of dead players) and debris, and
are defined by a set of twelve unique parameters as well as being a child of the
general `physent` class of game objects.

Unlike projectiles, which are parameterized in terms of maximum range, bouncers
are parameterized in terms of their lifetime. Note that while vectors like start
position are not listed below, they do exist as part of the parent physent
class.

The twelve unique parameters that bouncers have are as follows:

* int `lifetime` time left until the bouncer dies
* int `bounces` number of bounces that have been done
* float `lastyaw` yaw of the projectile at the last timestep
* float `roll` roll of the projectile at the last timestep
* bool `local` multiplayer sync flag for the projectile
* gameent `*owner` gameent which the projectile belongs to
* int `bouncetype` type of bounce the bouncer does
* int `variant` random variant of bouncer that this bouncer has
* vec `offset` world offset, starts at bouncer spawn point
* int `offsetmillis` time of projectile creation
* int `id` unique id assigned to each bouncer entity

## 3.4 Stains

Stains are a type of decal that is generally applied by the effect of another
entity's death. Examples of this include the bullet holes left behind when a
projectile makes contact with a surface or the blood stains left behind by a
dead actor's giblet.

### 3.4.1 Stain Objects
---

Stain objects have the following properties in their individual objects:

* int `millis` The game time when the stain spawns
* bvec `color` The color of the stain texture
* uchar `owner` The stain buffer the stain belongs to
* ushort `startvert` The vertex in the buffer the stain starts at
* ushort `endvert` The vertex in the buffer the stain ends at

### 3.4.2 Stain Settings
---

Unlike typical particles and decals, stains, by virtue of their entirely
cosmetic nature, have user-configurable settings to control their impact on
visuals and performance. For this reason, stains should never be assumed as they
can be turned off or modified client-side.

The commands:

* `stainfade <value>` time in s before stains fade away
* `stains <bool>` toggles rendering of stains
* `dbgstain <bool>` toggles debug output for stains

There are some Cubescript aliases which relate to shaders; those are not user
commands and are not covered here.

## 3.5 Particles
---

Particles are billboarded objects which are rendered clientside and simulate
small objects of various types. Particles broadly have three types: traditional
pointlike particles, linear "tape"-like particles, and meters.

As billboards, particles always face towards the player and therefore are always
viewed face-on. As a result, particle rendering is unique to each player and
is not shared across the server (as a point will have different facing vectors
to different actors' cameras.

Particles are not physents and do not bounce off of geometry nor interact with
the world in any particular way. Particles do, however, cull themselves upon
contact with geometry to prevent excessive resource usage.

### 3.5.1 Particle Types
---

Particles have many specific types which behave in different ways.

* `part` a point-like particle, such as puffs of smoke or flames
* `tape` an unanimated line particle that extends between two points
* `trail`
* `text` a text billboard, such as a player's name
* `textup`
* `meter` a meter with a fill between 0 and 100
* `metervs`
* `fireball` an animated fireball

### 3.5.2 Particle Properties
---

* vec `o` origin vector triple
* vec `d` direction vector triple
* int `gravity` gravity scale (<0 for upwards floaters)
* int `fade` fade scale
* int `millis` time in ms before fade
* bvec `color` color vector triple
* uchar `flags` particle-specific flags
* float `size` radius of particle
* union `(char *text, float val, physent *owner, (uchar color2[3], progress)`

The union type has a whole pile of objects inside it, and its structure is
outlined below:

```
|-----------------------------*text----------------------------|
|-------------val--------------|
|----------------------------*owner----------------------------|
|color2||color2||color2||-prog-|
```

The union is set as one of the four rows depending on the type of particle
present:

* `*text` pointer for a text entity
* `val` float for a fireball
* `*owner` pointer for a particle's owner
* `color2[3]` array and `progress` values for a meter

### 3.5.3 Pointlike Particles
---

The pointlike particles are internally refered to as being of the type `part`;
they are the particles that are most accurately refered to as a "particle".
Pointlike particles generally are created with some velocity along an axis and
additionally have some gravitational term which causes them to move in the z
direction.

Particle static entities of type `water`, `fire`, `smoke` are rendered as
pointlike particles.

### 3.5.4 Tape Particles
---

Tape particles are called `tape` for their resemblance to barricade tape in its
stationary, straight appearance, and act to create beam-like effects in the
level. The static particle entity which uses tape particles also goes by the
name `tape`; it creates tape particles along a certain direction.

### 3.5.5 Trail Particles
---

Trail particles create a number of standard particles radiating out from a
region of space. The `water` static particle entity type uses a particle trail.
Trail particles are potentially useful for following a projectile.

### 3.5.6 Text and Textup Particles
---

Text particles are most notably used ingame to render player names above their
heads. They also make an appearance while editing entities, as the entity type
is rendered as a particle above the entity origin.

Text particles have settable string, color, size and blend options to control
the content, transparency, font size, and color of the rendered text. There font
size is proportional to the particle's size parameter and does not follow
typesetting convention (as these don't make much sense with a 3D engine).

### 3.5.7 Meter and Metervs Particles
---

Meters are a status particle used to show the size of a particular value passed
to its `progress` value. `progress` is capped to values up to 100, and as a
result meters are restricted to integral percents in their representation.

More resolution is not particularly important for these particles, as they do
not display the actual value passed as a value, and those reading a meter
particle ingame would have trouble discerning values within a percent.

### 3.5.8 Fireball Particles
---

Fireballs are animated billboards which appear as a large ball of bright gas.
They are round and their general appearance is isotropic (no particular
orientation). Fireballs pulsate and have their surface change with time, which
makes them particularly suitable for relatively large sizes (normal particles
are static and hence fairly boring if they are multiple meters across.)

Fireballs are perhaps the particle least obviously a 2d billboard, as a result
its constant animation and scale change.

## 3.6 Physics
---

Physics apply to game entities called `physents`. Physents have a large number
of properties which affect their time evolution, and additionally are able to
realistically collide with surfaces.

Physents include item drops, players, non-player actors, and bouncers. These
entities also have additional properties unique to their respective entity
types, as they are all seperate children of the physent class.

### 3.6.1 Physent Properties
---

Physents all have the following properties:

* vec `o` origin vector (location displacement vector from origin)
* vec `vel` velocity vector
* vec `falling` gravity vector
* vec `deltapos` position displacement
* vec `newpos` interpolated next position
* float `yaw` yaw angle (around horizon CW)
* float `pitch` pitch angle (up/down; -90 down; 0 horizon; 90 up)
* float `roll` roll angle (CW about yaw+pitch vector)
* float `maxspeed` speed limit for this object (clamp speed to this level)
* int `timeinair` time spent without being on the ground in ms
* float `radius` size of entity
* float `eyeheight` height of eyes (default player = 18(/8) = 2.25m)
* float `maxheight` vertical size of entity (default palyer 18(/8) = 2.25m)
* float `aboveeye` clearance above eyes (default player 2(/8) = .25m)
* float `xradius` x hitbox radius
* float `yradius` y hitbox radius
* float `zmargin` z hitbox margin
* vec `floor` orientation of floor below physent
* int `inwater` material name of liquid that physent is in (0 otherwise)
* bool `jumping` whether the ent is in the process of jumping
* char `move` forwards and reverse movement
* char `strafe` side to side movement
* char `crouching` crouching (players)
* uchar `physstate` type of behavior physent is undergoing (e.g. falling)
* uchar `state` in normal play, state of physent (e.g. alive, spectating)
* uchar `editstate` in edit, state of physent (e.g. alive, spectating)
* uchar `type` type of entity (e.g. player)
* uchar `collidetype` bounding box type (e.g. elliptical)
* bool `blocked` toggles whether the ai should consider the physent blocked

### 3.6.2 Collision

# 5 Render
---

The core of the Imprimis engine is its renderer. The renderer is what transforms
the abstract objects in the world into visuals onscreen.

Imprimis' rendering capabilities are essentially a subset of Tesseract's, as
many static features in the Tesseract engine are not possible in Imprimis'
dynamic use case. The renderer is deferred, as opposed to forward as with
engines like Cube 2, and is capable of large numbers of dynamic lights onscreen
due to its architecture.

## 5.1 Texturing
---

Textures on world geometry can have one or several shaders applied to it which
affects its appearance. These effects are usually defined per-texture and
therefore immutable ingame, and can be modified on a per-map basis.

### 5.1.1 Shader Overview
---

#### Diffuse mapping (`stdworld`)

This is the standard color image of the texture, and what the texture browser
displays in its tiles. All other shaders also implicity present the diffuse map.

It is not possible (nor sensible) to have a texture without diffuse mapping, as
it provides the base image upon which other shaders may take effect.

#### Normal mapping (`bumpworld`)

This is mapping the surface normals (the actual orientation of the surfaces) and
calculating how the diffuse map's irradiated light changes because of the
orientation of the texture (regions pointing away from you are going to have
less area to shine light at you).

Normal mapping is a staple of 3D graphics, and nearly all surfaces have some
normal direction variance (macroscopic variance). Only where surfaces are
homogeneous and smooth (think lacquered surfaces) might it be applicable to
forgo a normal map.

Normal maps have three channels, corresponding to the three components of the
normal vector at any given point. By packing these three channels into a texture
file, it's possible to encode normal vector information for an entire surface.

#### Specular highlights (`specworld`)

This creates a specular reflection (where the surface reflects light sources
like a mirror) over the entire surface uniformly. The specular reflection
borrows none of its color from the underlying texture, and all of it from the
light it originally came from.

Specular highlights are homogeneous and do not take into account the
reflectivity of the surface, which is taken to be a defined constant. Most
real-world surfaces are not this homogeneous, and require a more complex shader,
outlined immediately below.

#### Specular mapping (`specmapworld`)

This maps out certain areas of the texture to have more specular reflection than
others: some parts of a texture, e.g. metal parts or areas worn smooth, are
going to specularly reflect more than other parts of a texture.

A specular map is a single channel grayscale file encoding how reflective an
area is for all locations on the surface.

#### Parallax mapping (`bumpparallaxworld`)*

* Note that bump is currently required to assign parallax to a texture, even
though the two are not necessarily required to be together. There are very few
circumstances in which it is possible to justify having parallax and not normal
mapping.

Parallax mapping changes how visible parts of the texture are depending on the
observers' position relative to them. This is different than normal mapping in
that normal mapping merely reduces the intensity of light from regions facing
away from you, while parallax mapping reduces its visible size. Together,
parallax and normal mapping can create a fairly convincing substitute for actual
geometry, though both have visible issues in their approximations at shallow
angles.

A parallax map (also called a heightmap, as it encodes vertical position) is a
single channel, which can be either its own grayscale file or the alpha channel
of the normal map.

#### Triplanar mapping (`triplanarworld`)

Triplanar mapping involves mapping the texture from three directions (x,y,z)
rather than one and using the information from those three orientations to allow
the texture to be mapped accurately at any orientation (rather than having
significant error at any orientation other than that of the cube face it
occupies). This is most useful for patching seams in compound curvature where no
patching of the seam with `voffset` or `vrotate` is possible.

Because triplanar mapping is fairly expensive, it is not recommended to be used
unless it is visibly needed. Triplanar mapping also disallows texture transforms
such as `vrotate`, so it cannot be used where texture rotations are needed.

#### Triplanar detail mapping (`triplanardetailworld`)

While standard triplanar mapping is useful for blending a texture with itself,
blending a texture with another according to angle is possibly useful (maybe???)
to smoothly transition without blendmaps.

The most plausible use case for `triplanardetailworld` would be to blend three
textures together on rough terrain, using the two textures for triplanar detail
mapping and laying a blendmap over the top of it for the third. Otherwise, just
using blendmap is preferable unless under some very strict map size restrictions
or consistency of the blend with respect to angle is critical.

To add the triplanar detail shader to a texture, setting the shader to
`triplanardetail*world` and declaring any normal/spec/etc. maps should be done
as usual.

The vslot to be blended should then be declared using `texdetail <vslot>` along
with other texture commands (e.g. `texscale`) at the end.

#### Glow mapping (`glowworld`)

Glow mapping makes certain of the textures always be lit. This is typical for
lights fixtures and computer equipment textures as well as other objects which
are always lit.

The glow map is an intensity map of the areas for the engine to glow. It can be
given in full RGB color for colored glow effects.

Note that this glow effect merely fixes the brightness of the texture to a
specified level. It does not actually create a light entity or light nearby
areas, which must be done with an actual light entity.

## 5.2 Lighting
---

Imprimis' light and shadow system is built on a deferred rendering pipeline
and is the main difference between it and older engines such as Cube 2. This
deferred pipeline offers advantages largely in the quantity of lights that can
be dynamically rendered onto the scene; however, it is not superior to Cube 2's
forward rendering pipeline in all aspects. The Imprimis rendering pipeline is
essentially the same as Tesseract's, and is outlined here.

There are essentially three types of lights in the engine:
* Sunlight
* Dynamic lights
* Static lights (map entities)

In the renderer, the latter two are treated in largely similar ways, while
sunlight is in a privileged position in the engine, being the only source for
which the engine's global illumination is enabled (due to performance issues).

### 5.2.1 Shadow Atlas
---

The engine's renderer uses a texture called a *shadow atlas* to cache the
mapping of lights onto surfaces in the game. The shadow atlas is a monochromatic
texture stored the GPU, 4096x4096 in size (32Mib of VRAM), and contains the
depth mappings of every light currently being rendered. The shadow atlas
does not have strong protections limiting its occupancy and therefore excessive
use of light entities can cause the shadow atlas to overflow and create visual
artifacts.

The shadow atlas is a depth buffer which maps how far parts of the scene are
from their sources. This is necessary for lights to determine how far away the
things they are lighting are so as to facilitate appropriate light intensities.

The depth of the shadow atlas can be changed from its default depth of 16 bits
per pixel (16bpp) to 32 bits by changing the `smdepthprec` variable. In general,
however, there is essentially no benefit to doubling the depth of the shadow
atlas to 32 bits.

### 5.2.2 Shadow Map
---
The shadow map is the actual texture which gets applied to textures ingame.
Using the depth information encoded in the shadow atlas, the shadow map contains
brightness information for lights which are being rendered. Like the shadow
atlas, the shadow map is square and monochromatic, but unlike the shadow atlas,
the shadow map size is variable and can be adjusted from resolutions of 2^10
(1024x1024) to 2^14 (16384x16384), an increase of 256 times. This large range
in shadow map size allows the engine's shadow map to scale in performance to
accomodate both the high performance of modern dedicated GPUs as well as
integrated graphics up to several years of age.

While the shadow map is monochromatic, the engine is indeed capable of lighting
in color. Color is not required in the shadow map, however, as light sources are
monochromatic; the final rendering output is modulated by the light entity's
particular color from the monochromatic shadow map. In doing so, Tesseract's
renderer saves the overhead of three channels per bit (or conversely, increases
the allowable precision by three times).

### 5.2.3 Shadow Map Filtering
---

The shadow map texture does not generally line up with shadow features, causing
ugly zig-zag aliasing which is particularly noticible at low shadow map
resolutions. To resolve this, the shadow map may be smoothed by different,
increasingly finer and more resource intensive methods.

The simplest and cheapest method, used by `smfilter 1`, is rotated grid
filtering. This filtering method is a simple antialiasing filter that reduces
"jaggies" in the shadowmap by attempting to alias them to a more beneficial
plane.

The two finer methods are increasingly wide weighted filters which can be fed
by either a simple bilinear tap method or by texture gathering. The latter is
the highest fidelity method, but the bilinear method benefits from native driver
support on modern GPUs. The size of the filter is either 3x3 for `smfilter 2` or
5x5 for `smfilter 3`; naturally, the wider filter is more compute intensive.

The use of bilinear taps or texture gather taps is controlled by the `smgather`
boolean variable.

Generally, however, shadow map filtering is a significantly faster way to remove
ugly shadow map aliasing than shadow map resolution increases, though shadow
filtering cannot construct sharper shadows like high shadow map resolutions are
able to do.

### 5.2.4 Cascaded Shadow Maps (CSM)
---

The sunlight in Imprimis is provided by a cascaded shadow map for maximum
performance while retaining high angular sharpness. The cascaded shadow map,
which for the sunlight is simply planar (as the sunlight comes collimated from
infinitely far away, there is no point in a 3d projection), consists of multiple
shadow maps of the same orientation in consecutively larger sizes. The contents
of a higher resolution shadow map (and its consequentially smaller angular size)
are cut out of larger shadow maps.

The result of cascaded shadowmapping is that the sunlight map is increasingly
higher resolution for regions closer to the camera location. This allows for
relatively cheaper, lower resolution shadowmapping of faraway sunlit locations
while maintaining good angular resolution up close.

In effect, cascaded shadow mapping is similar to mipmapping in its final form:
lessening distant rendering load by utilization of low resolution textures.

Relevant CSM commands:

* `csmbias <value>`
* `csmbias2 <value>`
* `csmcull <boolean>` Toggles masking of smaller CSM within larger ones.
* `csmdepthmargin <value>`
* `csmdepthrange <value`
* `csmfarplane <value>` Sets the size in cubits of the largest CSM.
* `csminoq <boolean>`
* `csmnearplane <value>` Sets the min size in cubits for the smallest CSM.
* `csmmaxsize <pixels>` Sets the size CSM texs relative to the shadow atlas.
* `csmpolyfactor <value>`
* `csmpolyfactor2 <valie>`
* `csmpolyoffset <value>`
* `csmpolyoffset2 <value>`
* `csmradiustweak <value>`
* `csmshadowmap <boolean>` Toggles rendering of the CSM (& therefore sunlight)
* `csmsplits <value>` Sets the number of CSM levels to use.
* `csmsplitweight <value>` Bias towards splitting CSM close (high) or far (low).

### 5.2.5 Global Illumination (GI)
---

Global illumination, also known as indirect lighting, is the illumination of
surfaces by the light reflected off of other surfaces. Global illumination
assumes diffuse reflection: that light that is shone upon surfaces bounces out
at random directions at an equal rate. While this is not quite exactly true
physically, it is a very good approximation to how lighting actually does
diffusely reflect.

The global illumination in Imprimis is calculated via the Radiance Hints
algorithm, which allows for a cheap approximation of indirect lighting via a
collection of *taps* placed automatically by the engine in the level. These taps
have light seeded by the values of a reflective shadow map (RSM) that is
calculated from the global sunlight (and not on-map light entities). The taps
then exchange with each other light "packets" which are then used to determine
the brightness of the surrounding area. The light "packets" recieved are
directional, so the taps store the light values in a low-order multipole
expansion of spherical harmonics (the cheapest way to do so).

The use of global illumination is to ameliorate excessive point lighting (which
is dynamic and therefore relatively expensive) by spreading the global sunlight
around the level. As this global lighting is fairly inexpensive, levels should
use sunlight + GI when possible.

#### The Reflective Shadow Map (RSM)

The RSM, unlike the shadow map, stores its values in a total of six channels
and two logical maps: the diffuse color of surfaces the sunlight impinges upon
is necessary to determine the color of the light which is diffusely reflected
off of those surfaces, as well as the orientation of those surfaces to determine
how strongly light has hit those surfaces. No depth map is needed in this case:
the sunlight, which comes from the far-field, does not meaningfully attenuate
within the scale of the map.

The RSM is by default a quarter the resolution of the standard shadow map: the
radiance hint taps generally don't have enough resolution to take advantage of a
very sharp map, and the RSM requires six channels compared to the shadowmap's
two.

## 5.3 Transparency
--

Transparency, also known as alpha, applies to objects which are partially clear,
but have some level of visibility, including with respect to other non-trivial
shaders (like specular or parallax mapping). Transparency is used by glass
material as well as by geometry which has had alpha material applied to it.

Transparency support in Imprimis is largely motivated by the particular
rendering architecture included therein. As a deferred renderer, which
composites full-scene maps of particular properties, Tesseract faces steep costs
to having multiple rendering layers (rendering a surface blended with another
surfaces). The compromise solution, while rather limited, does allow for limited
(single-layer) transparency.

### 5.3.1 Transparency Stenciling
---

Transparent regions in the engine, as marked by alpha material for cube
geometry, is rendered in a seperate step from ordinary geometry. The background
geometry is rendered with the transparency removed as usual, and then following
this the rendering pass is done over for transparent regions and then layered
over the top.

Multiple transparency passes have rather poor performance, and as such arbitrary
layers of transparency are rather non-performant due to the costs of making
arbitrary numbers of transparency stencils followed by arbitrary numbers of
rendering passes. Not only is the shading costs high for such an approach, but
also the memory space and bandwith requirements for such an arrangement.

### 5.3.2 Backface Transparency
---

A limited form of two-layered transparency, however, is supported by the engine.
The geometry that is flagged as alpha by the presence of alpha material can
optionally have its backface (the face visible from the other side of the
transparent region) rendered along with the front face. This requires extra
graphics resources, however, as an additional transparency pass is required
(though being on the backface of an already flagged region simplifies other
facets of transparency), and generally is only recommended where it can emulate
two seperate panes of reflective material.

This backface alpha property is enabled whenever the texture slot's `alpha`
property is set to a value greater than zero.

## 5.4 Screenspace Posteffects
---

Screenspace posteffects are a family of methods which use raster buffers
(essentially cached images) rather than the underlying geometry in order to do
their effects. These effects are fairly inexpensive, owing to the fact that GPUs
are very good at manipulating raster images, and as such they can approximate
techniques that are otherwise impractical to implement.

### 5.4.1 Screenspace Reflection (SSR)
---

Screenspace reflection may be the most well-known reflection technique as well
as the most well-known screenspace effect, as it allows for a relatively cheap
rendition of a notoriously difficult problem in game engine design: realtime,
responsive reflections. Screenspace reflections, indeed, is by far the fastest
and cheapest realtime reflection method, but it has traps and idiosyncracies
that need to be made aware to artists and level designers.

Screenspace reflection works by mapping the output frame onto itself, taking
a nearly-rendered frame and mapping this already-rendered frame onto reflected
surfaces on the level. This is vastly cheaper than doing a full reflection pass,
but at a cost: the only information the SSR pipeline has is that which is
located in the field of view. Looking into a mirror, for example, can't be done
with SSR, as the camera's location is not located at the location of the mirror
but rather that of the player, and of course, the player can't see their own
face through their own eyes.

Screenspace reflection, then, is mostly useful only for glancing views of
reflective surfaces, which is usually adequate for non-glaringly reflective
surfaces: the Fresnel effect in physics phys limits the reflectivity of most
real-world non-metallic surfaces to only occur at glancing angles.

SSR should usually only be used, then, for non-metallic surfaces which will
nearly always have their reflections mapped on the screen space itself: floors
and other horizontal surfaces in particular are excellent for this.

In spite of these drawbacks, however, SSR is generally the cheapest reasonable
way to provide the general appearance of reflectivity, and this is often good
enough for casual users such as video gamers who are presumably concentrated on
other things.

Currently, Imprimis only uses SSR for its water material's top surface, which
satisfies nearly every condition where SSR is applicable: it's horizontal,
non-metallic, and additionally its random movement distorts reflections, making
extreme resolution less important.

### 5.4.2 Screenspace Ambient Occlusion (SSAO)
---

A particular phenomenon of light propagation in real life is the darkening of
corners by the lack of available space for light to propagate inwards from. A
location in the corner created by two walls, for example, has a "field of view"
of only a quarter sphere, versus the half sphere visible from a point located in
the middle of a wall. Likewise, the junction of a ceiling/floor and two walls
only allows 1/8th of a sphere's worth of light into its vicinity, darkening it
further than a two-wall corner would.

These effects, in a perfect world, would be handled by high-resolution indirect
lighting techniques that calculate the diffuse bounce off of all surfaces and
compensate for this behavior. However, this is very expensive graphics-wise
indeed, and as such a crude approximation is rendered in the screenspace.

This screenspace effect works by looking at the depth buffer, an image
indicating how far objects on the screen are away from the camera, and darkening
valleys found in the depth buffer. This is vastly cheaper than sampling geometry
itself (as a depth buffer, as a single channel raster map, is very fast to
manipulate) and therefore allows for the simulation of the darkness generated by
the topology of the region. This darkening is then filtered to improve
smoothness, creating a realtime darkening effect of corners ingame.

### 5.4.3 Screenspace Refraction
---

A screenspace effect only applicable to transparent surfaces, such as glass and
alpha material, refraction emulates a particular effect that occurs when light
travels through optically dense surfaces such as glass. At material boundries,
light gets bent according to the surface normal of the boundary and the density
of the material, manipulating otherwise collimated light and distorting it.

While refraction technically should also create a parallax effect when there is
only one boundary (such as that which a pencil in a glass of water does),
implementing this is a nontrivial effect and also not a typical use case (nearly
all transparent volumes ingame are panes of some sort). As such, screenspace
refraction assumes that the volume itself is of relatively trivial thickness,
greatly simplifying the need for additional parallax calculations.

Screenspace refraction, then, distorts pixels in a region stenciled as
transparent using the transparent surface's topside normal map, providing the
impression that the imperfections in the material distorted the light coming
through it.

# 6 Actors
---

Actors are the entities that play the game: this includes human controlled
players and bot controlled players. At this time, no support for nonplayer
models exists: the only actors supported are ones that take the form of the
player model.

Actors are enlarged humans with a height of 2.5m (8') and a breadth of about 1m
(3' 3"). This slightly exaggerated size is such that a player can jump onto a 1m
tall box without being too exaggerated. As a result, players can fit in 3m by 1m
corridors without a problem, and crouch to fit in 2m by 1m corridors if
necessary.

## 6.1 Actor Objects

Actors are objects ingame with a large number of properties kept which are
synced to other players by the server ingame. This is significantly larger than
that of conventional entities and is synced much more carefully, as they are the
entities which are directly manipulated by players.

### 6.1.1 Actor Entity Properties
---

Actors are stored as an object `gameent` which is the object synced to other
clients. The information kept for each actor is kept regardless of their
ownership by a human player or a bot AI: they both interact with the engine
in the same way.

Actors store the following properties in their object fields:

* int `weight` weight of the player for hitboxes
* int `clientnum` the server ID for the player
* int `privilege` the level of authentication for the player
* int `lastupdate` time in ms since the last packet reception
* int `plag` packet lag (time in ms between packets)
* int `ping` ping time for client
* int `lifesequence`
* int `respawned`
* int `suicided`
* int `lastpain`
* int `lastaction`
* int `lastattack`
* int `attacking`
* int `lasttaunt`
* int `lastpickup`
* int `lastpickupmillis`
* int `flagpickup`
* int `frags` frags (kills) so far in the current game match
* int `flags` flags captured so far in the current game match
* int `deaths` deaths so far in the current game match
* int `totaldamage`
* int `totalshots`
* int `edit`
* float `deltayaw` change in yaw since last update
* float `deltapitch` change in pitch since last update
* float `deltaroll` change in roll since last update
* float `newyaw`
* float `newpitch`
* float `newroll`
* float `smoothmillis`
* string `name` client name shown to other players
* string `info`
* int `team` 1 (blue) or 2 (red): team the player belongs to
* int `playermodel` index of player model
* int `playercolor` index of player color (NOT hex color)
* ai::aiinfo `ai`
* int `ownernum`
* int `lastnode`
* vec `muzzle`

and the following functions:

* `gameent`
* void `respawn`
* `~gameent`
* void `hitpush`
* void `startgame`

# 7 Netcode
---

Like Tesseract and Cube 2, Imprimis runs a very client-heavy network topology
that minimizes the workload for the server itself. The server does little more
than redirect packets, while the individual clients have the burden of
interpreting what is passed to them by other clients.

As a result, Imprimis trades a good deal of security for a very simple and
responsive networking experience. It is not particularly difficult to create
clients which exploit this behavior, but server-mediated behavior is problematic
for a first person shooter where reaction time is paramount.

# 7.1 Topology

The general topology of networking and its relation to the state of game state
(variables and information) is outlined below.

```
+------------------------+------------------------+------------------------+
| Local Client           | Dedicated Server       | Remote Clients         |
+------------------------+------------------------+------------------------+
|                  Synchronous &            Synchronous &                  |
| +-------+         ratelimited              ratelimited         +-------+ |
| |       +-----------+  .  +------------------+  .  +-----------+ Non   | |
| | Local |  Client   |  .  |                  |  .  |  Client   | Local | |
| | State | Broadcast |---->|- - - - - - - - ->|---->|   Server  | Remote| |
| |       +-----------+  .  |                  |  .  +-----------+ State | |
| +-------+              .  |   Multicaster    |  .              +-------+ |
| |  Non  +-----------+  .  |                  |  .  +-----------+       | |
| | Local |  Client   |<----|<- - - - - - - - -|<----|  Client   | Local | |
| | State |   Server  |  .  |                  |  .  | Broadcast | Remote| |
| |       +-----------+  .  +------------------+  .  +-----------+ State | |
| +-------+              .                        .              +-------+ |
+------------------------+------------------------+------------------------+
```

As indicated in the diagram, the dedicated server does not control the game
state; it merely shuffles packets between clients which update their nonlocal
state. All dynamic and detministic objects in the game are assigned to a client
as part of their local state, which other clients ("remote clients") read and
copy to their nonlocal state information.

Clients are not charged with interpreting the behavior of game objects outside
of their local state: they accept the results that they recieve to their
nonlocal state information.

## 7.2 Server
---

The server in Imprimis is always present, even in singleplayer, and relays
information between clients. In singleplayer or in multiplayer games with bots,
these bots have full client status and their information is relayed to the
player via the server.

The server is very sparse in its function, and only a handful of system
resources are required to run one: Raspberry Pis are generally adequate for this
task.

A server instance does not check the contents of the packets which it sees and
essentially only radios what the clients tell it to other clients. A server's
control over the game is limited to its control over the gamemode and game end
time (ensuring that no one client can try to end the match at its whim) and
managing client bans and other holds.

### 7.2.1 Protocol
---

The server talks to clients via UDP using the ENet library. As the game uses
the UDP protocol, the job of making packets is left to the ENet library, which
allows for skipping the time consuming checks that TCP requires.

ENet is IP v4 only currently, and therefore the game cannot resolve IP v6
addresses.

### 7.2.2 Server State
---

The state of a server, or the contents which it "knows" at any given time, is
quite limited. Servers know the following:

* Time left in the match
* Addresses of connected clients
* Type of client (local, remote, bot)
* Master server listing status
* Time since master server listing confirmation
* Location and status of server entities (pickup items)

The server does not know where players are and does not keep track of projectile
locations.

### 7.2.3 Ratelimiting
---

To prevent server bandwith packet spam attacks, the server limits packets to one
every 7ms (143/s). This is somewhat lower than the maximum packet transmission
speed of clients, but is sufficiently higher than refresh rates and reaction
times of players that this is not a significant problem. By doing so, dedicated
servers cannot be innundated with a client sending many packets in very short
succession.

## 7.3 Client
---

Clients are vastly more fleshed out in the Imprimis multiplayer system. Clients
manage not only actors (players) but also their projectiles. In this way, all
deterministic dynamic gameplay events are controlled by clients. Clients have
their own "clients" and "servers" which refer to the side of the netcode that
sends events (client) and those that recieve the events (server).

### 7.3.1 Packets
---

Clients send messages to other clients (via the server) in packets with one of
over one hundred `types` which encode what kind of action the packet applies to.
Some message types are of a fixed length (e.g. cube modification), but others
are of variable length (e.g. chat messages).

### 7.3.2 Client Scope
---

The scope of an individual client is quite large, as the clients alone must bear
the full weight of all dynamic deterministic events in the game. This includes:

* Player movement
* Projectile movement
* Hit/Kill Determination
* Geometry and Entity Modification
* Global Variable Changes

This does not include non-deterministic or non-dynamic behavior, such as:

* Static entity rendering (e.g. particle entities)
* Aesthetic rendering (ragdolls, stains, projectiles)

#### Hit/Kill Determination

Clients tell others when they've hit somebody else, rather than the reverse
(clients telling others when they've been hit). As a result, hit confirmation
should always look "right" for clients: the body that confirms the hit is the
one who fired the projectile.

This is notable because network lag can cause a player's broadcasted position to
differ from the position that the client itself believes it is at. As a result
of this, clients dealing with a laggy client don't have to trust that client's
percieved position to record a hit.

## 7.4 Master Server
---

The master server is a seperate piece of software which can serve as a directory
for clients to find game servers with. The master server does not host game
servers itself; it is a service hosted by an organization (such as the official
Imprimis project) that can be used for clients to see where servers are located.

The server browser in the game gets a list of currently listed servers from a
master server (defaulting to the official one) and then presents them in the
server browser. Game servers periodically send a sync message to the master
server, allowing for the master server to automatically delist stale servers
that have not recently responded.

There is additional functionality in the master server to support centralized
authentication, but due to archtectural concerns this usage is depreciated.

# 10 Internal Objects
---

A number of C++ objects are defined in the engine to facilitate manipulation in
a replicable way. Many of these are geometry constructs which carry with them
the algebra and geometry of the structures they describe.

The game also has many specific-purpose objects which are described in their
particular section. This chapter is reserved for general, extensible objects
with utility in many potential parts of the engine.

## 10.1 Vector Objects

A large number of vector objects exist in the game to facilitate working with
objects in 2D, 3D, 4D, quaternion, and dual quaternion vector spaces.

#### 10.1.1 `vec`
---

`vec` is an incredibly ubiquitous object in the engine, where it is referenced
thousands of times over essentially every part of the game code. Key features of
the vec object:

* `vec` is always 3 dimensional, and has three defining float values.
* `vec` has its three arguments as either `x,y,z` or `r,g,b` (in a union)
* `vec` has many linear algebra operators defined for it: see `shared/geom.h`

As the `vec` object is only defined for a 3-vector, seperate classes like `vec2`
and `vec4` are used to do two or four dimensional linear algebra. However, these
objects are much less common and also have less operators defined for it,
befitting a 3d engine where locations of objects in the world are nearly always
defined as a 3d vector.

#### 10.1.2 `bvec`
---

`bvec` is a 3d color vector object. As opposed to the standard `vec` object,
which is useful mainly in world geometry, the `bvec` vector is intended for use
in the color vector space. In this space, the three basis vectors are R/G/B.

`bvec` does not inherit properties from the standard `vec` object and is not
capable of doing standard linear algebra operations like `vec` can; for this
reason, it is not suitable for use in standard geometric constructions.

Important properties of the `bvec` object:

* `bvec` is always 3 dimensional, and has three defining character values.
* `bvec` has its three arguments as either `x,y,z` or `r,g,b` (in a union)
* `bvec` has mostly color conversion operators defined and few normal operators

Critically, values in a `bvec` are of type `char`, meaning they are one byte
long and can encode values between 0 and 255. There is no notion of sign with a
`char`, and indeed having colors with negative values in its channels makes no
sense either.
