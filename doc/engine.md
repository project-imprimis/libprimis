# The Imprimis Engine

#### Note that this is a draft and subject to expansion and modification.

This text is an expository one, describing the *what* and some of the *why*
things are implemented as they are in the engine. This document does not attempt
to explain how the game is implemented.

## Chapters

#### 1. Standards
* 1.1 Coding Standards
* 1.2 Default Paths & Libraries
* 1.3 Conventions and Units

#### 2. World
* 2.1 Octree
* 2.2 Materials
* 2.3 Textures
* 2.4 Global Properties

#### 3. Entities
* 3.1 Static Entities
* 3.2 Projectiles
* 3.3 Bouncers
* 3.4 Particles
* 3.5 Physics

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

#### 7. Netcode
* 7.1 Server
* 7.2 Client
* 7.3 AI
* 7.4 Master Server
* 7.5 Authentication


#### 8. User and System Interfaces
* 8.1 Menus
* 8.2 Hudgun
* 8.3 File I/O & Assets
* 8.4 Console

#### 9. Game Implementation
* 9.1 Weapons
* 9.2 Game Variables
* 9.3 Modes

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
Opening brackets get their own new line.

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

#### 1.3.3 Colors
---

Colors which are defined past 0xFFFFFF (hex color for white) are generally
passed as a set of three paramaters `R G B` where `1.0 1.0 1.0` is 0xFFFFFF.
These colors tend to have `1.0 1.0 1.0` as the default and are expected to vary
upwards as much as downwards in practice.

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

The recursive nature of octree nodes are most useful for the simplicity of
occlusion queries. Occlusion queries are made significantly faster by being able
to discard members of the tree near the root and therefore skipping all the
lower members of the tree which necessarily are within their parent's node.

Cubes in Imprimis, the most basic form of geometry in the game, therefore occupy
the octree; instead of vertices in other engines being determined by their 3D
vector from the origin, a cube's place in the octal tree determines its
location.

### 2.1.2 Cube Manipulation
---

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
* Each vertex can only be found at one of 512 discrete points within the node.
* Textures always get an entire face of an octree node.
* Textures are projected from the node normal, not the deformed surface normal.

Therefore, the only way to increase detail in a given area when using cube
geometry is to increase the octree node density (by using a smaller gridpower).

For more information on texture projection, see §2.3.3.

### 2.1.3 Remipping and Subdivision
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
* `waterenvmap`
* `waterfallenv`
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
* `cloudclip <value` Sets level of clipping passed to envmap draw.
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
* envmap: a point where the engine captures an environment map
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
or envmaps or simply having a defined structure as mapmodels do. Entities are
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

### 3.1.4 Envmaps
---

Envmaps (short for environment maps) are entities that are used by the renderer
in order to simulate specular reflection originating from surrounding features.
Environment maps do not require realtime calculation, making them relatively
inexpensive to render, but due to their large memory footprint (the game saves
six textures representing a cubemap for each envmap point), the physical envmap
points need to be placed by the mapper.

Environment maps have no effect unless textures within its radius have the `env`
shader enabled in their configuration; this means that standard textures have no
reflection unless specifically enabled to do so. Typical candidates for enabling
environment mapping include glass and metallic surfaces.

As environment maps are statically calculated, they are required to be refreshed
manually in order to get them to display properly if they have been moved since
map load. To do this, the `recalc` command is recommended.

#### Commands

`envmapbb <boolean>` Toggles envmaps' bounding regions between spherical/square.
`envmapsize <n>` (user setting) Sets texture size for envmaps; edge length 2^n.
`envmapradius <value>` Sets the default radius of an envmap.

#### Attributes

Environment maps have three attributes controlling their behavior. These are the
radius, map size, and blur of the envmap entity's environment map.

#### 0: `radius`

The `radius` attribute sets the range at which the envmap affects geometry
rendering. If `envmapbb = 1` then this determines half of the edge length of the
bounding box which determines members of the envmap; if `envmapbb = 0` then this
value simply sets the radius of the sphere which bounds textures to be
envmapped.

#### 1: `size`

The `size` attribute sets the size of the environment map generated by the
envmap entity. This is a size, in powers of two, for each of the six faces which
are saved as a cubemap around the environment map.

This means that e.g. a `size` of 9, for example, means that there are
6*(2^9)^2 pixels in the environment map (3 megapixels). Bumping the size to 10,
then, is 6*(2^10)^2 = 6 MP or twice the VRAM required; for this reason, large
numbers of large environment maps can lead to VRAM shortages.

#### 2: `blur`

The blur parameter, which is clamped to either 1 or 2, sets the blur level to
pass when generating the environment map. For envmaps with low resolution,
blurring may help hide rasterization artifacts.

Since blur is a process applied to the *static* environment map which is cached
and not dynamically calculated, blur has absolutely no performance effect
ingame.

### 3.1.5 Particles
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

### 3.1.6 Sound
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

### 3.1.7 Spotlights
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

#### 3.1.8 Decals
---

Decals are static entities which act to project an image (specified by an index)
onto a surface of cube geometry. The limitation to cube geometry is an important
one, one that extends beyond just the static entity to decals in the engine at
large; the engine is not capable of placing bullet holes or blood stains onto
any mapmodel geometry.

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

### 3.1.9 Teleporters
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

### 3.1.10 Teledests
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

### 3.1.11 Jumppads
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

### 3.1.12 Flags
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

### 3.2.1 Projectile Attributes
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
