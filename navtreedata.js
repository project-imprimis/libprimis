/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Libprimis", "index.html", [
    [ "Documentation", "index.html#autotoc_md2", null ],
    [ "An Open Source Standalone Engine", "index.html#autotoc_md3", null ],
    [ "Key Features", "index.html#autotoc_md4", null ],
    [ "Quick Windows Install Instructions", "index.html#autotoc_md5", null ],
    [ "Quick Linux Install Instructions", "index.html#autotoc_md6", null ],
    [ "Join Us", "index.html#autotoc_md7", null ],
    [ "The Libprimis Engine", "md_engine.html", [
      [ "Preface", "md_engine.html#autotoc_md9", null ],
      [ "1. Standards", "md_engine.html#autotoc_md10", [
        [ "1.1 Coding Standards", "md_engine.html#autotoc_md11", [
          [ "1.1.1 This File", "md_engine.html#autotoc_md12", null ],
          [ "1.1.2 C/C++ Standards", "md_engine.html#autotoc_md13", [
            [ "Symbol Names", "md_engine.html#autotoc_md14", null ],
            [ "Enums", "md_engine.html#autotoc_md15", null ],
            [ "Objects", "md_engine.html#autotoc_md16", null ],
            [ "`for` loops", "md_engine.html#autotoc_md17", null ],
            [ "Indentation and Bracing", "md_engine.html#autotoc_md18", null ],
            [ "Spacing", "md_engine.html#autotoc_md19", null ]
          ] ]
        ] ],
        [ "1.2 Default Paths & Libraries", "md_engine.html#autotoc_md20", [
          [ "1.2.1 Paths", "md_engine.html#autotoc_md21", null ],
          [ "1.2.2 Libraries", "md_engine.html#autotoc_md22", null ]
        ] ],
        [ "1.3 Conventions and Units", "md_engine.html#autotoc_md23", [
          [ "1.3.1 Distances", "md_engine.html#autotoc_md24", null ],
          [ "1.3.2 Coordinates", "md_engine.html#autotoc_md25", null ],
          [ "1.3.3 Mathematical Notes", "md_engine.html#autotoc_md26", null ],
          [ "1.3.4 Colors", "md_engine.html#autotoc_md27", null ]
        ] ],
        [ "1.4 Program Structure", "md_engine.html#autotoc_md28", null ],
        [ "1.5 File Structure", "md_engine.html#autotoc_md29", [
          [ "1.5.1 `/engine` files", "md_engine.html#autotoc_md30", [
            [ "`/interface`", "md_engine.html#autotoc_md31", null ],
            [ "`/model`", "md_engine.html#autotoc_md32", null ],
            [ "`/render`", "md_engine.html#autotoc_md33", null ],
            [ "`/world`", "md_engine.html#autotoc_md34", null ]
          ] ]
        ] ],
        [ "1.6 System Contexts", "md_engine.html#autotoc_md35", [
          [ "1.6.1 Operating System", "md_engine.html#autotoc_md36", null ],
          [ "1.6.2 Hardware", "md_engine.html#autotoc_md37", [
            [ "Performance Considerations", "md_engine.html#autotoc_md38", null ]
          ] ]
        ] ]
      ] ],
      [ "2. World", "md_engine.html#autotoc_md39", [
        [ "2.1 Octree Geometry", "md_engine.html#autotoc_md40", [
          [ "2.1.1 Octree Data Structure & Cube Geometry", "md_engine.html#autotoc_md41", null ],
          [ "2.1.2 Child Nodes", "md_engine.html#autotoc_md42", null ],
          [ "2.1.3 World Root", "md_engine.html#autotoc_md43", [
            [ "Map Expansion", "md_engine.html#autotoc_md44", null ]
          ] ],
          [ "2.1.4 Cube Manipulation", "md_engine.html#autotoc_md45", null ],
          [ "2.1.5 Remipping and Subdivision", "md_engine.html#autotoc_md46", [
            [ "Subdivision", "md_engine.html#autotoc_md47", null ],
            [ "Remipping", "md_engine.html#autotoc_md48", null ],
            [ "Commands", "md_engine.html#autotoc_md49", null ]
          ] ]
        ] ],
        [ "2.2 Materials", "md_engine.html#autotoc_md50", [
          [ "2.2.1 Air", "md_engine.html#autotoc_md51", null ],
          [ "2.2.2 Water", "md_engine.html#autotoc_md52", [
            [ "Commands", "md_engine.html#autotoc_md53", null ]
          ] ],
          [ "2.2.3 Glass", "md_engine.html#autotoc_md54", [
            [ "Commands", "md_engine.html#autotoc_md55", null ]
          ] ],
          [ "2.2.5 Clip", "md_engine.html#autotoc_md56", null ],
          [ "2.2.6 Noclip", "md_engine.html#autotoc_md57", null ],
          [ "2.2.7 Death", "md_engine.html#autotoc_md58", null ],
          [ "2.2.8 No GI", "md_engine.html#autotoc_md59", null ],
          [ "2.2.9 Alpha", "md_engine.html#autotoc_md60", null ]
        ] ],
        [ "2.3 Textures", "md_engine.html#autotoc_md61", [
          [ "2.3.1 Texture Slots", "md_engine.html#autotoc_md63", null ],
          [ "2.3.2 Virtual Slots", "md_engine.html#autotoc_md64", null ],
          [ "2.3.3 Texture Projection", "md_engine.html#autotoc_md65", null ],
          [ "2.3.4 Texture Slot Properties", "md_engine.html#autotoc_md66", [
            [ "Commands", "md_engine.html#autotoc_md62", null ],
            [ "`texalpha <front> <back>`, `valpha <front> <back>`: transparency modifiers", "md_engine.html#autotoc_md67", null ],
            [ "`texangle <index>`, `vangle <index>` : fine texture rotation", "md_engine.html#autotoc_md68", null ],
            [ "`texcolor <R> <G> <B>`, `vcolor <R> <G> <B>`: texture tinting", "md_engine.html#autotoc_md69", null ],
            [ "`texoffset <x> <y>`, `voffset <x> <y>`: translational texture offset", "md_engine.html#autotoc_md70", null ],
            [ "`texrefract <scale> <R> <G> <B>`, `vrefract <scale> <R> <G> <B>`: refract", "md_engine.html#autotoc_md71", null ],
            [ "`texrotate <index>` `vrotate <index>`: coarse texture rotations/transforms", "md_engine.html#autotoc_md72", null ],
            [ "`texscale <scale>`, `vscale <scale>`: texture scaling", "md_engine.html#autotoc_md73", null ],
            [ "`texscroll <x> <y>`, `vscroll <x> <y>`: time-varying translational offset", "md_engine.html#autotoc_md74", null ]
          ] ]
        ] ],
        [ "2.4 Global Properties", "md_engine.html#autotoc_md75", [
          [ "2.4.1 Sunlight", "md_engine.html#autotoc_md76", [
            [ "Commands", "md_engine.html#autotoc_md77", null ]
          ] ],
          [ "2.4.2 Fog", "md_engine.html#autotoc_md78", [
            [ "Commands", "md_engine.html#autotoc_md79", null ]
          ] ],
          [ "2.4.3 Ambient Lighting", "md_engine.html#autotoc_md80", [
            [ "Commands", "md_engine.html#autotoc_md81", null ]
          ] ],
          [ "2.4.4 Skybox", "md_engine.html#autotoc_md82", [
            [ "Commands:", "md_engine.html#autotoc_md83", null ]
          ] ],
          [ "2.4.5 Cloud Layer", "md_engine.html#autotoc_md84", [
            [ "Commands:", "md_engine.html#autotoc_md85", null ]
          ] ],
          [ "2.4.6 Atmo", "md_engine.html#autotoc_md86", [
            [ "Commands:", "md_engine.html#autotoc_md87", null ]
          ] ]
        ] ],
        [ "2.5 World Level Format", "md_engine.html#autotoc_md88", [
          [ "2.5.1 Map Format Summary", "md_engine.html#autotoc_md89", null ],
          [ "2.5.2 Map Header", "md_engine.html#autotoc_md90", null ],
          [ "2.5.3 Map Variables", "md_engine.html#autotoc_md91", null ],
          [ "2.5.4 Map Entities", "md_engine.html#autotoc_md92", null ],
          [ "2.5.5 Map Vslots", "md_engine.html#autotoc_md93", null ]
        ] ]
      ] ],
      [ "3. Entities", "md_engine.html#autotoc_md94", [
        [ "3.1 Static Entities", "md_engine.html#autotoc_md95", [
          [ "3.1.1 Lights", "md_engine.html#autotoc_md96", [
            [ "Attributes", "md_engine.html#autotoc_md97", null ]
          ] ],
          [ "3.1.2 Mapmodels", "md_engine.html#autotoc_md98", [
            [ "Attributes", "md_engine.html#autotoc_md99", null ]
          ] ],
          [ "3.1.3 Playerstarts", "md_engine.html#autotoc_md100", [
            [ "Attributes", "md_engine.html#autotoc_md101", null ]
          ] ],
          [ "3.1.4 Particles", "md_engine.html#autotoc_md102", [
            [ "Attributes", "md_engine.html#autotoc_md103", null ]
          ] ],
          [ "3.1.5 Sound", "md_engine.html#autotoc_md104", [
            [ "Attributes", "md_engine.html#autotoc_md105", null ]
          ] ],
          [ "3.1.6 Spotlights", "md_engine.html#autotoc_md106", [
            [ "Attributes", "md_engine.html#autotoc_md107", null ]
          ] ],
          [ "3.1.7 Decals", "md_engine.html#autotoc_md108", [
            [ "Attributes", "md_engine.html#autotoc_md109", null ]
          ] ]
        ] ],
        [ "3.2 Projectiles", "md_engine.html#autotoc_md110", [
          [ "3.2.1 Projectile Attribute Overview", "md_engine.html#autotoc_md111", null ],
          [ "3.2.2 Projectile Vector Attributes", "md_engine.html#autotoc_md112", [
            [ "`dir`ection", "md_engine.html#autotoc_md113", null ],
            [ "l`o`cation", "md_engine.html#autotoc_md114", null ],
            [ "`from`", "md_engine.html#autotoc_md115", null ],
            [ "`to`", "md_engine.html#autotoc_md116", null ],
            [ "`offset`", "md_engine.html#autotoc_md117", null ]
          ] ],
          [ "3.2.3 Other Projectile Attributes", "md_engine.html#autotoc_md118", [
            [ "`speed`", "md_engine.html#autotoc_md119", null ],
            [ "`owner`", "md_engine.html#autotoc_md120", null ],
            [ "`atk`", "md_engine.html#autotoc_md121", null ],
            [ "`local`", "md_engine.html#autotoc_md122", null ],
            [ "`offsetmillis`", "md_engine.html#autotoc_md123", null ],
            [ "`id`", "md_engine.html#autotoc_md124", null ]
          ] ],
          [ "3.2.4 Projectile Time Evolution", "md_engine.html#autotoc_md125", null ]
        ] ],
        [ "3.3 Bouncers", "md_engine.html#autotoc_md126", null ],
        [ "3.4 Stains", "md_engine.html#autotoc_md127", [
          [ "3.4.1 Stain Objects", "md_engine.html#autotoc_md128", null ],
          [ "3.4.2 Stain Settings", "md_engine.html#autotoc_md129", null ]
        ] ],
        [ "3.5 Particles", "md_engine.html#autotoc_md130", [
          [ "3.5.1 Particle Types", "md_engine.html#autotoc_md131", null ],
          [ "3.5.2 Particle Properties", "md_engine.html#autotoc_md132", null ],
          [ "3.5.3 Pointlike Particles", "md_engine.html#autotoc_md133", null ],
          [ "3.5.4 Tape Particles", "md_engine.html#autotoc_md134", null ],
          [ "3.5.5 Trail Particles", "md_engine.html#autotoc_md135", null ],
          [ "3.5.6 Text and Textup Particles", "md_engine.html#autotoc_md136", null ],
          [ "3.5.7 Meter and Metervs Particles", "md_engine.html#autotoc_md137", null ],
          [ "3.5.8 Fireball Particles", "md_engine.html#autotoc_md138", null ]
        ] ],
        [ "3.6 Physics", "md_engine.html#autotoc_md139", [
          [ "3.6.1 Physent Properties", "md_engine.html#autotoc_md140", null ],
          [ "3.6.2 Collision", "md_engine.html#autotoc_md141", null ]
        ] ]
      ] ],
      [ "5 Render", "md_engine.html#autotoc_md142", [
        [ "5.1 Texturing", "md_engine.html#autotoc_md143", [
          [ "5.1.1 Shader Overview", "md_engine.html#autotoc_md144", [
            [ "Diffuse mapping (`stdworld`)", "md_engine.html#autotoc_md145", null ],
            [ "Normal mapping (`bumpworld`)", "md_engine.html#autotoc_md146", null ],
            [ "Specular highlights (`specworld`)", "md_engine.html#autotoc_md147", null ],
            [ "Specular mapping (`specmapworld`)", "md_engine.html#autotoc_md148", null ],
            [ "Parallax mapping (`bumpparallaxworld`)", "md_engine.html#autotoc_md149", null ],
            [ "Triplanar mapping (`triplanarworld`)", "md_engine.html#autotoc_md150", null ],
            [ "Triplanar detail mapping (`triplanardetailworld`)", "md_engine.html#autotoc_md151", null ],
            [ "Glow mapping (`glowworld`)", "md_engine.html#autotoc_md152", null ]
          ] ]
        ] ],
        [ "5.2 Lighting", "md_engine.html#autotoc_md153", [
          [ "5.2.1 Shadow Atlas", "md_engine.html#autotoc_md154", null ],
          [ "5.2.2 Shadow Map", "md_engine.html#autotoc_md155", null ],
          [ "5.2.3 Shadow Map Filtering", "md_engine.html#autotoc_md156", null ],
          [ "5.2.4 Cascaded Shadow Maps (CSM)", "md_engine.html#autotoc_md157", null ],
          [ "5.2.5 Global Illumination (GI)", "md_engine.html#autotoc_md158", [
            [ "The Reflective Shadow Map (RSM)", "md_engine.html#autotoc_md159", null ]
          ] ]
        ] ],
        [ "5.3 Transparency", "md_engine.html#autotoc_md160", [
          [ "5.3.1 Transparency Stenciling", "md_engine.html#autotoc_md161", null ],
          [ "5.3.2 Backface Transparency", "md_engine.html#autotoc_md162", null ]
        ] ],
        [ "5.4 Screenspace Posteffects", "md_engine.html#autotoc_md163", [
          [ "5.4.1 Screenspace Reflection (SSR)", "md_engine.html#autotoc_md164", null ],
          [ "5.4.2 Screenspace Ambient Occlusion (SSAO)", "md_engine.html#autotoc_md165", null ],
          [ "5.4.3 Screenspace Refraction", "md_engine.html#autotoc_md166", null ]
        ] ],
        [ "5.5 Antialiasing", "md_engine.html#autotoc_md167", [
          [ "5.5.1 Supersample Antialiasing (SSAA)", "md_engine.html#autotoc_md168", null ],
          [ "5.5.2 Multisample Antialiasing (MSAA)", "md_engine.html#autotoc_md169", null ],
          [ "5.5.3 Fast Approximate Antialiasing (FXAA)", "md_engine.html#autotoc_md170", null ],
          [ "5.5.4 Temporal Quincunx Antialiasing (TQAA)", "md_engine.html#autotoc_md171", null ],
          [ "5.5.5 Subpixel Morphological Antialiasing (SMAA)", "md_engine.html#autotoc_md172", null ]
        ] ]
      ] ],
      [ "6 Actors and Models", "md_engine.html#autotoc_md173", [
        [ "6.1 Actor Objects", "md_engine.html#autotoc_md174", [
          [ "6.1.1 Actor Entity Properties", "md_engine.html#autotoc_md175", null ]
        ] ],
        [ "6.2 Models", "md_engine.html#autotoc_md176", [
          [ "6.2.1 Model Format Overview", "md_engine.html#autotoc_md177", [
            [ "OBJ (Wavefront)", "md_engine.html#autotoc_md178", null ],
            [ "MD5 (Doom 3)", "md_engine.html#autotoc_md179", null ]
          ] ],
          [ "6.2.2 Basic Model Commands", "md_engine.html#autotoc_md180", null ],
          [ "6.2.3 Animated Model Commands", "md_engine.html#autotoc_md181", null ]
        ] ]
      ] ],
      [ "7. User and System Interfaces", "md_engine.html#autotoc_md182", [
        [ "7.4 Scripting", "md_engine.html#autotoc_md183", [
          [ "7.4.1 Cubescript Semantics", "md_engine.html#autotoc_md184", [
            [ "Lazy Execution `[]`", "md_engine.html#autotoc_md185", null ],
            [ "Eager Execution `()`", "md_engine.html#autotoc_md186", null ],
            [ "Assignment `=`", "md_engine.html#autotoc_md187", null ],
            [ "Lookup Alias `$`", "md_engine.html#autotoc_md188", null ],
            [ "Literal Substitution `\\`", "md_engine.html#autotoc_md189", null ],
            [ "Comments `//`", "md_engine.html#autotoc_md190", null ]
          ] ],
          [ "7.4.2 Commands", "md_engine.html#autotoc_md191", [
            [ "Inline Commands", "md_engine.html#autotoc_md192", null ],
            [ "Standard Commands", "md_engine.html#autotoc_md193", null ],
            [ "Interpreter return types", "md_engine.html#autotoc_md194", null ],
            [ "Decoding `nargs`", "md_engine.html#autotoc_md195", null ]
          ] ],
          [ "7.4.3 Variables", "md_engine.html#autotoc_md196", [
            [ "Integer Variables `VAR`", "md_engine.html#autotoc_md197", null ],
            [ "Float Variables `FVAR`", "md_engine.html#autotoc_md198", null ],
            [ "Hex Variables `HVAR`", "md_engine.html#autotoc_md199", null ],
            [ "Color Variables `CVAR`", "md_engine.html#autotoc_md200", null ],
            [ "String Variables `SVAR`", "md_engine.html#autotoc_md201", null ],
            [ "Persistent Variables `*VARP`", "md_engine.html#autotoc_md202", null ]
          ] ]
        ] ]
      ] ],
      [ "8 Internal Objects", "md_engine.html#autotoc_md203", [
        [ "8.1 Vector Objects", "md_engine.html#autotoc_md204", null ],
        [ "8.2 Cube Objects", "md_engine.html#autotoc_md207", [
          [ "8.2.1 `cube`", "md_engine.html#autotoc_md208", null ],
          [ "8.2.2 `cubeext`", "md_engine.html#autotoc_md209", null ]
        ] ]
      ] ]
    ] ],
    [ "The Libprimis Interface Specification", "md_interface.html", [
      [ "1. Usage", "md_interface.html#autotoc_md211", [
        [ "1.1 What is an interface", "md_interface.html#autotoc_md212", null ],
        [ "1.2 Augmenting an interface", "md_interface.html#autotoc_md213", null ]
      ] ],
      [ "2. Standard Interfaces", "md_interface.html#autotoc_md214", [
        [ "2.1 Game", "md_interface.html#autotoc_md215", [
          [ "2.1.1 Player", "md_interface.html#autotoc_md216", null ],
          [ "2.1.2 Weapon", "md_interface.html#autotoc_md217", null ],
          [ "2.1.3 Projectile", "md_interface.html#autotoc_md218", null ]
        ] ]
      ] ]
    ] ],
    [ "The Tesseract Renderer", "md_tesseract-renderer.html", [
      [ "The Tesseract Rendering Pipeline", "md_tesseract-renderer.html#autotoc_md220", [
        [ "Platform", "md_tesseract-renderer.html#autotoc_md221", null ],
        [ "Motivation", "md_tesseract-renderer.html#autotoc_md222", null ],
        [ "Shadows", "md_tesseract-renderer.html#autotoc_md223", null ],
        [ "Deferred shading and the g-buffer", "md_tesseract-renderer.html#autotoc_md224", null ],
        [ "Mesh rendering", "md_tesseract-renderer.html#autotoc_md225", null ],
        [ "Decal rendering", "md_tesseract-renderer.html#autotoc_md226", null ],
        [ "Material shading/Light accumulation", "md_tesseract-renderer.html#autotoc_md227", null ],
        [ "Screen-space ambient obscurance", "md_tesseract-renderer.html#autotoc_md228", null ],
        [ "Global illumination", "md_tesseract-renderer.html#autotoc_md229", null ],
        [ "Transparency, reflection, and refraction", "md_tesseract-renderer.html#autotoc_md230", null ],
        [ "Particle rendering", "md_tesseract-renderer.html#autotoc_md231", null ],
        [ "Tonemapping and bloom", "md_tesseract-renderer.html#autotoc_md232", null ],
        [ "Generic post-processing", "md_tesseract-renderer.html#autotoc_md233", null ],
        [ "Anti-aliasing", "md_tesseract-renderer.html#autotoc_md234", null ],
        [ "Further information", "md_tesseract-renderer.html#autotoc_md235", null ]
      ] ]
    ] ],
    [ "The Game Timeline", "md_timeline.html", [
      [ "Phase I: Project Scope", "md_timeline.html#autotoc_md237", null ],
      [ "Phase II: Codebase Preparation", "md_timeline.html#autotoc_md238", null ],
      [ "Phase III: Feature Implementation", "md_timeline.html#autotoc_md239", null ],
      [ "Phase IV: Game Implementation", "md_timeline.html#autotoc_md240", null ],
      [ "Phase V: UX Refinement", "md_timeline.html#autotoc_md241", null ],
      [ "Phase VI: Final Release Prep", "md_timeline.html#autotoc_md242", null ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"iengine_8h.html#a32f3b0225eb75fb287a510e709f8b82e",
"md_engine.html#autotoc_md200",
"structselinfo.html#ac7e357c2f1b3e3baed4761f45d8653eb"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';