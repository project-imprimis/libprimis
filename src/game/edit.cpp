#include "game.h"

static const struct
{
    const char *name;
    int filter;
} editmatfilters[] =
{
    { "empty", EditMatFlag_Empty },
    { "notempty", EditMatFlag_NotEmpty },
    { "solid", EditMatFlag_Solid },
    { "notsolid", EditMatFlag_NotSolid }
};

void mpcalclight(bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_CalcLight);
    }
    calclight();
}

ICOMMAND(calclight, "", (), mpcalclight(true));

void mpremip(bool local)
{
    extern selinfo sel;
    if(local)
    {
        game::edittrigger(sel, Edit_Remip);
    }
    remip();
    allchanged();
}

ICOMMAND(remip, "", (), mpremip(true));

void pasteundo(undoblock *u)
{
    if(u->numents)
    {
        pasteundoents(u);
    }
    else
    {
        pasteundoblock(u->block(), u->gridmap());
    }
}

void swapundo(undolist &a, undolist &b, int op)
{
    if(noedit())
    {
        return;
    }
    if(a.empty())
    {
        conoutf(Console_Warn, "nothing more to %s", op == Edit_Redo ? "redo" : "undo");
        return;
    }
    int ts = a.last->timestamp;
    if(multiplayer)
    {
        int n   = 0,
            ops = 0;
        for(undoblock *u = a.last; u && ts==u->timestamp; u = u->prev)
        {
            ++ops;
            n += u->numents ? u->numents : countblock(u->block());
            if(ops > 10 || n > 2500)
            {
                conoutf(Console_Warn, "undo too big for multiplayer");
                if(nompedit)
                {
                    multiplayerwarn();
                    return;
                }
                op = -1;
                break;
            }
        }
    }
    selinfo l = sel;
    while(!a.empty() && ts==a.last->timestamp)
    {
        if(op >= 0)
        {
            game::edittrigger(sel, op);
        }
        undoblock *u = a.poplast(), *r;
        if(u->numents)
        {
            r = copyundoents(u);
        }
        else
        {
            block3 *ub = u->block();
            l.o = ub->o;
            l.s = ub->s;
            l.grid = ub->grid;
            l.orient = ub->orient;
            r = newundocube(l);
        }
        if(r)
        {
            r->size = u->size;
            r->timestamp = totalmillis;
            b.add(r);
        }
        pasteundo(u);
        if(!u->numents)
        {
            changed(*u->block(), false);
        }
        freeundo(u);
    }
    commitchanges();
    if(!hmapsel)
    {
        sel = l;
        reorient();
    }
    forcenextundo();
}

void editundo() { swapundo(undos, redos, Edit_Undo); }
void editredo() { swapundo(redos, undos, Edit_Redo); }

void mpcopy(editinfo *&e, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Copy);
    }
    if(e==NULL)
    {
        e = editinfos.add(new editinfo);
    }
    if(e->copy)
    {
        freeblock(e->copy);
    }
    e->copy = NULL;
    PROTECT_SEL(e->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
}

void mppaste(editinfo *&e, selinfo &sel, bool local)
{
    if(e==NULL)
    {
        return;
    }
    if(local)
    {
        game::edittrigger(sel, Edit_Paste);
    }
    if(e->copy)
    {
        pasteblock(*e->copy, sel, local);
    }
}

void copy()
{
    if(noedit(true))
    {
        return;
    }
    mpcopy(localedit, sel, true);
}

void pastehilite()
{
    if(!localedit)
    {
        return;
    }
    sel.s = localedit->copy->s;
    reorient();
    havesel = true;
}

void paste()
{
    if(noedit(true))
    {
        return;
    }
    mppaste(localedit, sel, true);
}

COMMAND(copy, "");
COMMAND(pastehilite, "");
COMMAND(paste, "");
COMMANDN(undo, editundo, "");
COMMANDN(redo, editredo, "");

bool unpackundo(const uchar *inbuf, int inlen, int outlen)
{
    uchar *outbuf = NULL;
    if(!uncompresseditinfo(inbuf, inlen, outbuf, outlen)) return false;
    ucharbuf buf(outbuf, outlen);
    if(buf.remaining() < 2)
    {
        delete[] outbuf;
        return false;
    }
    int numents = *reinterpret_cast<const ushort *>(buf.pad(2));
    if(numents)
    {
        if(buf.remaining() < numents*static_cast<int>(2 + sizeof(entity)))
        {
            delete[] outbuf;
            return false;
        }
        for(int i = 0; i < numents; ++i)
        {
            int idx = *reinterpret_cast<const ushort *>(buf.pad(2));
            entity &e = *reinterpret_cast<entity *>(buf.pad(sizeof(entity)));
            pasteundoent(idx, e);
        }
    }
    else
    {
        unpackundocube(buf, outbuf);
    }
    delete[] outbuf;
    commitchanges();
    return true;
}

#define EDITING_VSLOT(a) vslotref vslotrefs[] = { a }; (void)vslotrefs;

VAR(invalidcubeguard, 0, 1, 1);

/*mpplacecube: places a cube at the location passed
 * Arguments:
 *  sel: selection info object containing objects to be filled
 *  tex: texture to cover the cube with
 *  local: toggles the engine sending out packets to others, or to not broadcast
 *         due to having recieved the command from another client
 * Returns:
 *  void
 */
void mpplacecube(selinfo &sel, int tex, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_AddCube);
    }
    LOOP_SEL_XYZ(
        discardchildren(c, true);
        setcubefaces(c, facesolid);
    );
}

int bounded(int n)
{
    return n<0 ? 0 : (n>8 ? 8 : n);
}

void pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(EDGE_GET(edge, dc)+dir);
    EDGE_SET(edge, dc, ne);
    int oe = EDGE_GET(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne))
    {
        EDGE_SET(edge, 1-dc, ne);
    }
}

void linkedpush(cube &c, int d, int x, int y, int dc, int dir)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);

    for(int i = 0; i < 2; ++i)
    {
        for(int j = 0; j < 2; ++j)
        {
            getcubevector(c, d, i, j, dc, p);
            if(v==p)
            {
                pushedge(CUBE_EDGE(c, d, i, j), dir, dc);
            }
        }
    }
}

void mpeditface(int dir, int mode, selinfo &sel, bool local)
{
    if(mode==1 && (sel.cx || sel.cy || sel.cxs&1 || sel.cys&1))
    {
        mode = 0;
    }
    int d = DIMENSION(sel.orient);
    int dc = DIM_COORD(sel.orient);
    int seldir = dc ? -dir : dir;

    if(local)
        game::edittrigger(sel, Edit_Face, dir, mode);

    if(mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if(((dir>0) == dc && h<=0) || ((dir<0) == dc && h>=worldsize))
        {
            return;
        }
        if(dir<0)
        {
            sel.o[d] += sel.grid * seldir;
        }
    }

    if(dc)
    {
        sel.o[d] += sel.us(d)-sel.grid;
    }
    sel.s[d] = 1;

    LOOP_SEL_XYZ(
        if(c.children)
        {
            setcubefaces(c, facesolid);
        }
        ushort mat = getmaterial(c);
        discardchildren(c, true);
        c.material = mat;
        if(mode==1) // fill command
        {
            if(dir<0)
            {
                setcubefaces(c, facesolid);
                cube &o = blockcube(x, y, 1, sel, -sel.grid);
                for(int i = 0; i < 6; ++i) //for each face
                {
                    c.texture[i] = o.children ? Default_Geom : o.texture[i];
                }
            }
            else
            {
                setcubefaces(c, faceempty);
            }
        }
        else
        {
            uint bak = c.faces[d];
            uchar *p = reinterpret_cast<uchar *>(&c.faces[d]);
            if(mode==2)
            {
                linkedpush(c, d, sel.corner&1, sel.corner>>1, dc, seldir); // corner command
            }
            else
            {
                for(int mx = 0; mx < 2; ++mx) // pull/push edges command
                {
                    for(int my = 0; my < 2; ++my)
                    {
                        if(x==0 && mx==0 && sel.cx)
                        {
                            continue;
                        }
                        if(y==0 && my==0 && sel.cy)
                        {
                            continue;
                        }
                        if(x==sel.s[R[d]]-1 && mx==1 && (sel.cx+sel.cxs)&1)
                        {
                            continue;
                        }
                        if(y==sel.s[C[d]]-1 && my==1 && (sel.cy+sel.cys)&1)
                        {
                            continue;
                        }
                        if(p[mx+my*2] != ((uchar *)&bak)[mx+my*2])
                        {
                            continue;
                        }
                        linkedpush(c, d, mx, my, dc, seldir);
                    }
                }
            }
            optiface(p, c);
            if(invalidcubeguard==1 && !isvalidcube(c))
            {
                uint newbak = c.faces[d];
                uchar *m = reinterpret_cast<uchar *>(&bak);
                uchar *n = reinterpret_cast<uchar *>(&newbak);
                for(int k = 0; k < 4; ++k)
                {
                    if(n[k] != m[k]) // tries to find partial edit that is valid
                    {
                        c.faces[d] = bak;
                        c.edges[d*4+k] = n[k];
                        if(isvalidcube(c))
                        {
                            m[k] = n[k];
                        }
                    }
                }
                c.faces[d] = bak;
            }
        }
    );
    if (mode==1 && dir>0)
    {
        sel.o[d] += sel.grid * seldir;
    }
}


void edithmap(int dir, int mode)
{
    if((nompedit && multiplayer) || !hmapsel)
    {
        multiplayerwarn();
        return;
    }
    hmap::run(dir, mode);
}

void editface(int *dir, int *mode)
{
    if(noedit(moving!=0))
    {
        return;
    }
    if(hmapedit!=1)
    {
        mpeditface(*dir, *mode, sel, true);
    }
    else
    {
        edithmap(*dir, *mode);
    }
}

/*mpdelcube: deletes a cube by discarding cubes insel's children and emptying faces
 * Arguments:
 *  sel:   the selection which is to be deleted
 *  local: whether to send a network message or not (nonlocal cube deletions don't
 *             send messages, as this would cause an infinite loop)
 * Returns:
 *  void
 */

void mpdelcube(selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_DelCube);
    }
    LOOP_SEL_XYZ(discardchildren(c, true); setcubefaces(c, faceempty));
}

void delcube()
{
    if(noedit(true))
    {
        return;
    }
    mpdelcube(sel, true);
}


VAR(selectionsurf, 0, 0, 1);

void pushsel(int *dir)
{
    if(noedit(moving!=0))
    {
        return;
    }
    int d = DIMENSION(orient);
    int s = DIM_COORD(orient) ? -*dir : *dir;
    sel.o[d] += s*sel.grid;
    if(selectionsurf==1)
    {
        player->o[d] += s*sel.grid;
        player->resetinterp();
    }
}

COMMAND(pushsel, "i");
COMMAND(editface, "ii");
COMMAND(delcube, "");

void tofronttex() // maintain most recently used of the texture lists when applying texture
{
    int c = curtexindex;
    if(texmru.inrange(c))
    {
        texmru.insert(0, texmru.remove(c));
        curtexindex = -1;
    }
}

void mpeditvslot(int delta, VSlot &ds, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_VSlot, delta, allfaces, 0, &ds);
        if(!(lastsel==sel))
        {
            tofronttex();
        }
        if(allfaces || !(repsel == sel))
        {
            reptex = -1;
        }
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    VSlot *findedit = NULL;
    LOOP_SEL_XYZ(remapvslots(c, delta != 0, ds, allfaces ? -1 : sel.orient, findrep, findedit));
    remappedvslots.setsize(0);
    if(local && findedit)
    {
        lasttex = findedit->index;
        lasttexmillis = totalmillis;
        curtexindex = texmru.find(lasttex);
        if(curtexindex < 0)
        {
            curtexindex = texmru.length();
            texmru.add(lasttex);
        }
    }
}

bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf)
{
    VSlot ds;
    if(!unpackvslot(buf, ds, delta != 0))
    {
        return false;
    }
    mpeditvslot(delta, ds, allfaces, sel, false);
    return true;
}


VAR(allfaces, 0, 0, 1);
VAR(usevdelta, 1, 0, 0);

void vdelta(uint *body)
{
    if(noedit())
    {
        return;
    }
    usevdelta++;
    execute(body);
    usevdelta--;
}
COMMAND(vdelta, "e");

void vrotate(int *n)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Rotation;
    ds.rotation = usevdelta ? *n : std::clamp(*n, 0, 7);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vrotate, "i");

void vangle(float *a)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Angle;
    ds.angle = vec(*a, sinf(RAD**a), cosf(RAD**a));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vangle, "f");

void voffset(int *x, int *y)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Offset;
    ds.offset = usevdelta ? ivec2(*x, *y) : ivec2(*x, *y).max(0);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(voffset, "ii");

void vscroll(float *s, float *t)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Scroll;
    ds.scroll = vec2(*s/1000.0f, *t/1000.0f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vscroll, "ff");

void vscale(float *scale)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Scale;
    ds.scale = *scale <= 0 ? 1 : (usevdelta ? *scale : std::clamp(*scale, 1/8.0f, 8.0f));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vscale, "f");

void valpha(float *front, float *back)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Alpha;
    ds.alphafront = std::clamp(*front, 0.0f, 1.0f);
    ds.alphaback = std::clamp(*back, 0.0f, 1.0f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(valpha, "ff");

void vcolor(float *r, float *g, float *b)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Color;
    ds.colorscale = vec(std::clamp(*r, 0.0f, 2.0f), std::clamp(*g, 0.0f, 2.0f), std::clamp(*b, 0.0f, 2.0f));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vcolor, "fff");

void vrefract(float *k, float *r, float *g, float *b)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_Refract;
    ds.refractscale = std::clamp(*k, 0.0f, 1.0f);
    if(ds.refractscale > 0 && (*r > 0 || *g > 0 || *b > 0))
    {
        ds.refractcolor = vec(std::clamp(*r, 0.0f, 1.0f), std::clamp(*g, 0.0f, 1.0f), std::clamp(*b, 0.0f, 1.0f));
    }
    else
    {
        ds.refractcolor = vec(1, 1, 1);
    }
    mpeditvslot(usevdelta, ds, allfaces, sel, true);

}
COMMAND(vrefract, "ffff");

void vreset()
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vreset, "");

void vshaderparam(const char *name, float *x, float *y, float *z, float *w)
{
    if(noedit())
    {
        return;
    }
    VSlot ds;
    ds.changed = 1 << VSlot_ShParam;
    if(name[0])
    {
        SlotShaderParam p = { getshaderparamname(name), -1, 0, {*x, *y, *z, *w} };
        ds.params.add(p);
    }
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(vshaderparam, "sfFFf");

void mpedittex(int tex, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Tex, tex, allfaces);
        if(allfaces || !(repsel == sel))
        {
            reptex = -1;
        }
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    LOOP_SEL_XYZ(edittexcube(c, tex, allfaces ? -1 : sel.orient, findrep));
}

static int unpacktex(int &tex, ucharbuf &buf, bool insert = true)
{
    if(tex < 0x10000)
    {
        return true;
    }
    VSlot ds;
    if(!unpackvslot(buf, ds, false))
    {
        return false;
    }
    VSlot &vs = *lookupslot(tex & 0xFFFF, false).variants;
    if(vs.index < 0 || vs.index == Default_Sky)
    {
        return false;
    }
    VSlot *edit = insert ? editvslot(vs, ds) : findvslot(*vs.slot, vs, ds);
    if(!edit)
    {
        return false;
    }
    tex = edit->index;
    return true;
}

int shouldpacktex(int index)
{
    if(vslots.inrange(index))
    {
        VSlot &vs = *vslots[index];
        if(vs.changed)
        {
            return 0x10000 + vs.slot->index;
        }
    }
    return 0;
}

bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf)
{
    if(!unpacktex(tex, buf))
    {
        return false;
    }
    mpedittex(tex, allfaces, sel, false);
    return true;
}

static void filltexlist()
{
    if(texmru.length()!=vslots.length())
    {
        for(int i = texmru.length(); --i >=0;) //note reverse iteration
        {
            if(texmru[i]>=vslots.length())
            {
                if(curtexindex > i)
                {
                    curtexindex--;
                }
                else if(curtexindex == i)
                {
                    curtexindex = -1;
                }
                texmru.remove(i);
            }
        }
        for(int i = 0; i < vslots.length(); i++)
        {
            if(texmru.find(i)<0)
            {
                texmru.add(i);
            }
        }
    }
}

void compactmruvslots()
{
    remappedvslots.setsize(0);
    for(int i = texmru.length(); --i >=0;) //note reverse iteration
    {
        if(vslots.inrange(texmru[i]))
        {
            VSlot &vs = *vslots[texmru[i]];
            if(vs.index >= 0)
            {
                texmru[i] = vs.index;
                continue;
            }
        }
        if(curtexindex > i)
        {
            curtexindex--;
        }
        else if(curtexindex == i)
        {
            curtexindex = -1;
        }
        texmru.remove(i);
    }
    if(vslots.inrange(lasttex))
    {
        VSlot &vs = *vslots[lasttex];
        lasttex = vs.index >= 0 ? vs.index : 0;
    }
    else
    {
        lasttex = 0;
    }
    reptex = vslots.inrange(reptex) ? vslots[reptex]->index : -1;
}

void edittex(int i, bool save = true)
{
    lasttex = i;
    lasttexmillis = totalmillis;
    if(save)
    {
        for(int j = 0; j < texmru.length(); j++)
        {
            if(texmru[j]==lasttex)
            {
                curtexindex = j;
                break;
            }
        }
    }
    mpedittex(i, allfaces, sel, true);
}

void edittex_(int *dir)
{
    if(noedit())
    {
        return;
    }
    filltexlist();
    if(texmru.empty())
    {
        return;
    }
    texpaneltimer = 5000;
    if(!(lastsel==sel))
    {
        tofronttex();
    }
    curtexindex = std::clamp(curtexindex<0 ? 0 : curtexindex+*dir, 0, texmru.length()-1);
    edittex(texmru[curtexindex], false);
}

void gettex()
{
    if(noedit(true))
    {
        return;
    }
    filltexlist();
    int tex = -1;
    LOOP_XYZ(sel, sel.grid, tex = c.texture[sel.orient]);
    for(int i = 0; i < texmru.length(); i++)
    {
        if(texmru[i]==tex)
        {
            curtexindex = i;
            tofronttex();
            return;
        }
    }
}

void getcurtex()
{
    if(noedit(true))
    {
        return;
    }
    filltexlist();
    int index = curtexindex < 0 ? 0 : curtexindex;
    if(!texmru.inrange(index))
    {
        return;
    }
    intret(texmru[index]);
}

void getseltex()
{
    if(noedit(true))
    {
        return;
    }
    cube &c = lookupcube(sel.o, -sel.grid);
    if(c.children || iscubeempty(c))
    {
        return;
    }
    intret(c.texture[sel.orient]);
}

void gettexname(int *tex, int *subslot)
{
    if(*tex<0)
    {
        return;
    }
    VSlot &vslot = lookupvslot(*tex, false);
    Slot &slot = *vslot.slot;
    if(!slot.sts.inrange(*subslot))
    {
        return;
    }
    result(slot.sts[*subslot].name);
}

void getslottex(int *idx)
{
    if(*idx < 0 || !slots.inrange(*idx))
    {
        intret(-1);
        return;
    }
    Slot &slot = lookupslot(*idx, false);
    intret(slot.variants->index);
}

COMMANDN(edittex, edittex_, "i");
ICOMMAND(settex, "i", (int *tex),
{
    if(!vslots.inrange(*tex) || noedit())
    {
        return;
    }
    filltexlist();
    edittex(*tex);
});
COMMAND(gettex, "");
COMMAND(getcurtex, "");
COMMAND(getseltex, "");
ICOMMAND(getreptex, "", (),
{
    if(!noedit())
    {
        intret(vslots.inrange(reptex) ? reptex : -1);
    }
});
COMMAND(gettexname, "ii");
ICOMMAND(texmru, "b", (int *idx),
{
    filltexlist();
    intret(texmru.inrange(*idx) ? texmru[*idx] : texmru.length());
});
ICOMMAND(looptexmru, "re", (ident *id, uint *body),
{
    LOOP_START(id, stack);
    filltexlist();
    for(int i = 0; i < texmru.length(); i++)
    {
        loopiter(id, stack, texmru[i]); execute(body);
    }
    loopend(id, stack);
});
ICOMMAND(numvslots, "", (), intret(vslots.length()));
ICOMMAND(numslots, "", (), intret(slots.length()));
COMMAND(getslottex, "i");
ICOMMAND(texloaded, "i", (int *tex), intret(slots.inrange(*tex) && slots[*tex]->loaded ? 1 : 0));

void replacetexcube(cube &c, int oldtex, int newtex)
{
    for(int i = 0; i < 6; ++i) //for each face
    {
        if(c.texture[i] == oldtex)
        {
            c.texture[i] = newtex;
        }
    }
    //recursively apply to children
    if(c.children)
    {
        for(int i = 0; i < 8; ++i)
        {
            replacetexcube(c.children[i], oldtex, newtex);
        }
    }
}

void mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Replace, oldtex, newtex, insel ? 1 : 0);
    }
    if(insel)
    {
        LOOP_SEL_XYZ(replacetexcube(c, oldtex, newtex));
    }
    //recursively apply to children
    else
    {
        for(int i = 0; i < 8; ++i)
        {
            replacetexcube(worldroot[i], oldtex, newtex);
        }
    }
    allchanged();
}

bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf)
{
    if(!unpacktex(oldtex, buf, false))
    {
        return false;
    }
    EDITING_VSLOT(oldtex);
    if(!unpacktex(newtex, buf))
    {
        return false;
    }
    mpreplacetex(oldtex, newtex, insel, sel, false);
    return true;
}

/*replace: replaces one texture slot with the most recently queued one
 * Arguments:
 *  insel: toggles whether to apply to the whole map or just the part in selection
 * Returns:
 *  void
 */
void replace(bool insel)
{
    if(noedit())
    {
        return;
    }
    if(reptex < 0)
    {
        conoutf(Console_Error, "can only replace after a texture edit");
        return;
    }
    mpreplacetex(reptex, lasttex, insel, sel, true);
}


#undef EDITING_VSLOT


////////// flip and rotate ///////////////
static inline uint dflip(uint face)
{
    return face == faceempty ? face : 0x88888888 - (((face & 0xF0F0F0F0) >> 4) | ((face & 0x0F0F0F0F) << 4));
}

static inline uint cflip(uint face)
{
    return ((face&0xFF00FF00)>>8) | ((face&0x00FF00FF)<<8);
}

static inline uint rflip(uint face)
{
    return ((face&0xFFFF0000)>>16)| ((face&0x0000FFFF)<<16);
}

static inline uint mflip(uint face)
{
    return (face&0xFF0000FF) | ((face&0x00FF0000)>>8) | ((face&0x0000FF00)<<8);
}

void flipcube(cube &c, int d)
{
    swap(c.texture[d*2], c.texture[d*2+1]);
    c.faces[D[d]] = dflip(c.faces[D[d]]);
    c.faces[C[d]] = cflip(c.faces[C[d]]);
    c.faces[R[d]] = rflip(c.faces[R[d]]);
    if(c.children)
    {
        //recursively apply to children
        for(int i = 0; i < 8; ++i)
        {
            if(i&octadim(d))
            {
                swap(c.children[i], c.children[i-octadim(d)]);
            }
        }
        for(int i = 0; i < 8; ++i)
        {
            flipcube(c.children[i], d);
        }
    }
}

//reassign a quartet of cubes by rotating them 90 deg
static inline void rotatequad(cube &a, cube &b, cube &c, cube &d)
{
    cube t = a;
         a = b;
         b = c;
         c = d;
         d = t;
}

/*rotatecube: rotates a cube by a given number of 90 degree increments
 * Arguments:
 *  c: the cube to rotate
 *  d: the number of 90 degree clockwise increments to rotate by
 * Returns:
 *  void
 *
 * Note that the orientation of rotation is clockwise about the axis normal to
 * the current selection's selected face (using the left-hand rule)
 */
void rotatecube(cube &c, int d)
{
    c.faces[D[d]] = cflip(mflip(c.faces[D[d]]));
    c.faces[C[d]] = dflip(mflip(c.faces[C[d]]));
    c.faces[R[d]] = rflip(mflip(c.faces[R[d]]));
    swap(c.faces[R[d]], c.faces[C[d]]);
    //reassign textures
    swap(c.texture[2*R[d]], c.texture[2*C[d]+1]);
    swap(c.texture[2*C[d]], c.texture[2*R[d]+1]);
    swap(c.texture[2*C[d]], c.texture[2*C[d]+1]);
    //move child members
    if(c.children)
    {
        int row = octadim(R[d]);
        int col = octadim(C[d]);
        for(int i=0; i<=octadim(d); i+=octadim(d)) rotatequad
        (
            c.children[i+row],
            c.children[i],
            c.children[i+col],
            c.children[i+col+row]
        );
        //recursively apply to children
        for(int i = 0; i < 8; ++i)
        {
            rotatecube(c.children[i], d);
        }
    }
}

/*mpflip: flips a selection across a plane defined by the selection's face
 * Arguments:
 *  sel: the selection to be fliped
 *  local: whether to send a network message, true if created locally, false if
 *         from another client
 * Returns:
 *  void
 */
void mpflip(selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Flip);
        makeundo();
    }
    int zs = sel.s[DIMENSION(sel.orient)];
    LOOP_XY(sel)
    {
        for(int z = 0; z < zs; ++z)
        {
            flipcube(SELECT_CUBE(x, y, z), DIMENSION(sel.orient));
        }
        for(int z = 0; z < zs/2; ++z)
        {
            cube &a = SELECT_CUBE(x, y, z);
            cube &b = SELECT_CUBE(x, y, zs-z-1);
            swap(a, b);
        }
    }
    changed(sel);
}

void flip()
{
    if(noedit())
    {
        return;
    }
    mpflip(sel, true);
}

void mprotate(int cw, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Rotate, cw);
    }
    int d = DIMENSION(sel.orient);
    if(!DIM_COORD(sel.orient))
    {
        cw = -cw;
    }
    int m = sel.s[C[d]] < sel.s[R[d]] ? C[d] : R[d],
        ss = sel.s[m] = max(sel.s[R[d]], sel.s[C[d]]);
    if(local)
    {
        makeundo();
    }
    for(int z = 0; z < sel.s[D[d]]; ++z)
    {
        for(int i = 0; i < (cw>0 ? 1 : 3); ++i)
        {
            LOOP_XY(sel)
            {
                rotatecube(SELECT_CUBE(x,y,z), d);
            }
            for(int y = 0; y < ss/2; ++y)
            {
                for(int x = 0; x < ss-1-2*y; ++x)
                {
                    rotatequad
                    (
                        SELECT_CUBE(ss-1-y, x+y, z),
                        SELECT_CUBE(x+y, y, z),
                        SELECT_CUBE(y, ss-1-x-y, z),
                        SELECT_CUBE(ss-1-x-y, ss-1-y, z)
                    );
                }
            }
        }
    }
    changed(sel);
}

void rotate(int *cw)
{
    if(noedit())
    {
        return;
    }
    mprotate(*cw, sel, true);
}

COMMAND(flip, "");
COMMAND(rotate, "i");

ICOMMAND(replace, "", (), replace(false));
ICOMMAND(replacesel, "", (), replace(true));

/*mpeditmat: selectively fills a selection with selected materials
 * Arguments:
 *  matid: material id to fill
 *  filter: if nonzero, determines what existing mats to apply to
 *  sel: selection of cubes to fill
 *  local: whether to send a network message, true if created locally, false if
 *         from another client
 * Returns:
 *  void
 */
void mpeditmat(int matid, int filter, selinfo &sel, bool local)
{
    if(local)
    {
        game::edittrigger(sel, Edit_Mat, matid, filter);
    }

    ushort filtermat = 0,
           filtermask = 0,
           matmask;
    int filtergeom = 0;
    if(filter >= 0)
    {
        filtermat = filter&0xFF;
        filtermask = filtermat&(MatFlag_Volume|MatFlag_Index) ? MatFlag_Volume|MatFlag_Index : (filtermat&MatFlag_Clip ? MatFlag_Clip : filtermat);
        filtergeom = filter&~0xFF;
    }
    if(matid < 0)
    {
        matid = 0;
        matmask = filtermask;
        if(IS_CLIPPED(filtermat&MatFlag_Volume))
        {
            matmask &= ~MatFlag_Clip;
        }
    }
    else
    {
        matmask = matid&(MatFlag_Volume|MatFlag_Index) ? 0 : (matid&MatFlag_Clip ? ~MatFlag_Clip : ~matid);
        if(IS_CLIPPED(matid&MatFlag_Volume))
        {
            matid |= Mat_Clip;
        }
    }
    LOOP_SEL_XYZ(setmat(c, matid, matmask, filtermat, filtermask, filtergeom));
}

/*editmat: takes the globally defined selection and fills it with a material
 * Arguments:
 *  name: the name of the material (water, alpha, etc.)
 *  filtername: the name of the existing material to apply to
 * Returns:
 *  void
 */
void editmat(char *name, char *filtername)
{
    if(noedit())
    {
        return;
    }
    int filter = -1;
    if(filtername[0])
    {
        for(int i = 0; i < static_cast<int>(sizeof(editmatfilters)/sizeof(editmatfilters[0])); ++i)
        {
            if(!strcmp(editmatfilters[i].name, filtername))
            {
                filter = editmatfilters[i].filter;
                break;
            }
        }
        if(filter < 0)
        {
            filter = findmaterial(filtername);
        }
        if(filter < 0)
        {
            conoutf(Console_Error, "unknown material \"%s\"", filtername);
            return;
        }
    }
    int id = -1;
    if(name[0] || filter < 0)
    {
        id = findmaterial(name);
        if(id<0)
        {
            conoutf(Console_Error, "unknown material \"%s\"", name);
            return;
        }
    }
    mpeditmat(id, filter, sel, true);
}

COMMAND(editmat, "ss");

