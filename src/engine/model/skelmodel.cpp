
#include "engine.h"

#include "interface/console.h"
#include "interface/control.h"

#include "render/radiancehints.h"
#include "render/rendergl.h"
#include "render/rendermodel.h"

#include "world/physics.h"

#include "ragdoll.h"
#include "animmodel.h"
#include "skelmodel.h"

VARP(gpuskel, 0, 1, 1);

VAR(maxskelanimdata, 1, 192, 0);
