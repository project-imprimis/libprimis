# The Tesseract Renderer

This document is a verbatim copy of the Tesseract rendering document
[http://tesseract.gg/renderer.txt](http://tesseract.gg/renderer.txt), converted
to GFM (GitHub Flavored Markdown).

This is for the engine that Imprimis is based upon, and its design choices may
not always apply to Imprimis. However, in most cases, Imprimis uses the same
rendering decisions (and code) as Tesseract.

## The Tesseract Rendering Pipeline

This article is a brief overview of the rendering pipeline in Tesseract, an
open-source FPS game and level-creation system. It assumes basic knowledge of
modern rendering techniques and is meant to point out particular design
decisions rather than explain in detail how the renderer works.

### Platform

The Tesseract renderer ideally targets OpenGL 3.0 or greater, using only Core
profile functionality and extensions, as its graphics API and using SDL 2.0 as a
platform abstraction layer across Windows, Linux, *BSD, and MacOS X. However, it
also functions on OpenGL 2.1 where extensions are available to emulate Core
profile functionality. Mobile GPUs with OpenGL ES are, at the moment, not
targeted. GLSL compatibility is provided across various incompatible versions
using a small preprocessor-based prelude of macros abstracting texture access
and attribute/interpolant/output specification that would otherwise be
incompatible across GLSL versions.

### Motivation

The main design goal of Tesseract is both to support high numbers of dynamically
shadowmapped omnidirectional point-lights and minimize the number of rendering
passes used with respect to its predecessor engine, Sauerbraten. While improved
visuals over Sauerbraten were somewhat desired, it was more important to make
the rendering process more dynamic with at least comparable visual fidelity
while still emphasizing performance/throughput, although at a higher baseline
than Sauerbraten required. So while other potential design choices might have
resulted in further visual improvements, they were ultimately discarded in the
service of more reasonable performance.

Sauerbraten made many redundant geometry passes per frame for effects such as
glow, reflections and refractions, and shadowing that multiplied the cost of
geometry intensive levels and complex material shaders. So, where possible,
Tesseract instead chooses methods that factor rendering costs, such as deferred
shading which allows complex material shaders to be evaluated only once per
lighting pass, or screen-space effects for things such as reflection that reuse
rendering results rather than require more subsequent rendering passes.

Further, Sauerbraten was reliant on precomputed lightmapping techniques to
handle lighting of the world. This approach posed several problems. Dynamic
entities in the world could not fully participate in lighting and so never
looked quite "right" due to mismatches between dynamic and static lighting
techniques. Lightmap generation took significant amounts of time, making light
placement a painful guess-and-check process for mappers, creating a mismatch
between the ease of instantly creating the level geometry and the
not-so-instant-process of lighting it. Finally, storage of these lightmaps
became a concern, so low-precision lightmaps were usually chosen, at the cost of
appearance to reduce storage requirements. Tesseract instead chooses to use a
fully dynamic lighting engine to resolve the mismatch between lighting of
dynamic and static entities while making better trade-offs between appearance
and storage requirements.

Certain features of Sauerbraten's renderer that were otherwise functional such
as occlusion culling, decal rendering, and particle rendering have mostly been
inherited from Sauerbraten and are not extensively detailed in this document, if
at all. For more information about Sauerbraten in general, see
http://sauerbraten.org

### Shadows

Tesseract's shadowing setup is built around observations originally made while
implementing omnidirectional shadowmapping in the DarkPlaces engine.

The first observation is that by use of the texture gather (a.k.a. fetch4)
feature of modern GPUs, it is possible to implement a weighted box PCF filter of
sufficient visual quality that generally performs better than all other
competing shadowmap filters while also requiring less bandwidth-hungry shadowmap
formats, especially if 16bpp depth formats are used. This is advantageous over
competing methods such as variance shadowmaps or exponential shadowmaps that
prefer high-precision floating-point texture formats or prefiltering with
separable blurs that can become quite costly when shadowmaps are atlased into a
larger texture as well as suffering from light bleeding artifacts that plain old
PCF does not have any problems with. A final benefit of relying only on plain
old depth textures and PCF is that depth-only renders are generally accelerated
on modern GPUs and so provide a speedup for rendering the shadowmaps in the
first place over other techniques before they are ever sampled.

For general information about PCF filters, see
http://www.gdcvault.com/play/10092/Efficient-PCF-Shadow-Map

It was later discovered that this same weighted box filter could be approximated
with the native bilinear shadowmap filter (originally limited to Nvidia hardware
under the "UltraShadow" moniker, but now basically present on all DirectX 10
hardware when using a shadow sampler in combination with linear filtering) so
that no texture gather functionality is even required, and allowing further
performance enhancements. The particular approximation avoids use of division/
renormalizing blending weights while only causing a slight sharpening of the
filter result that is almost indistinguishable from the aforementioned weighted
box filter. This method in general, though, allows the (approximated) NxN
weighted box filters to be implemented in about (N+1)/2*(N+1)/2 taps. The
default shadowmap filter provides a 3x3 weighted box filter using only 4 native
bilinear taps, providing a good balance between performance and quality.

The final 3x3 filter utilizing native bilinear shadow taps contains some
non-obvious voodoo and was largely found by experimenting with fast
approximations for renormalizing filter weights in the weighted box filter.
Ultimately it was discovered that just the seed value for iteration via Newton's
method (http://en.wikipedia.org/wiki/Newton's_method) was more than sufficient
to compute filter weights and did not significantly impact the look of the
result. Texture rectangles are also used where possible instead of normalized 2D
textures to avoid some extra texture coordinate math. The filter (with the
unoptimized yet more precise box filter in comments) is listed here for
posterity's sake:

```cpp
#define shadowval(center, xoff, yoff) float(shadow2DRect(shadowatlas, center + vec3(xoff, yoff, 0.0)))
float filtershadow(vec3 shadowtc)
{
    vec2 offset = fract(shadowtc.xy - 0.5);
    vec3 center = shadowtc;
    //center.xy -= offset;
    //vec4 size = vec4(offset + 1.0, 2.0 - offset), weight = vec4(2.0 - 1.0 / size.xy, 1.0 / size.zw - 1.0);
    //return (1.0/9.0)*dot(size.zxzx*size.wwyy,
    //    vec4(shadowval(center, weight.zw),
    //         shadowval(center, weight.xw),
    //         shadowval(center, weight.zy),
    //         shadowval(center, weight.xy)));
    center.xy -= offset*0.5;
    vec4 size = vec4(offset + 1.0, 2.0 - offset);
    return (1.0/9.0)*dot(size.zxzx*size.wwyy,
        vec4(shadowval(center, -0.5, -0.5),
             shadowval(center, 1.0, -0.5),
             shadowval(center, -0.5, 1.0),
             shadowval(center, 1.0, 1.0)));
}
```

This idea is extended to larger filter radiuses but is not shown here.

After experimenting with different projection setups for omnidirectional shadows
such as tetrahedral (4 faces) or dual-parabolic (2 faces), it was found that the
ordinary cubemap (6 faces) layout was best as the larger number of smaller
frustums actually provides better opportunities for culling and caching of faces
while providing the least amount of projection distortion. However, for
multi-tap shadowmap filters, the native cubemap format is insufficient for
easily computing the locations of neighboring taps. Also, despite texture arrays
allowing for batching of many shadowmaps during a single rendering pass, they do
not allow adequate control of sizing of individual shadowmaps and their
partitions. 

For further information about the basics of rendering cubemap shadowmaps, see
page 42+ of:

https://http.download.nvidia.com/developer/presentations/2004/GPU_Jackpot/Shadow_Mapping.pdf

Both of these problems may be addressed by unrolled cubemaps, or rather, by
emulating cubemaps within a 2D texture by manually computing the offset of each
"cubemap" face within an atlas texture. The face offset needs only to be
computed once and then any number of filter taps can be cheaply computed based
on that offset. The perpective projection of each frustum must be slightly wider
than the necessary 90 degree field-of-view, to allow the filter taps to sample
some texels outside of the actual frustum bounds without crossing any face
boundaries. A filter with an N texel radius needs a face border of at least that
many texels to account for such out-of-bounds taps.

Further, it becomes trivial to support custom layouts based on modifying the
unrolled lookup algorithm, or to allow other types of shadowmap projections to
co-exist with the unrolled cubemaps in a single texture atlas. Yet another
advantage of the cubemap approach in general, not limited to unrolled cubemaps,
is that rather than sampling omnidirectional shadows frustum-by-frustum
(requiring as many as 6 frustums) as some other past engines do and needing
complicated multi-pass stenciling techniques limit overdraw, the omnidirectoinal
shadowmap may be sampled in a single draw pass over all affected pixels.
 
Initially, this emulation was done by use of a cubemap (known as a "Visual
Shadow Depth Cube Texture" or VSDCT) to implement the face offset lookup to
indirect into the texture atlas. Later, an equally efficient sequence of simple,
coherent branches was discovered that obviated the need for any lookup texture
and removed precision issues inherent in the lookup texture strategy. The lookup
function that provided the best balance of performance across Nvidia and AMD
GPUs is listed here:

```cpp
vec3 getshadowtc(vec3 dir, vec4 shadowparams, vec2 shadowoffset)
{
    vec3 adir = abs(dir);
    float m = max(adir.x, adir.y);
    vec2 mparams = shadowparams.xy / max(adir.z, m);
    vec4 proj;
    if(adir.x > adir.y) proj = vec4(dir.zyx, 0.0); else proj = vec4(dir.xzy, 1.0);
    if(adir.z > m) proj = vec4(dir, 2.0);
    return vec3(proj.xy * mparams.x + vec2(proj.w, step(proj.z, 0.0)) * shadowparams.z + shadowoffset, mparams.y + shadowparams.w);
}
```

This function overall maps a world-space light-to-surface vector to texture
coordinates within the shadowmap atlas. A useful trick is used in the first few
lines for computing a depth for the shadowmap comparison - the maximum linear
depth along the 3 axial projections is ultimately the linear depth for the
cubemap face that will be later selected - and allows the depth computation to
happen before the resulting projection is found via branching giving slightly
better pipelining here. Note that this lookup function assumes a slightly 
on-standard orientation for the rendering of cubemap faces that avoids the need
to flip some coordinates relative to the native cubemap face orientations. It
otherwise lays out the faces in a 3x2 grid. Various math has been baked into
uniforms and passed into this function to transform to post-perspective depth
from linear depth for the later actual shadowmap test in the filtershadow
function. The function is only listed here to give an idea of the performance of
the unrolled cubemap lookup, so reader beware, it is not quite plug-and-play and
some investigation of the engine source code is required for more details. The
result of this lookup function is then passed into the filtershadow function
listed above. These two little functions are rather important and inspired
Tesseract's design; they represent its beating heart and make large numbers of
omnidirectional shadowed lights possible.

All of the shadowmaps affecting a single frame are further aggregated into one
giant shadowmap atlas, currently 4096x4096 using 16bpp depth texture format.
This better decouples the shadowmap generation and lighting phases and allows
lookups for any number of shadowmaps to be easily performed in a single batch or
many shading passes. Various types of shadowmaps are stored in the atlas:
unrolled cubemap shadowmaps for point lights, a simple perspective projection
for spotlights, and cascaded shadowmaps for directional sunlight.

For cascaded shadowmaps for sunlight, Tesseract uses an enhanced parallel-split
scheme with rotationally invariant world-space bounding boxes rounded to stable
coordinate increments for each split as originally detailed for Dice's Frostbite
engine. This allows for somewhat less waste of available shadowmap resolution
than the standard view-parallel split scheme as well as combats temporally
instability/shadow swim that would otherwise occur. For further information,
see:

http://dice.se/publications/title-shadows-decals-d3d10-techniques-from-frostbite/

"Caching is the new culling." Lights can often have large radiuses that pass
through walls and other such occluders, often making occlusion culling or
view-frustum culling of light volumes ineffective. As an alternative to never
the less greatly reduce shadowmap rendering costs for such lights, the shadowmap
atlas caches shadowmaps from frame to frame, down to the granularity of
individual cubemap faces, if no moving objects are present in the shadowmap.
Lights in Tesseract usually only affect static world geometry, at least when
individual cubemap faces are considered, so the majority of shadowmapped lights
are not more expensive than unshadowed lights, adding only the cost of the
shadowmap lookup and filtering itself. To further optimize the rendering of
shadows for static geometry, for each frustum of each light, an optimal mesh is
generated of all triangles contained only within that frustum and omitting all
backfacing triangles. To avoid moving textures around within the atlas, cached
shadowmaps attempt to retain their placement from the previous frame within the
atlas. To combat fragmentation, if the atlas becomes overly full, cached
shadowmaps are occasionally evicted from a quadrant window of the atlas that
progresses through the atlas from frame to frame.

### Deferred shading and the g-buffer

After evaluating many alternatives, given the small range of materials used in
Tesseract maps, it was decided that deferred shading, in contrast to competing
methods such as light pre-pass or light-indexed, was the most sensible method
for the actual shading/lighting step. Deferred shading provides other benefits
such as easy blending of materials in the g-buffer before the actual shading
step takes place.

Further, by use of tiled approaches to deferred shading, the cost of sampling
the g-buffer can be largely amortized, to the extent that Tesseract's renderer
is, in fact, compute bound by the cost of evaluating the actual per-light
lighting equation on lights that pass culling/rejection tests, rather than bound
by bandwidth or culling costs as other deferred renderers and related research
claim to be.

For further information about the trade-offs involved in various deferred
rendering schemes, see

http://c0de517e.blogspot.com/2011/01/mythbuster-deferred-rendering.html

or

http://software.intel.com/en-us/articles/deferred-rendering-for-current-and-future-rendering-pipelines

or

http://aras-p.info/blog/2012/03/27/tiled-forward-shading-links/

Tesseract breaks the screen up into a grid of 10x10 tiles aligned to pixel group
boundaries. Lights are then inserted into per-tile lists by computing a 2D
bounding box of affected tiles. Finally, lights are batched into groups of at
most 8 lights of equivalent type (shadowed or unshadowed, point or spot light)
which are then evaluated per-tile in a single draw call. As many calls to the
tile shader are made as necessary to exhaust all the lights in the per-tile
list. Other lighting effects such as sunlight, ambient lighting, or global
illumination is also optionally applied by the tile shader.

It was found that beyond coarse light culling and per-tile bucketing, more
complicated schemes in the fragment shader for culling lights, possibly
involving dynamic light lists, yield little to no actual speedups when measured
against simpler rejection tests that involve static uniforms. The bandwidth
costs of accessing dynamic light lists tend to actually exceed the costs of
accessing the g-buffer in a tiled renderer thus motivating a simpler tile
shader. As even using uniform buffers to store light parameters imposes a cost
similar to a texture access, it is strongly preferred to use statically indexed
uniforms to supply light parameters to minimize the cost of iterating through
light parameters in the tile shader.

For information about computing accurate screen-space bounding rectangles for
point light sources, see:

http://www.terathon.com/code/scissor.html

The actual g-buffer is composed a depth24-stencil8 depth buffer, an RGBA8 with
diffuse/albedo in RGB and specular strength in A, an RGBA8 with world-space
normal in RGB and either a scalar glow value or an alpha transparency or a
multi-purpose anti-aliasing mask/depth hash value in A. When rendering
transparent objects, an extra packed-HDR (or RGBA8 if required) texture is used
with additive/emissive light in RGB. On some platforms, a further RGBA8 texture
is used to store a piece-wise encoded linear depth where directly accessing from
the depth buffer texture is either slow or buggy. RGBA8 textures are used for
all layers of the g-buffer both to support older GPUs that can't accept multiple
render targets of varying formats and because RGBA8 textures provide a good
trade-off between size and encoding flexibility.

World-space RGB8 normals are chosen as they are both temporally stable
(no frame-to-frame jitter artifacts) and require almost no encode/decode costs
like other eye-space normal encodings might. The additive/emissive layer allows
for easy handling of environment maps or reflection effects. Overall, this
layout provides a reasonably compact g-buffer while handling the range of
materials used by Tesseract maps. For more information about g-buffer normal
encodings, see:

http://aras-p.info/texts/CompactNormalStorage.html

### Mesh rendering

Where possible, Tesseract utilizes support for half-precision floating point in
modern GPUs to reduce the memory footprint of mesh vertex buffers. Instead of
the usual tangent and bitangent representation, Tesseract utilizes quaternion
tangent frames (QTangents) for compressed tangent-space specification of mesh
triangles, which combine well with the existing use of dual-quaternion skinning
for animated meshes. For more information about QTangents, see:

http://www.crytek.com/cryengine/presentations/spherical-skinning-with-dual-quaternions-and-qtangents

### Decal rendering

Before final shading, any decaling effects are applied to the scene by blending
into the g-buffer.

### Material shading/Light accumulation

The shading is evaluated into a light accumulation buffer, containing the final
shaded result, that preferably uses the R11G11B10F packed floating-point format.
When the GPU hardware does not support packed float format or is otherwise buggy
(as observed on some older AMD GPUs that do not properly implement blending of
packed float format render targets), a fallback RGB10 fixed-point format is used
that is scaled to a 0..2 range to allow some overbright lighting and still
provide somewhat better precision than an LDR RGB8 format. For more information
about the packed floating-point format and its limitations, see:

http://www.opengl.org/registry/specs/EXT/packed_float.txt

Linear-space lighting and sRGB textures have also been avoided here because,
during experimenting, it was found they unavoidably produce lighting values that
quantize poorly in these lower precision formats, and the bandwidth cost of
higher-precision formats was ultimately not worth the perceived benefits. The
gamma-space lighting curves and values are well understood by Sauerbraten
mappers, providing for better fill lighting due to softer/less harsh lighting
falloff, interoperating better with a wealth of pre-existing textures optimized
to look appealing under gamma-space lighting, and producing fewer banding
artifacts with lower-precision HDR texture formats.

While the lighting thus still happens in gamma-space, overbright lighting values
are never the less supported and utilized, motivating later tonemapping and
bloom steps.

For more information about the trade-offs involved in working in gamma-space vs.
linear-space, see http://filmicgames.com/archives/299

### Screen-space ambient obscurance

To help break up the monotony of those indoor areas of a map that may rely on
ambient lighting and to help reduce the burden of requiring lots of point lights
to provide contrast in such places, Tesseract implements a form of SSAO.

After the g-buffer has been filled, but before the shading step, the depth
buffer is downscaled to half-resolution and SSAO is computed, utilizing both the
downscaled depth and the normal layer of the g-buffer, into a another buffer
packing both the noisy/unfiltered obscurance value and a copy of the depth in
each texel. This buffer is then bilaterally filtered, efficiently sampling both
the obscurance and depths in a single tap due to the aforementioned packing
scheme. The final resulting buffer is then used to affect sunlight and ambient
lighting in the deferred shading step.

In particular, Tesseract makes use of the Alchemy Screen-Space Ambient
Obscurance algorithm detailed here:

http://graphics.cs.williams.edu/papers/AlchemyHPG11/

Tesseract further incorpates improvements to the algorithm suggested here:

http://graphics.cs.williams.edu/papers/SAOHPG12/

### Global illumination

One of the primary motivations for including global illumination in Tesseract
was not so much to increase visual quality, but instead to actually increase
performance. While Tesseract can support a large number of shadowed lights,
eventually mappers with the best of intentions can defeat the best of engines.
So having some form of indirect/bounced lighting allows for light to get to
normally dark corners in a map that would otherwise require a lot of "fill"
lights to brighten them up or otherwise rely on ugly/flat ambient lighting.
Tesseract provides a form of diffuse global illumination for the map's global,
directional sunlight that can thus help to brighten up maps, without requiring a
lot of point light entities, so long as a mapper is careful to allow sunlight
into the map interior.

Diffuse global illumination is computed only for sunlight using the Radiance
Hints algorithm, which is similar to but distinct from Light Propagation
Volumes. First, a reflective shadowmap is computed for the scene from the sun's
perspective, storing both the depth and reflected surface color for any surface
the sunlight directly hits. Then using a particular random sampling scheme, the
reflective shadowmap is gathered into a set of RGBA8 cascaded 3D textures
storing low-order spherical harmonics. 3D textures are used for both Radiance
Hints and LPV algorithms as they allow for cheap trilinear filtering of the
spherical harmonics. However, Radiance Hints still differs from the LPV approach
in that it gathers numerous samples from the reflective shadowmap in one shading
pass, rather than injecting seed values into a 3D grid and iteratively refining
it, offering some performance and simplicity advantages for the case of
single-bounce diffuse global illumination.

During this process, an ambient occlusion term is also computed beyond what is
detailed by the basic Radiance Hints algorithm. Where possible, these 3D
textures are cached from frame to frame. These cascaded 3D textures are then
sampled in the shading step to provide both the sunlight global illumination
effect as well as using the ambient obscurance term to implement an atmospheric/
skylight effect.

For further information on Radiance Hints, see "Real-Time Diffuse Global
Illumination Using Radiance Hints" at

http://graphics.cs.aueb.gr/graphics/docs/papers/RadianceHintsPreprint.pdf

or

http://graphics.cs.aueb.gr/graphics/research_illumination.html

### Transparency, reflection, and refraction

Sauerbraten supported an efficient alpha material for world geometry, where
first only the depth of world geometry was rendered, and then finally shading of
the world geometry was rendered with alpha-blending enabled. This allowed only
the first layer, and optionally before that a back-facing layer, to be rendered
cheaply with no depth-sorting involved and is essentially a limited and cheaper
form of the more general-purpose depth-peeling approach. This was sufficient for
making props like windows or similar glass structures in levels by mappers.
Though there is the drawback that transparent layers can't be seen behind other
transparent layers, when used in moderation this drawback is not debilitating.

Tesseract expands upon this notion by shading transparent geometry in a separate
later pass from opaque geometry, though both are accumulated into the light
accumulation buffer. This has both the above-mentioned benefits, as well as
allowing transparencies to be easily shadowed and lit just like any other opaque
geometry and avoiding the need for a separate forward-renderer implementation.
Because transparent geometry is first rendered into the g-buffer as if it were
opaque, there is no need to do a prior depth-only rendering pass to isolate the
front-most transparency layer like in Sauerbraten. Careful stenciling and
scissoring is used to limit the actual shading step to only the necessary screen
pixels that will have transparent geometry blended over them. The A channel in
the normal layer of the g-buffer is used to store the alpha transparency value
to control the blending output of this shading step.

This separate shading pass for transparent geometry also allows the light
accumulation buffer from the previous opaque geometry pass to be easily
resampled for screen-space reflection and refraction effects on materials
like distorted glass or water, providing for a greater range of reflective and
refractive materials than Sauerbraten was previously capable of. The emissive
layer of the g-buffer is used for handling the refractive/reflective component
of a material's shading.

Refraction effects are done by sampling the light accumulation buffer with added
distortion, limited by a mask of refracting surfaces to control bleed-in of
things outside the refraction area. For more information about the refraction
mask technique, see:

http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter19.html

Reflection poses some difficulty since a separate render pass for every
reflecting plane was no longer desired and would anyway be too expensive given
the heavyweight deferred shading pipeline. A screen-space ray marching approach
is instead used that in a small number of fixed steps walks through the depth
buffer until it hits a surface and then samples the light accumulation buffer at
this location. Some care is needed to fade out the reflections when either
looking directly at the reflecting surface or when it might otherwise sample
outside of screen borders or valid reflection boundaries in general. This
approach has some potential artifacts when objects are floating above reflective
surfaces, but on the other hand allows any number of reflective objects in the
scene without requiring a separate render pass for every distinct reflection
plane in the scene. For materials where screen-space reflections are inadequate,
environment maps are instead used.

For further information on screen-space reflections, see

http://www.crytek.com/cryengine/presentations/secrets-of-cryengine-3-graphics-technology

or

http://www.gamedev.net/blog/1323/entry-2254101-real-time-local-reflections/

### Particle rendering

Before the tonemapping step, Tesseract does particle rendering. Unlike in
Sauerbraten, the depth buffer and shading results are more easily available
without having to read back the frame buffer or using special-cased kluges to
avoid such read-backs, making effects like soft or refractive particles cheaper
and simpler to implement.

### Tonemapping and bloom

The light accumulation buffer is first quickly downscaled to approximately
512x512. A high-pass filter is run over this and then separably blurred to
yield a bloom buffer that will be added to the lighting. This downscaled buffer
(the non-blurred one) is also converted to luma values and iteratively reduced
down to 1x1 to compute an average luma for the scene. This average luma is
slowly accumulated into a further 1x1 texture that allows for the scene's
brightness key to gradually adjust to changing viewing conditions. This
accumulated average luma is fed back (via a vertex texture fetch in the
tonemapping shader) into a tonemapping pass which maps the lighting into a
displayable range. Note that the gamma-space lighting is converted temporarily
into linear-space before tonemapping is applied and then converted back to
gamma-space.

To better preserve color tones and in contrast to tonemapping operators that
unfortunately tend to "greyify" a scene such as filmic tonemapping, Tesseract
uses a simpler "photographic" tonemapping operator suggested by Emil Persson
a.k.a. Humus, but applied to luma. See:

http://beyond3d.com/showthread.php?t=60907

or

http://beyond3d.com/showthread.php?t=52747

For more information about the trade-offs involved in various tonemapping
operators, see:

http://filmicgames.com/archives/category/tonemapping

or

http://mynameismjp.wordpress.com/2010/04/30/a-closer-look-at-tone-mapping/

### Generic post-processing

Before the final anti-aliasing and/or upscale step, any generic post-processing
effects are applied. Currently this stage is not extensively utilized.

### Anti-aliasing

In contrast to Sauerbraten's forward renderer, Tesseract's performance is
strongly impacted by resolution. Many schemes were evaluated for reducing
shading costs, such as inferred lighting or interleaved rendering, but
ultimately they were more complicated and no more performant or visually
pleasing than simply rendering at reduced resolution and anti-aliasing the
result with a final upscale to desktop resolution. Since Tesseract relies upon
deferred shading, simply using MSAA by itself does not provide adequate
performance due to increasing memory bandwidth usage from large multisampled
g-buffer textures, though Tesseract does, in fact, implement stand-alone MSAA in
spite of deferred shading. To this end, Tesseract provides several forms of
post-processing-centric anti-aliasing, though mostly in the service of
implementing one particular post-process anti-aliasing algorithm, Enhanced
Subpixel Morphological Anti-Aliasing by Jorge Jiminez et al, otherwise known as
SMAA.

The baseline SMAA 1x algorithm provides morphological anti-aliasing utilizing
only the output color buffer. While this algorithm is an improvement over
competitors such as FXAA, it still suffers from some temporal instability
visible as frame-to-frame jitter/swim. To combat this, Tesseract implements
temporal anti-aliasing that combines with SMAA to provide the SMAA T2x mode.
The SMAA T2x mode, and temporal anti-aliasing in general, however, are often
inadequate when things move quickly on-screen. Temporal anti-aliasing reprojects
the rendering output of prior frames onto the current frame, and when the scene
changes quickly, this is often not possible, so the temporal anti-aliasing fails
to anti-alias in such cases. The A channel of the g-buffer's normal layer is
used to provide a mask of all pixels belonging to moving objects in the scene,
as distinguished from static world geometry, instead of requiring a more costly
velocity buffer. Ultimately, only static geometry that is subject only to
camera-relative movement participates in the temporal anti-aliasing which allows
cheap computation of per-pixel velocity vectors from the global camera
transforms without requiring storing object velocities.

To overcome the particular movement limitations of temporal anti-aliasing, SMAA
also provides several modes that combine with multisample anti-aliasing, SMAA
S2x and SMAA 4x, utilizing 2x spatial multisampling to provide temporal
stability. SMAA 4x further combines temporal anti-aliasing and 2x multisample
anti-aliasing with the baseline morphological anti-aliasing to provide a level
of post-process anti-aliasing that can rival MSAA 8x modes while using far less
bandwidth (only requiring 2x MSAA textures) and being much faster. 

Overall, SMAA gracefully scales up and down both in terms of performance and
visual quality according to the user's tastes with its ability to incorporate
all these disparate anti-aliasing methods, and while still interacting well with
deferred shading. For more information about SMAA, see:

http://www.iryoku.com/smaa/

and also for further improvements recently suggested by Crytek see:

http://www.crytek.com/cryengine/presentations/cryengine-3-graphic-gems

Tesseract's deferred MSAA implementation renders into multisampled g-buffer and
light accumulation textures. Before shading, an edge detection pass is run using
information contained in the normal/depth hash layer of the g-buffer to fill the
stencil buffer with an edge mask. The depth hash value, stored in the A channel
of the normal layer of the g-buffer, is simply an 8 bit hash combining
information about linear depth and material id that when combined with the world
space normal stored in the RGB channels of this same texture provides reasonable
and cheap determination of pixels that only very occasionally mispredicts edges.
Single-sample shading is evaluated at internal/non-edge pixels, and multisample
shading is evaluated at edge pixels. The tonemapping pass is able to run before
the MSAA resolve to properly support the multisampled SMAA modes; however, this
is not done by default for stand-alone MSAA usage as it can decrease performance
for mostly imperceptible benefits in quality.

For more information about implementing MSAA in combination with deferred
shading, see:

http://software.intel.com/en-us/articles/deferred-rendering-for-current-and-future-rendering-pipelines

As a final low-quality but highly performant backup and comparison basis,
Tesseract provides an implementation of FXAA, though the baseline SMAA 1x is
ultimately preferred.

For higher quality upscaling of the anti-aliased result than the usual linear
filtering, Tesseract also provides a bicubic filter such as used for upscaling
video. This can alleviate some blurring a linear filter would otherwise cause.
For more information about fast bicubic filtering, see:

http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter20.html

### Further information

Homepage - Tesseract, http://tesseract.gg
Developer - Lee Salzman, http://sauerbraten.org/lee/

Last revised November 7, 2013

