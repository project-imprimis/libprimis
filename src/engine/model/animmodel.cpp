
#include "engine.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"

#include "world/physics.h"

#include "ragdoll.h"
#include "animmodel.h"

//animmodel
VARP(fullbrightmodels, 0, 0, 200);
VAR(testtags, 0, 0, 1);
VARF(dbgcolmesh, 0, 0, 1,
{
    extern void cleanupmodels();
    cleanupmodels();
});

hashnameset<animmodel::meshgroup *> animmodel::meshgroups;
int animmodel::intersectresult = -1,
    animmodel::intersectmode = 0;
float animmodel::intersectdist = 0,
      animmodel::intersectscale = 1;
bool animmodel::enabletc = false,
     animmodel::enabletangents = false,
     animmodel::enablebones = false,
     animmodel::enablecullface = true,
     animmodel::enabledepthoffset = false;
float animmodel::sizescale = 1;
vec4 animmodel::colorscale(1, 1, 1, 1);
GLuint animmodel::lastvbuf = 0,
       animmodel::lasttcbuf = 0,
       animmodel::lastxbuf = 0,
       animmodel::lastbbuf = 0,
       animmodel::lastebuf = 0;
Texture *animmodel::lasttex = NULL,
        *animmodel::lastdecal = NULL,
        *animmodel::lastmasks = NULL,
        *animmodel::lastnormalmap = NULL;
int animmodel::matrixpos = 0;
matrix4 animmodel::matrixstack[64];

hashtable<animmodel::shaderparams, animmodel::shaderparamskey> animmodel::shaderparamskey::keys;
int animmodel::shaderparamskey::firstversion = 0,
    animmodel::shaderparamskey::lastversion = 1;
