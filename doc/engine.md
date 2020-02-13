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
* 3.2 Actors
* 3.3 Projectiles
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

#### 6. Netcode
* 6.1 Server
* 6.2 Client
* 6.3 AI

#### 7. User and System Interfaces
* 7.1 Menus
* 7.2 Hudgun
* 7.3 File I/O & Assets
* 7.4 Console

#### 8. Game Implementation
* 8.1 Weapons
* 8.2 Game Variables
* 8.3 Modes

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

### 1.3 Conventions and Units

#### 1.3.1 Distances

Distance is always in the unit of cube units ("cubits"), which is the size of a
gridpower 0 cube, when not specified. This distance is equal to an eighth of a
meter, 12.5 centimeters, or approximately five inches (to within a couple %).

#### 1.3.2 Coordinates

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

Colors which are defined past 0xFFFFFF (hex color for white) are generally
passed as a set of three paramaters `R G B` where `1.0 1.0 1.0` is 0xFFFFFF.
These colors tend to have `1.0 1.0 1.0` as the default and are expected to vary
upwards as much as downwards in practice.

# 2. World

The *world* is the name for the level that the game is played on, including the
octree geometry, materials, and cloud/skyboxes. The world does include the
static entities as well which are placed upon it, but those are instead
described in the third chapter, which covers entities and actors in more detail.

The world's biggest feature is the octal tree that makes up the primary level
geometry. This geometry has the advantage of being easily occludable and simple
to modify on the fly, as opposed to models which are placed on the world as
static entities.

## 2.1 Octree Geometry

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
* Each vertex can only be found at one of 512 discrete points within the node.
* Textures always get an entire face of an octree node.
* Textures are projected from the node normal, not the deformed surface normal.

Therefore, the only way to increase detail in a given area when using cube
geometry is to increase the octree node density (by using a smaller gridpower).

For more information on texture projection, see §2.3.3.

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

#### Commands

`remip`: performs a remip calculation on the level
`maxmerge N`: sets the maximum merge gridpower to N

## 2.2 Materials

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

Air, the name for the lack of materials, unsurprisingly is the default
"material" for the level. Air can be "placed" by `/editmat air` or `/air`
whereupon it removes all previously existing materials flagged for that
selection.

The name "air" does not imply that there is any oxygen mechanics in the game
and there is no material representing the lack of air.

### 2.2.2 Water

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

No GI material flags its volume as not being lit by global illumination. This
does not have a material impact (or improvement) in performance but may be
useful to combat artifacts in the low-resolution global illumination algorithm.

Note that those regions beyond the radiance hints far plane (`rhfarplane`) will
be lit regardless of their No GI status. Keep this in mind whenever placing
long-distance radiance hints.

### 2.2.9 Alpha

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

Virtual slots encode simple manipulation of textures, such as coloration, scale,
rotation, and orientation. These do not require declaration upon map generation
and are generally created dynamically ingame after the execution of a `v`
command. Virtual slots (vslots) then save only the parent texture index and the
modifications done to it; they save significant memory space by not requiring
a modified copy to be stored in video memory.

The modifications that a vslot can store are described below in the V-command
section.

### 2.3.3 Texture Projection

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

The world in Imprimis has many global variables that affect the entire level
evenly. These include ambient lighting, fog, and skybox settings, as well as
more technical aspects such as mipping intensity. This section does not include
the global settings for individual materials, as is covered in §2.2.

### 2.4.1 Sunlight

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
* `cloudfade <value>` Sets the fade rate
* `cloudheight <value>` Sets the apparent height of the clouds in the sky.
* `cloudlayer <path>` Sets the path to the cloud layer, excluding extension.
* `cloudoffsetx <value>` Sets the x offset amount, in pixels.
* `cloudoffsety <value>` Sets the y offset amount, in pixels.
* `cloudscale <value>` Sets the scale factor of the clouds (1 by default).
* `cloudscrollx <value>` Sets the x scroll amount, in pixels/s.
* `cloudscrolly <value>` Sets the y scroll amount, in pixels/s.
* `cloudsubdiv <value>` Sets the number of edges the cloud perimeter has.

### 2.4.7 Atmo

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

# 3. Entities

## 3.1 Static Entities

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

### 3.1.1 Lights

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

#### 0 `radius`
The maximum distance the light entity can cast light; strongly related to
performance impact of the light and shadow map usage

The radius of the light is, as with other distances, defined in cubits.

#### 1 `red`

The intensity of the red channel of the light's output. Nominally, 255 is "full"
red, but this can be exceeded for an overbright light.

#### 2 `green`

The intensity of the green channel of the light's output. Nominally, 255 is
"full" green, but this can be exceeded for an overbright light.

#### 3 `blue`

The intensity of the blue channel of the light's output. Nominally, 255 is
"full" blue, but this can be exceeded for an overbright light.

#### 4 `flags`

Lights support four flags which can be combined to achieve particular effects.

* 1 `noshadow`: treats geometry/models as transparent, allowing light through
* 2 `static`: disables shadow map updates, causing static shadows
* 4 `volumetric`: simulates light reflection off of dust in the air
* 8 `nospec`: disables specular highlights
