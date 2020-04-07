#include "game.h"

namespace entities
{
    using namespace game;

    int extraentinfosize() { return 0; }       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
    {
    }

    void readent(entity &e, char *buf, int ver)     // read from disk, and init
    {
    }

#ifndef STANDALONE
    vector<extentity *> ents;

    vector<extentity *> &getents() { return ents; }

    bool mayattach(extentity &e) { return false; }
    bool attachent(extentity &e, extentity &a) { return false; }

    const char *itemname(int i)
    {
        return NULL;
#if 0
        int t = ents[i]->type;
        if(!VALID_ITEM(t)) return NULL;
        return itemstats[t-I_FIRST].name;
#endif
    }

    int itemicon(int i)
    {
        return -1;
#if 0
        int t = ents[i]->type;
        if(!VALID_ITEM(t)) return -1;
        return itemstats[t-I_FIRST].icon;
#endif
    }

    const char *entmdlname(int type)
    {
        static const char * const entmdlnames[GamecodeEnt_MaxEntTypes] =
        {
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            "game/teleport", NULL, NULL,
            NULL
        };
        return entmdlnames[type];
    }

    const char *entmodel(const entity &e)
    {
        if(e.type == GamecodeEnt_Teleport)
        {
            if(e.attr2 > 0) return mapmodelname(e.attr2);
            if(e.attr2 < 0) return NULL;
        }
        return e.type < GamecodeEnt_MaxEntTypes ? entmdlname(e.type) : NULL;
    }

    void preloadentities()
    {
        for(int i = 0; i < GamecodeEnt_MaxEntTypes; ++i)
        {
            const char *mdl = entmdlname(i);
            if(!mdl) continue;
            preloadmodel(mdl);
        }
        for(int i = 0; i < ents.length(); i++)
        {
            extentity &e = *ents[i];
            switch(e.type)
            {
                case GamecodeEnt_Teleport:
                    if(e.attr2 > 0) preloadmodel(mapmodelname(e.attr2));
                case GamecodeEnt_Jumppad:
                    if(e.attr4 > 0) preloadmapsound(e.attr4);
                    break;
            }
        }
    }

    void renderentities()
    {
        for(int i = 0; i < ents.length(); i++)
        {
            extentity &e = *ents[i];
            int revs = 10;
            switch(e.type)
            {
                case GamecodeEnt_Teleport:
                    if(e.attr2 < 0) continue;
                    break;
                default:
                    if(!e.spawned() || !VALID_ITEM(e.type)) continue;
                    break;
            }
            const char *mdlname = entmodel(e);
            if(mdlname)
            {
                vec p = e.o;
                p.z += 1+sinf(lastmillis/100.0+e.o.x+e.o.y)/20;
                rendermodel(mdlname, Anim_Mapmodel|ANIM_LOOP, p, lastmillis/(float)revs, 0, 0, MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
            }
        }
    }

    void addammo(int type, int &v, bool local)
    {
#if 0
        itemstat &is = itemstats[type-I_FIRST];
        v += is.add;
        if(v>is.max) v = is.max;
        if(local) msgsound(is.sound);
#endif
    }

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void pickupeffects(int n, gameent *d)
    {
#if 0
        if(!ents.inrange(n))
        {
            return;
        }
        int type = ents[n]->type;
        if(!VALID_ITEM(type))
        {
            return;
        }
        ents[n]->clearspawned();
        if(!d)
        {
            return;
        }
        itemstat &is = itemstats[type-I_FIRST];
        if(d!=player1 || isthirdperson())
        {
            //particle_text(d->abovehead(), is.name, PART_TEXT, 2000, 0xFFC864, 4.0f, -8);
            particle_icon(d->abovehead(), is.icon%4, is.icon/4, PART_HUD_ICON_GREY, 2000, 0xFFFFFF, 2.0f, -8);
        }
        playsound(itemstats[type-I_FIRST].sound, d!=player1 ? &d->o : NULL, NULL, 0, 0, 0, -1, 0, 1500);
        d->pickup(type);
#endif
    }

    // these functions are called when the client touches the item

    void teleporteffects(gameent *d, int tp, int td, bool local)
    {
        if(ents.inrange(tp) && ents[tp]->type == GamecodeEnt_Teleport)
        {
            extentity &e = *ents[tp];
            if(e.attr4 >= 0)
            {
                int snd = Sound_Teleport, flags = 0;
                if(e.attr4 > 0)
                {
                    snd = e.attr4; flags = SND_MAP;
                }
                if(d == player1)
                {
                    playsound(snd, NULL, NULL, flags);
                }
                else
                {
                    playsound(snd, &e.o, NULL, flags);
                    if(ents.inrange(td) && ents[td]->type == GamecodeEnt_Teledest) playsound(snd, &ents[td]->o, NULL, flags);
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
                    flags = SND_MAP;
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
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(GamecodeEnt_Teledest, e+1);
            if(e==beenhere || e<0)
            {
                conoutf(CON_WARN, "no teleport destination for channel %d", tag);
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
                d->vel = vec(ents[n]->attr3*10.0f, ents[n]->attr2*10.0f, ents[n]->attr1*12.5f);
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
        for(int i = 0; i < ents.length(); i++)
        {
            if(VALID_ITEM(ents[i]->type))
            {
                putint(p, i);
                putint(p, ents[i]->type);
            }
        }
        putint(p, -1);
    }

    void resetspawns()
    {
        for(int i = 0; i < ents.length(); i++)
        {
            ents[i]->clearspawned();
        }
    }

    void spawnitems(bool force)
    {
        for(int i = 0; i < ents.length(); i++)
        {
            if(VALID_ITEM(ents[i]->type))
            {
                ents[i]->setspawned(force || !server::delayspawn(ents[i]->type));
            }
        }
    }

    void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->setspawned(on); }

    extentity *newentity() { return new gameentity(); }
    void deleteentity(extentity *e) { delete (gameentity *)e; }

    void clearents()
    {
        while(ents.length())
        {
            deleteentity(ents.pop());
        }
    }

    void animatemapmodel(const extentity &e, int &anim, int &basetime)
    {
    }

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case GamecodeEnt_Flag:
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
            case GamecodeEnt_Teledest:
                e.attr3 = e.attr2;
                e.attr2 = e.attr1;
                e.attr1 = (int)player1->yaw;
                break;
        }
    }

    void entradius(extentity &e, bool color)
    {
        switch(e.type)
        {
            case GamecodeEnt_Teleport:
                for(int i = 0; i < ents.length(); i++)
                {
                    if(ents[i]->type == GamecodeEnt_Teledest && e.attr1==ents[i]->attr2)
                    {
                        renderentarrow(e, vec(ents[i]->o).sub(e.o).normalize(), e.o.dist(ents[i]->o));
                        break;
                    }
                }
                break;

            case GamecodeEnt_Jumppad:
                renderentarrow(e, vec((int)(char)e.attr3*10.0f, (int)(char)e.attr2*10.0f, e.attr1*12.5f).normalize(), 4);
                break;

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

    bool printent(extentity &e, char *buf, int len)
    {
        return false;
    }

    const char *entnameinfo(entity &e) { return ""; }
    const char *entname(int i)
    {
        static const char * const entnames[GamecodeEnt_MaxEntTypes] =
        {
            "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight", "decal",
            "teleport", "teledest", "jumppad",
            "flag"
        };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        //e.flags = 0;
        if(local)
        {
            addmsg(NetMsg_EditEnt, "rii3ii5", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
        }
    }

    float dropheight(entity &e)
    {
        if(e.type==GamecodeEnt_Flag)
        {
            return 0.0f;
        }
        return 4.0f;
    }
#endif
}

