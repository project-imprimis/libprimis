#include "game.h"

// "convenience" macros implicitly define:
// e         entity, currently edited ent
// n         int,    index to currently edited ent
#define ADD_IMPLICIT(f) \
{ \
    if(entgroup.empty() && enthover>=0) \
    { \
        entadd(enthover); \
        undonext = (enthover != oldhover); \
        f; \
        entgroup.drop(); \
    } \
    else \
    { \
        f; \
    } \
}

#define ENT_FOCUS_V(i, f, v) \
{ \
    int n = efocus = (i); \
    if(n>=0) \
    { \
        extentity &e = *v[n]; \
        f; \
    } \
}

#define ENT_FOCUS(i, f) \
{ \
    ENT_FOCUS_V(i, f, entities::getents()) \
}

#define ENT_EDIT_V(i, f, v) \
{ \
    ENT_FOCUS_V(i, \
    { \
        int oldtype = e.type; \
        removeentityedit(n);  \
        f; \
        if(oldtype!=e.type) \
        { \
            detachentity(e); \
        } \
        if(e.type!=EngineEnt_Empty) \
        { \
            addentityedit(n); \
            if(oldtype!=e.type) \
            { \
                attachentity(e); \
            } \
        } \
        entities::editent(n, true); \
        clearshadowcache(); \
    }, v); \
}

#define ENT_EDIT(i, f) \
{ \
    ENT_EDIT_V(i, f, entities::getents()) \
}

#define ADD_GROUP(exp) \
{ \
    vector<extentity *> &ents = entities::getents(); \
    for(int i = 0; i < ents.length(); i++) \
    { \
        ENT_FOCUS_V(i, \
        { \
            if(exp) \
            { \
                entadd(n); \
            } \
        }, ents); \
    } \
}

#define GROUP_EDIT_LOOP(f) \
{ \
    vector<extentity *> &ents = entities::getents(); \
    entlooplevel++; \
    int efocusplaceholder = efocus; \
    for(int i = 0; i < entgroup.length(); i++) \
    { \
        ENT_EDIT_V(entgroup[i], f, ents); \
    } \
    efocus = efocusplaceholder; \
    entlooplevel--; \
}

#define GROUP_EDIT_PURE(f) \
{ \
    if(entlooplevel>0) \
    { \
        ENT_EDIT(efocus, f); \
    } \
    else \
    { \
        GROUP_EDIT_LOOP(f); \
        commitchanges(); \
    } \
}

#define GROUP_EDIT_UNDO(f) \
{ \
    makeundoent(); \
    GROUP_EDIT_PURE(f); \
}

#define GROUP_EDIT(f) \
{ \
    ADD_IMPLICIT(GROUP_EDIT_UNDO(f)); \
}

namespace entities
{
    extern void editent(int i, bool local);
    extern void fixentity(extentity &e);
    extern void entradius(extentity &e, bool color);
    extern vector<extentity *> &getents();
}

extern selinfo sel;
extern bool havesel;
int entlooplevel = 0;
int efocus    = -1,
    enthover  = -1,
    entorient = -1,
    oldhover  = -1;
bool undonext = true;

vector<int> outsideents;
int spotlights       = 0,
    volumetriclights = 0,
    nospeclights     = 0;

//=== ent modification functions ===//

void entadd(int id)
{
    undonext = true;
    entgroup.add(id);
}

bool noentedit()
{
    if(!editmode)
    {
        conoutf(Console_Error, "operation only allowed in edit mode");
        return true;
    }
    return !entediting;
}

undoblock *newundoent()
{
    int numents = entgroup.length();
    if(numents <= 0)
    {
        return NULL;
    }
    undoblock *u = (undoblock *)new uchar[sizeof(undoblock) + numents*sizeof(undoent)];
    u->numents = numents;
    undoent *e = (undoent *)(u + 1);
    for(int i = 0; i < entgroup.length(); i++)
    {
        e->i = entgroup[i];
        e->e = *entities::getents()[entgroup[i]];
        e++;
    }
    return u;
}

void makeundoent()
{
    if(!undonext)
    {
        return;
    }
    undonext = false;
    oldhover = enthover;
    undoblock *u = newundoent();
    if(u)
    {
        addundo(u);
    }
}

int findtype(char *what)
{
    for(int i = 0; *getentname(i); i++)
    {
        if(strcmp(what, getentname(i))==0)
        {
            return i;
        }
    }
    conoutf(Console_Error, "unknown entity type \"%s\"", what);
    return EngineEnt_Empty;
}

void entset(char *what, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    if(noentedit())
    {
        return;
    }
    int type = findtype(what);
    if(type != EngineEnt_Empty)
    {
        GROUP_EDIT(e.type=type;
                  e.attr1=*a1;
                  e.attr2=*a2;
                  e.attr3=*a3;
                  e.attr4=*a4;
                  e.attr5=*a5);
    }
}

bool printparticles(extentity &e, char *buf, int len)
{
    switch(e.attr1)
    {
        case 0:
        case 4:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        {
            nformatstring(buf, len, "%s %d %d %d 0x%.3hX %d", getentname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        }
        case 3:
        {
            nformatstring(buf, len, "%s %d %d 0x%.3hX %d %d", getentname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        }
        case 5:
        case 6:
        {
            nformatstring(buf, len, "%s %d %d 0x%.3hX 0x%.3hX %d", getentname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        }
    }
    return false;
}

//prints out attributes of an entity
void printent(extentity &e, char *buf, int len)
{
    switch(e.type)
    {
        case EngineEnt_Particles:
        {
            if(printparticles(e, buf, len))
            {
                return;
            }
            break;
        }
        default:
        {
            break;
        }
    }
    nformatstring(buf, len, "%s %d %d %d %d %d", getentname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
}

//goes through all ents and selects the nearest one
void nearestent()
{
    if(noentedit())
    {
        return;
    }
    int closest = -1;
    float closedist = 1e16f;
    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
    {
        extentity &e = *ents[i];
        if(e.type == EngineEnt_Empty)
        {
            continue;
        }
        float dist = e.o.dist(player->o);
        if(dist < closedist)
        {
            closest = i;
            closedist = dist;
        }
    }
    if(closest >= 0 && entgroup.find(closest) < 0)
    {
        entadd(closest);
    }
}

ICOMMAND(enthavesel,"",  (), ADD_IMPLICIT(intret(entgroup.length())));
ICOMMAND(entselect, "e", (uint *body), if(!noentedit()) ADD_GROUP(e.type != EngineEnt_Empty && entgroup.find(n)<0 && executebool(body)));
ICOMMAND(entloop,   "e", (uint *body), if(!noentedit()) ADD_IMPLICIT(GROUP_EDIT_LOOP(((void)e, execute(body)))));
ICOMMAND(insel,     "",  (), ENT_FOCUS(efocus, intret(pointinsel(sel, e.o))));
ICOMMAND(entget,    "",  (), ENT_FOCUS(efocus, string s; printent(e, s, sizeof(s)); result(s)));
ICOMMAND(entindex,  "",  (), intret(efocus));
COMMAND(entset, "siiiii");
COMMAND(nearestent, "");

void enttype(char *type, int *numargs)
{
    if(*numargs >= 1)
    {
        int typeidx = findtype(type);
        if(typeidx != EngineEnt_Empty)
        {
            GROUP_EDIT(e.type = typeidx);
        }
    }
    else ENT_FOCUS(efocus,
    {
        result(getentname(e.type));
    })
}

//sets one or more entity attributes to the selected entities
void entattr(int *attr, int *val, int *numargs)
{
    if(*numargs >= 2)
    {
        if(*attr >= 0 && *attr <= 4)
            GROUP_EDIT(
                switch(*attr)
                {
                    case 0: e.attr1 = *val; break;
                    case 1: e.attr2 = *val; break;
                    case 2: e.attr3 = *val; break;
                    case 3: e.attr4 = *val; break;
                    case 4: e.attr5 = *val; break;
                }
            );
    }
    else ENT_FOCUS(efocus,
    {
        switch(*attr)
        {
            case 0: intret(e.attr1); break;
            case 1: intret(e.attr2); break;
            case 2: intret(e.attr3); break;
            case 3: intret(e.attr4); break;
            case 4: intret(e.attr5); break;
        }
    });
}

COMMAND(enttype, "sN");
COMMAND(entattr, "iiN");

extern int findentity(int type, int index = 0, int attr1 = -1, int attr2 = -1)
{
    const vector<extentity *> &ents = entities::getents();
    if(index > ents.length())
    {
        index = ents.length();
    }
    else for(int i = index; i<ents.length(); i++)
    {
        extentity &e = *ents[i];
        if(e.type==type && (attr1<0 || e.attr1==attr1) && (attr2<0 || e.attr2==attr2))
        {
            return i;
        }
    }
    for(int j = 0; j < index; ++j)
    {
        extentity &e = *ents[j];
        if(e.type==type && (attr1<0 || e.attr1==attr1) && (attr2<0 || e.attr2==attr2))
        {
            return j;
        }
    }
    return -1;
}

int spawncycle = -1;

void findplayerspawn(dynent *d, int forceent, int tag) // place at random spawn
{
    int pick = forceent;
    if(pick<0)
    {
        int r = randomint(10)+1;
        pick = spawncycle;
        for(int i = 0; i < r; ++i)
        {
            pick = findentity(EngineEnt_Playerstart, pick+1, -1, tag);
            if(pick < 0)
            {
                break;
            }
        }
        if(pick < 0 && tag)
        {
            pick = spawncycle;
            for(int i = 0; i < r; ++i)
            {
                pick = findentity(EngineEnt_Playerstart, pick+1, -1, 0);
                if(pick < 0)
                {
                    break;
                }
            }
        }
        if(pick >= 0)
        {
            spawncycle = pick;
        }
    }
    if(pick>=0)
    {
        const vector<extentity *> &ents = entities::getents();
        d->pitch = 0;
        d->roll = 0;
        for(int attempt = pick;;)
        {
            d->o = ents[attempt]->o;
            d->yaw = ents[attempt]->attr1;
            if(entinmap(d, true))
            {
                break;
            }
            attempt = findentity(EngineEnt_Playerstart, attempt+1, -1, tag);
            if(attempt<0 || attempt==pick)
            {
                d->o = ents[pick]->o;
                d->yaw = ents[pick]->attr1;
                entinmap(d);
                break;
            }
        }
    }
    else
    {
        d->o.x = d->o.y = d->o.z = 0.5f*worldsize;
        d->o.z += 1;
        entinmap(d);
    }
}

static inline void findents(octaentities &oe, int low, int high, bool notspawned, const vec &pos, const vec &invradius, vector<int> &found)
{
    vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < oe.other.length(); i++)
    {
        int id = oe.other[i];
        extentity &e = *ents[id];
        if(  e.type >= low
          && e.type <= high
          && (e.spawned() || notspawned)
          && vec(e.o).sub(pos).mul(invradius).squaredlen() <= 1)
        {
            found.add(id);
        }
    }
}

static inline void findents(cube *c, const ivec &o, int size, const ivec &bo, const ivec &br, int low, int high, bool notspawned, const vec &pos, const vec &invradius, vector<int> &found)
{
    LOOP_OCTA_BOX(o, size, bo, br)
    {
        if(c[i].ext && c[i].ext->ents)
        {
            findents(*c[i].ext->ents, low, high, notspawned, pos, invradius, found);
        }
        if(c[i].children && size > octaentsize)
        {
            ivec co(i, o, size);
            findents(c[i].children, co, size>>1, bo, br, low, high, notspawned, pos, invradius, found);
        }
    }
}

VARF(entediting, 0, 0, 1,
{
    if(!entediting)
    {
        entcancel();
        efocus = enthover = -1;
    }
});

vector<int> entgroup;

//returns origin "o" of selected region
vec getselpos()
{
    vector<extentity *> &ents = entities::getents();
    if(entgroup.length() && ents.inrange(entgroup[0]))
    {
        return ents[entgroup[0]]->o;
    }
    if(ents.inrange(enthover))
    {
        return ents[enthover]->o;
    }
    return vec(sel.o);
}

undoblock *copyundoents(undoblock *u)
{
    entcancel();
    undoent *e = u->ents();
    for(int i = 0; i < u->numents; ++i)
    {
        entadd(e[i].i);
    }
    undoblock *c = newundoent();
    for(int i = 0; i < u->numents; ++i)
    {
        if(e[i].e.type==EngineEnt_Empty)
        {
            entgroup.removeobj(e[i].i);
        }
    }
    return c;
}

void pasteundoent(int idx, const entity &ue)
{
    if(idx < 0 || idx >= maxents)
    {
        return;
    }
    vector<extentity *> &ents = entities::getents();
    while(ents.length() < idx)
    {
        ents.add(entities::newentity())->type = EngineEnt_Empty;
    }
    int efocus = -1;
    ENT_EDIT(idx, (entity &)e = ue);
}

void pasteundoents(undoblock *u)
{
    undoent *ue = u->ents();
    for(int i = 0; i < u->numents; ++i)
    {
        pasteundoent(ue[i].i, ue[i].e);
    }
}

void entflip()
{
    if(noentedit())
    {
        return;
    }
    int d = DIMENSION(sel.orient);
    float mid = sel.s[d]*sel.grid/2+sel.o[d];
    GROUP_EDIT_UNDO(e.o[d] -= (e.o[d]-mid)*2);
}

void entrotate(int *cw)
{
    if(noentedit())
    {
        return;
    }
    int d  = DIMENSION(sel.orient),
        dd = (*cw<0) == DIM_COORD(sel.orient) ? R[d] : C[d];
    float mid = sel.s[dd]*sel.grid/2+sel.o[dd];
    vec s(sel.o.v);
    GROUP_EDIT_UNDO(
        e.o[dd] -= (e.o[dd]-mid)*2;
        e.o.sub(s);
        swap(e.o[R[d]], e.o[C[d]]);
        e.o.add(s);
    );
}

VAR(entselsnap, 0, 0, 1);

extern void boxs(int orient, vec o, const vec &s, float size);
extern void boxs(int orient, vec o, const vec &s);
extern void boxs3D(const vec &o, vec s, int g);
extern bool editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first);

int entmoving = 0;

//drags entities in selection by given displacement vector (snaps to grid if needed)
void entdrag(const vec &ray)
{
    if(noentedit() || !haveselent())
    {
        return;
    }
    float r = 0,
          c = 0;
    static vec dest, handle;
    vec eo, es;
    int d = DIMENSION(entorient),
        dc= DIM_COORD(entorient);

    ENT_FOCUS(entgroup.last(),
        entselectionbox(e, eo, es);
        if(!editmoveplane(e.o, ray, d, eo[d] + (dc ? es[d] : 0), handle, dest, entmoving==1))
        {
            return;
        }
        ivec g(dest);
        int z = g[d]&(~(sel.grid-1));
        g.add(sel.grid/2).mask(~(sel.grid-1));
        g[d] = z;
        //snap to grid if selsnap is enabled
        r = (entselsnap ? g[R[d]] : dest[R[d]]) - e.o[R[d]];
        c = (entselsnap ? g[C[d]] : dest[C[d]]) - e.o[C[d]];
    );

    if(entmoving==1)
    {
        makeundoent();
    }
    GROUP_EDIT_PURE(e.o[R[d]] += r; e.o[C[d]] += c);
    entmoving = 2;
}

VAR(showentradius, 0, 1, 1);

//draws a circle around a point with a given radius in the world
//used for rendering entities' bounding regions
void renderentring(const extentity &e, float radius, int axis)
{
    int numsteps = 15; //increase this for higher-res bounding circles
    if(radius <= 0)
    {
        return;
    }
    gle::defvertex();
    gle::begin(GL_LINE_LOOP);
    for(int i = 0; i < numsteps; ++i)
    {
        vec p(e.o);
        const vec2 &sc = sincos360[i*(360/numsteps)];
        p[axis>=2 ? 1 : 0] += radius*sc.x;
        p[axis>=1 ? 2 : 1] += radius*sc.y;
        gle::attrib(p);
    }
    xtraverts += gle::end();
}

//draws a renderentring() around three coordinate axes for an ent at a particular
//location and at a particualar radius (e.g. the light, sound, etc. radius attr)
void renderentsphere(const extentity &e, float radius)
{
    if(radius <= 0)
    {
        return;
    }
    for(int k = 0; k < 3; ++k) //one each for x,y,z
    {
        renderentring(e, radius, k);
    }
}

void renderentattachment(const extentity &e)
{
    if(!e.attached)
    {
        return;
    }
    gle::defvertex();
    gle::begin(GL_LINES);
    gle::attrib(e.o);
    gle::attrib(e.attached->o);
    xtraverts += gle::end();
}

void renderentarrow(const extentity &e, const vec &dir, float radius)
{
    if(radius <= 0)
    {
        return;
    }
    float arrowsize = min(radius/8, 0.5f);
    vec target = vec(dir).mul(radius).add(e.o),
        arrowbase = vec(dir).mul(radius - arrowsize).add(e.o),
        spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(arrowsize);

    gle::defvertex();

    gle::begin(GL_LINES);
    gle::attrib(e.o);
    gle::attrib(target);
    xtraverts += gle::end();

    gle::begin(GL_TRIANGLE_FAN);
    gle::attrib(target);
    for(int i = 0; i < 5; ++i)
    {
        gle::attrib(vec(spoke).rotate(2*M_PI*i/4.0f, dir).add(arrowbase));
    }
    xtraverts += gle::end();
}

//cone for spotlight ents, defined by a spread angle (half of the overall peak angle)
//and by the radius of its bounding sphere
void renderentcone(const extentity &e, const vec &dir, float radius, float angle)
{
    int numsteps = 8; //increase this for higher-res bounding cone (beware ray clutter)
    if(radius <= 0)
    {
        return;
    }
    vec spot = vec(dir).mul(radius*cosf(angle*RAD)).add(e.o), spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(radius*sinf(angle*RAD));

    gle::defvertex();

    gle::begin(GL_LINES);
    for(int i = 0; i < numsteps; ++i)
    {
        gle::attrib(e.o);
        gle::attrib(vec(spoke).rotate(2*M_PI*i/static_cast<float>(numsteps), dir).add(spot));
    }
    xtraverts += gle::end();

    gle::begin(GL_LINE_LOOP);
    for(int i = 0; i < numsteps; ++i)
    {
        gle::attrib(vec(spoke).rotate(2*M_PI*i/static_cast<float>(numsteps), dir).add(spot));
    }
    xtraverts += gle::end();
}

void renderentbox(const extentity &e, const vec &center, const vec &radius, int yaw, int pitch, int roll)
{
    matrix4x3 orient;
    orient.identity();
    orient.settranslation(e.o);
    //reorientation of ent box according to passed euler angles
    if(yaw)
    {
        orient.rotate_around_z(sincosmod360(yaw));
    }
    if(pitch)
    {
        orient.rotate_around_x(sincosmod360(pitch));
    }
    if(roll)
    {
        orient.rotate_around_y(sincosmod360(-roll));
    }
    orient.translate(center);

    gle::defvertex();

    vec front[4] = { vec(-radius.x, -radius.y, -radius.z),
                     vec( radius.x, -radius.y, -radius.z),
                     vec( radius.x, -radius.y,  radius.z),
                     vec(-radius.x, -radius.y,  radius.z) },

        back[4]  = { vec(-radius.x, radius.y, -radius.z),
                     vec( radius.x, radius.y, -radius.z),
                     vec( radius.x, radius.y,  radius.z),
                     vec(-radius.x, radius.y,  radius.z) };
    for(int i = 0; i < 4; ++i)
    {
        front[i] = orient.transform(front[i]);
        back[i] = orient.transform(back[i]);
    }
    //note that there is two loops & a series of lines -> 12 lines total
    gle::begin(GL_LINE_LOOP);
    for(int i = 0; i < 4; ++i)
    {
        gle::attrib(front[i]);
    }
    xtraverts += gle::end();
    gle::begin(GL_LINES);
    gle::attrib(front[0]);
    gle::attrib(front[2]);
    gle::attrib(front[1]);
    gle::attrib(front[3]);
    for(int i = 0; i < 4; ++i)
    {
        gle::attrib(front[i]);
        gle::attrib(back[i]);
    }
    xtraverts += gle::end();

    gle::begin(GL_LINE_LOOP);
    for(int i = 0; i < 4; ++i)
    {
        gle::attrib(back[i]);
    }
    xtraverts += gle::end();
}

void renderentradius(extentity &e, bool color)
{
    switch(e.type)
    {
        case EngineEnt_Light:
            if(e.attr1 <= 0)
            {
                break;
            }
            if(color)
            {
                //color bounding sphere to color of light ent
                gle::colorf(e.attr2/255.0f, e.attr3/255.0f, e.attr4/255.0f);
            }
            renderentsphere(e, e.attr1);
            break;

        case EngineEnt_Spotlight:
            if(e.attached)
            {
                if(color)
                {
                    gle::colorf(0, 1, 1); //always teal regardless of host light color
                }
                float radius = e.attached->attr1;
                if(radius <= 0)
                {
                    break;
                }
                vec dir = vec(e.o).sub(e.attached->o).normalize();
                float angle = std::clamp(static_cast<int>(e.attr1), 1, 89); //discard >=90 angles it cannot use
                renderentattachment(e);
                renderentcone(*e.attached, dir, radius, angle);
            }
            break;

        case EngineEnt_Sound:
            if(color)
            {
                gle::colorf(0, 1, 1); //always teal
            }
            renderentsphere(e, e.attr2);
            break;

        case EngineEnt_Mapmodel:
        {
            if(color)
            {
                gle::colorf(0, 1, 1); //always teal
            }
            entities::entradius(e, color);
            vec dir;
            vecfromyawpitch(e.attr2, e.attr3, 1, 0, dir);
            renderentarrow(e, dir, 4);
            break;
        }

        case EngineEnt_Playerstart:
        {
            if(color)
            {
                gle::colorf(0, 1, 1); //always teal
            }
            entities::entradius(e, color);
            vec dir;
            vecfromyawpitch(e.attr1, 0, 1, 0, dir);
            renderentarrow(e, dir, 4); //points towards where player faces at spawn
            break;
        }

        case EngineEnt_Decal:
        {
            if(color)
            {
                gle::colorf(0, 1, 1);
            }
            DecalSlot &s = lookupdecalslot(e.attr1, false);
            float size = max(static_cast<float>(e.attr5), 1.0f);
            float depth = getdecalslotdepth(s);
            renderentbox(e, vec(0, depth * size/2, 0), vec(size/2, depth * size/2, size/2), e.attr2, e.attr3, e.attr4);
            break;
        }

        default:
            if(e.type>=EngineEnt_GameSpecific)
            {
                if(color)
                {
                    gle::colorf(0, 1, 1);
                }
                entities::entradius(e, color);
            }
            break;
    }
}

static void renderentbox(const vec &eo, vec es)
{
    es.add(eo);

    // bottom quad
    gle::attrib(eo.x, eo.y, eo.z); gle::attrib(es.x, eo.y, eo.z);
    gle::attrib(es.x, eo.y, eo.z); gle::attrib(es.x, es.y, eo.z);
    gle::attrib(es.x, es.y, eo.z); gle::attrib(eo.x, es.y, eo.z);
    gle::attrib(eo.x, es.y, eo.z); gle::attrib(eo.x, eo.y, eo.z);

    // top quad
    gle::attrib(eo.x, eo.y, es.z); gle::attrib(es.x, eo.y, es.z);
    gle::attrib(es.x, eo.y, es.z); gle::attrib(es.x, es.y, es.z);
    gle::attrib(es.x, es.y, es.z); gle::attrib(eo.x, es.y, es.z);
    gle::attrib(eo.x, es.y, es.z); gle::attrib(eo.x, eo.y, es.z);

    // sides
    gle::attrib(eo.x, eo.y, eo.z); gle::attrib(eo.x, eo.y, es.z);
    gle::attrib(es.x, eo.y, eo.z); gle::attrib(es.x, eo.y, es.z);
    gle::attrib(es.x, es.y, eo.z); gle::attrib(es.x, es.y, es.z);
    gle::attrib(eo.x, es.y, eo.z); gle::attrib(eo.x, es.y, es.z);
}

void renderentselection(const vec &o, const vec &ray, bool entmoving)
{
    if(noentedit() || (entgroup.empty() && enthover < 0))
    {
        return;
    }
    vec eo, es;

    if(entgroup.length())
    {
        gle::colorub(0, 40, 0);
        gle::defvertex();
        gle::begin(GL_LINES, entgroup.length()*24);
        for(int i = 0; i < entgroup.length(); i++)
        {
            ENT_FOCUS(entgroup[i],
                entselectionbox(e, eo, es);
                renderentbox(eo, es);
            );
        }
        xtraverts += gle::end();
    }

    if(enthover >= 0)
    {
        gle::colorub(0, 40, 0);
        ENT_FOCUS(enthover, entselectionbox(e, eo, es)); // also ensures enthover is back in focus
        boxs3D(eo, es, 1);
        gle::colorub(200,0,0);
        boxs(entorient, eo, es);
        boxs(entorient, eo, es, std::clamp(0.015f*camera1->o.dist(eo)*tan(fovy*0.5f*RAD), 0.1f, 1.0f));
    }

    if(showentradius)
    {
        glDepthFunc(GL_GREATER);
        gle::colorf(0.25f, 0.25f, 0.25f);
        for(int i = 0; i < entgroup.length(); i++)
        {
            ENT_FOCUS(entgroup[i], renderentradius(e, false));
        }
        if(enthover>=0)
        {
            ENT_FOCUS(enthover, renderentradius(e, false));
        }
        glDepthFunc(GL_LESS);
        for(int i = 0; i < entgroup.length(); i++)
        {
            ENT_FOCUS(entgroup[i], renderentradius(e, true));
        }
        if(enthover>=0)
        {
            ENT_FOCUS(enthover, renderentradius(e, true));
        }
    }
}

//=== entity edit commands ===//
bool enttoggle(int id)
{
    undonext = true;
    int i = entgroup.find(id);
    if(i < 0)
    {
        entadd(id);
    }
    else
    {
        entgroup.remove(i);
    }
    return i < 0;
}

bool hoveringonent(int ent, int orient)
{
    if(noentedit())
    {
        return false;
    }
    entorient = orient;
    if((efocus = enthover = ent) >= 0)
    {
        return true;
    }
    efocus   = entgroup.empty() ? -1 : entgroup.last();
    enthover = -1;
    return false;
}

ICOMMAND(entadd, "", (),
{
    if(enthover >= 0 && !noentedit())
    {
        if(entgroup.find(enthover) < 0)
        {
            entadd(enthover);
        }
        if(entmoving > 1)
        {
            entmoving = 1;
        }
    }
});

ICOMMAND(enttoggle, "", (),
{
    if(enthover < 0 || noentedit() || !enttoggle(enthover))
    {
        entmoving = 0;
        intret(0);
    }
    else
    {
        if(entmoving > 1)
        {
            entmoving = 1;
        }
        intret(1);
    }
});

ICOMMAND(entmoving, "b", (int *n),
{
    if(*n >= 0)
    {
        if(!*n || enthover < 0 || noentedit())
        {
            entmoving = 0;
        }
        else
        {
            if(entgroup.find(enthover) < 0)
            {
                entadd(enthover);
                entmoving = 1;
            }
            else if(!entmoving)
            {
                entmoving = 1;
            }
        }
    }
    intret(entmoving);
});

void entpush(int *dir)
{
    if(noentedit())
    {
        return;
    }
    int d = DIMENSION(entorient);
    int s = DIM_COORD(entorient) ? -*dir : *dir;
    if(entmoving)
    {
        GROUP_EDIT_PURE(e.o[d] += static_cast<float>(s*sel.grid)); // editdrag supplies the undo
    }
    else
    {
        GROUP_EDIT(e.o[d] += static_cast<float>(s*sel.grid));
    }
}

VAR(entautoviewdist, 0, 25, 100);
void entautoview(int *dir)
{
    if(!haveselent())
    {
        return;
    }
    static int s = 0;
    vec v(player->o);
    v.sub(worldpos);
    v.normalize();
    v.mul(entautoviewdist);
    int t = s + *dir;
    s = abs(t) % entgroup.length();
    if(t<0 && s>0)
    {
        s = entgroup.length() - s;
    }
    ENT_FOCUS(entgroup[s],
        v.add(e.o);
        player->o = v;
        player->resetinterp();
    );
}

COMMAND(entautoview, "i");
COMMAND(entflip, "");
COMMAND(entrotate, "i");
COMMAND(entpush, "i");

void delent()
{
    if(noentedit())
    {
        return;
    }
    GROUP_EDIT(e.type = EngineEnt_Empty;);
    entcancel();
}

//entdrop:
// 0: places it at current eye pos
// 1: places it at current feet pos
// 2: places it in center of current sel
// 3: places it at the floor of current sel
VAR(entdrop, 0, 2, 3);

bool dropentity(entity &e, int drop = -1)
{
    vec radius(4.0f, 4.0f, 4.0f);
    if(drop<0)
    {
        drop = entdrop;
    }
    if(e.type == EngineEnt_Mapmodel)
    {
        model *m = loadmapmodel(e.attr1);
        if(m)
        {
            vec center;
            mmboundbox(e, m, center, radius);
            radius.x += fabs(center.x);
            radius.y += fabs(center.y);
        }
        radius.z = 0.0f;
    }
    switch(drop)
    {
        case 1:
        {
            if(e.type != EngineEnt_Light && e.type != EngineEnt_Spotlight)
            {
                dropenttofloor(&e);
            }
            break;
        }
        case 2:
        case 3:
        {
            int cx = 0,
                cy = 0;
            if(sel.cxs == 1 && sel.cys == 1)
            {
                cx = (sel.cx ? 1 : -1) * sel.grid / 2;
                cy = (sel.cy ? 1 : -1) * sel.grid / 2;
            }
            e.o = vec(sel.o);
            int d  = DIMENSION(sel.orient),
                dc = DIM_COORD(sel.orient);
            e.o[R[d]] += sel.grid / 2 + cx;
            e.o[C[d]] += sel.grid / 2 + cy;
            if(!dc)
            {
                e.o[D[d]] -= radius[D[d]];
            }
            else
            {
                e.o[D[d]] += sel.grid + radius[D[d]];
            }
            if(drop == 3)
            {
                dropenttofloor(&e);
            }
            break;
        }
    }
    return true;
}

void dropent()
{
    if(noentedit())
    {
        return;
    }
    GROUP_EDIT(dropentity(e));
}

void attachent()
{
    if(noentedit())
    {
        return;
    }
    GROUP_EDIT(attachentity(e));
}

COMMAND(attachent, "");

static int keepents = 0;

extentity *newentity(bool local, const vec &o, int type, int v1, int v2, int v3, int v4, int v5, int &idx, bool fix = true)
{
    vector<extentity *> &ents = entities::getents();
    if(local)
    {
        idx = -1;
        for(int i = keepents; i < ents.length(); i++)
        {
            if(ents[i]->type == EngineEnt_Empty)
            {
                idx = i;
                break;
            }
        }
        if(idx < 0 && ents.length() >= maxents)
        {
            conoutf("too many entities");
            return NULL;
        }
    }
    else
    {
        while(ents.length() < idx)
        {
            ents.add(entities::newentity())->type = EngineEnt_Empty;
        }
    }
    extentity &e = *entities::newentity();
    e.o = o;
    e.attr1 = v1;
    e.attr2 = v2;
    e.attr3 = v3;
    e.attr4 = v4;
    e.attr5 = v5;
    e.type = type;
    e.reserved = 0;
    if(local && fix)
    {
        switch(type)
        {
            case EngineEnt_Decal:
            {
                if(!e.attr2 && !e.attr3 && !e.attr4)
                {
                    //place decals at camera orient
                    e.attr2 = static_cast<int>(camera1->yaw);
                    e.attr3 = static_cast<int>(camera1->pitch);
                    e.attr4 = static_cast<int>(camera1->roll);
                }
                break;
            }
            case EngineEnt_Mapmodel:
            {
                if(!e.attr2) e.attr2 = static_cast<int>(camera1->yaw);
                break;
            }
            case EngineEnt_Playerstart:
            {
                //place playerstart at camera yaw
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
                e.attr3 = e.attr2;
                e.attr2 = e.attr1;
                e.attr1 = static_cast<int>(camera1->yaw);
                break;
            }
        }
        entities::fixentity(e);
    }
    if(ents.inrange(idx))
    {
        entities::deleteentity(ents[idx]);
        ents[idx] = &e;
    }
    else
    {
        idx = ents.length();
        ents.add(&e);
    }
    return &e;
}

void newentity(int type, int a1, int a2, int a3, int a4, int a5, bool fix = true)
{
    int idx;
    extentity *t = newentity(true, player->o, type, a1, a2, a3, a4, a5, idx, fix);
    if(!t)
    {
        return;
    }
    dropentity(*t);
    t->type = EngineEnt_Empty;
    enttoggle(idx);
    makeundoent();
    ENT_EDIT(idx, e.type = type);
    commitchanges();
}

void newent(char *what, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    if(noentedit())
    {
        return;
    }
    int type = findtype(what);
    if(type != EngineEnt_Empty)
    {
        newentity(type, *a1, *a2, *a3, *a4, *a5);
    }
}

int entcopygrid;
vector<entity> entcopybuf;

void entcopy()
{
    if(noentedit())
    {
        return;
    }
    entcopygrid = sel.grid;
    entcopybuf.shrink(0);
    ADD_IMPLICIT({
        for(int i = 0; i < entgroup.length(); i++)
        {
            ENT_FOCUS(entgroup[i], entcopybuf.add(e).o.sub(vec(sel.o)));
        }
    });
}

void entpaste()
{
    if(noentedit() || entcopybuf.empty())
    {
        return;
    }
    entcancel();
    float m = static_cast<float>(sel.grid)/static_cast<float>(entcopygrid);
    for(int i = 0; i < entcopybuf.length(); i++)
    {
        const entity &c = entcopybuf[i];
        vec o = vec(c.o).mul(m).add(vec(sel.o));
        int idx;
        extentity *e = newentity(true, o, EngineEnt_Empty, c.attr1, c.attr2, c.attr3, c.attr4, c.attr5, idx);
        if(!e)
        {
            continue;
        }
        entadd(idx);
        keepents = max(keepents, idx+1);
    }
    keepents = 0;
    int j = 0;
    GROUP_EDIT_UNDO(e.type = entcopybuf[j++].type;);
}

void entreplace()
{
    if(noentedit() || entcopybuf.empty())
    {
        return;
    }
    const entity &c = entcopybuf[0];
    if(entgroup.length() || enthover >= 0)
    {
        GROUP_EDIT({
            e.type = c.type;
            e.attr1 = c.attr1;
            e.attr2 = c.attr2;
            e.attr3 = c.attr3;
            e.attr4 = c.attr4;
            e.attr5 = c.attr5;
        });
    }
    else
    {
        newentity(c.type, c.attr1, c.attr2, c.attr3, c.attr4, c.attr5, false);
    }
}

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, int attr5, bool local)
{
    if(i < 0 || i >= maxents)
    {
        return;
    }
    vector<extentity *> &ents = entities::getents();
    if(ents.length()<=i)
    {
        extentity *e = newentity(local, o, type, attr1, attr2, attr3, attr4, attr5, i);
        if(!e)
        {
            return;
        }
        addentityedit(i);
        attachentity(*e);
    }
    else
    {
        extentity &e = *ents[i];
        removeentityedit(i);
        int oldtype = e.type;
        if(oldtype!=type)
        {
            detachentity(e);
        }
        e.type = type;
        e.o = o;
        e.attr1 = attr1; e.attr2 = attr2; e.attr3 = attr3; e.attr4 = attr4; e.attr5 = attr5;
        addentityedit(i);
        if(oldtype!=type)
        {
            attachentity(e);
        }
    }
    entities::editent(i, local);
    clearshadowcache();
    commitchanges();
}

COMMAND(newent, "siiiii");
COMMAND(delent, "");
COMMAND(dropent, "");
COMMAND(entcopy, "");
COMMAND(entpaste, "");
COMMAND(entreplace, "");

namespace entities
{
    using namespace game;

    const char *entmdlname(int type)
    {
        static const char * const entmdlnames[GamecodeEnt_MaxEntTypes] =
        {
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            "game/teleport", NULL, NULL,
            NULL
        };
        return entmdlnames[type];
    }

    void preloadentities()
    {
        for(int i = 0; i < GamecodeEnt_MaxEntTypes; ++i)
        {
            const char *mdl = entmdlname(i);
            if(!mdl)
            {
                continue;
            }
            preloadmodel(mdl);
        }
        for(int i = 0; i < ents.length(); i++)
        {
            extentity &e = *ents[i];
            switch(e.type)
            {
                case GamecodeEnt_Teleport:
                {
                    if(e.attr2 > 0)
                    {
                        preloadmodel(mapmodelname(e.attr2));
                    }
                }
                [[fallthrough]];
                case GamecodeEnt_Jumppad:
                {
                    if(e.attr4 > 0)
                    {
                        preloadmapsound(e.attr4);
                    }
                    break;
                }
            }
        }
    }

    // these functions are called when the client touches the item

    void teleporteffects(gameent *d, int tp, int td, bool local)
    {
        if(ents.inrange(tp) && ents[tp]->type == GamecodeEnt_Teleport)
        {
            extentity &e = *ents[tp];
            if(e.attr4 >= 0)
            {
                int snd = Sound_Teleport,
                    flags = 0;
                if(e.attr4 > 0)
                {
                    snd = e.attr4; flags = Music_Map;
                }
                if(d == player1)
                {
                    playsound(snd, NULL, NULL, flags);
                }
                else
                {
                    playsound(snd, &e.o, NULL, flags);
                    if(ents.inrange(td) && ents[td]->type == GamecodeEnt_Teledest)
                    {
                        playsound(snd, &ents[td]->o, NULL, flags);
                    }
                }
            }
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(32, ENET_PACKET_FLAG_RELIABLE);
            putint(p, NetMsg_Teleport);
            putint(p, d->clientnum);
            putint(p, tp);
            putint(p, td);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void jumppadeffects(gameent *d, int jp, bool local)
    {
        if(ents.inrange(jp) && ents[jp]->type == GamecodeEnt_Jumppad)
        {
            extentity &e = *ents[jp];
            if(e.attr4 >= 0)
            {
                int snd = Sound_JumpPad, flags = 0;
                if(e.attr4 > 0)
                {
                    snd = e.attr4;
                    flags = Music_Map;
                }
                if(d == player1)
                {
                    playsound(snd, NULL, NULL, flags);
                }
                else
                {
                    playsound(snd, &e.o, NULL, flags);
                }
            }
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(16, ENET_PACKET_FLAG_RELIABLE);
            putint(p, NetMsg_Jumppad);
            putint(p, d->clientnum);
            putint(p, jp);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void teleport(int n, gameent *d)     // also used by monsters
    {
        int e = -1,
            tag = ents[n]->attr1,
            beenhere = -1;
        for(;;)
        {
            e = findentity(GamecodeEnt_Teledest, e+1);
            if(e==beenhere || e<0)
            {
                conoutf(Console_Warn, "no teleport destination for channel %d", tag);
                return;
            }
            if(beenhere<0)
            {
                beenhere = e;
            }
            if(ents[e]->attr2==tag)
            {
                teleporteffects(d, n, e, true);
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                if(ents[e]->attr3 > 0)
                {
                    vec dir;
                    vecfromyawpitch(d->yaw, 0, 1, 0, dir);
                    float speed = d->vel.magnitude2();
                    d->vel.x = dir.x*speed;
                    d->vel.y = dir.y*speed;
                }
                else
                {
                    d->vel = vec(0, 0, 0);
                }
                entinmap(d);
                updatedynentcache(d);
                ai::inferwaypoints(d, ents[n]->o, ents[e]->o, 16.f);
                break;
            }
        }
    }

    VARR(teleteam, 0, 1, 1);

    void trypickup(int n, gameent *d)
    {
        switch(ents[n]->type)
        {
            default:
                if(d->canpickup(ents[n]->type))
                {
                    addmsg(NetMsg_ItemPickup, "rci", d, n);
                    ents[n]->clearspawned(); // even if someone else gets it first
                }
                break;

            case GamecodeEnt_Teleport:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<500)
                {
                    break;
                }
                if(!teleteam && modecheck(gamemode, Mode_Team))
                {
                    break;
                }
                if(ents[n]->attr3 > 0)
                {
                    DEF_FORMAT_STRING(hookname, "can_teleport_%d", ents[n]->attr3);
                    if(!execidentbool(hookname, true))
                    {
                        break;
                    }
                }
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                teleport(n, d);
                break;
            }

            case GamecodeEnt_Jumppad:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<300)
                {
                    break;
                }
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                jumppadeffects(d, n, true);
                if(d->ai)
                {
                    d->ai->becareful = true;
                }
                d->falling = vec(0, 0, 0);
                d->physstate = PhysEntState_Fall;
                d->timeinair = 1;
                d->vel = vec(ents[n]->attr3*10.0f, ents[n]->attr2*10.0f, ents[n]->attr1*10.0f);
                break;
            }
        }
    }

    void checkitems(gameent *d)
    {
        if(d->state!=ClientState_Alive)
        {
            return;
        }
        vec o = d->feetpos();
        for(int i = 0; i < ents.length(); i++)
        {
            extentity &e = *ents[i];
            if(e.type==GamecodeEnt_NotUsed)
            {
                continue;
            }
            if(!e.spawned() && e.type!=GamecodeEnt_Teleport && e.type!=GamecodeEnt_Jumppad)
            {
                continue;
            }
            float dist = e.o.dist(o);
            if(dist<(e.type==GamecodeEnt_Teleport ? 16 : 12))
            {
                trypickup(i, d);
            }
        }
    }

    void putitems(packetbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, NetMsg_ItemList);
        putint(p, -1);
    }

    void resetspawns()
    {
        for(int i = 0; i < ents.length(); i++)
        {
            ents[i]->clearspawned();
        }
    }

    void setspawn(int i, bool on)
    {
        if(ents.inrange(i))
        {
            ents[i]->setspawned(on);
        }
    }

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case GamecodeEnt_Flag:
            {
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
            }
            [[fallthrough]];
            case GamecodeEnt_Teledest:
            {
                e.attr3 = e.attr2;
                e.attr2 = e.attr1;
                e.attr1 = static_cast<int>(player1->yaw);
                break;
            }
        }
    }

    void entradius(extentity &e, bool color)
    {
        switch(e.type)
        {
            //teleport edit aid vector
            case GamecodeEnt_Teleport:
            {
                for(int i = 0; i < ents.length(); i++)
                {
                    if(ents[i]->type == GamecodeEnt_Teledest && e.attr1==ents[i]->attr2)
                    {
                        renderentarrow(e, vec(ents[i]->o).sub(e.o).normalize(), e.o.dist(ents[i]->o));
                        break;
                    }
                }
                break;
            }
            //jumppad edit aid vector
            case GamecodeEnt_Jumppad:
            {
                renderentarrow(e, vec(static_cast<float>(e.attr3), static_cast<float>(e.attr2), static_cast<float>(e.attr1)).normalize(), 4);
                break;
            }
            case GamecodeEnt_Flag:
            case GamecodeEnt_Teledest:
            {
                vec dir;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                break;
            }
        }
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        //e.flags = 0;
        if(local)
        {
            addmsg(NetMsg_EditEnt, "rii3ii5", i, static_cast<int>(e.o.x*DMF), static_cast<int>(e.o.y*DMF), static_cast<int>(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
        }
    }
}

