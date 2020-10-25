// weapon.cpp: all shooting and effects code, projectile management
#include "game.h"

namespace game
{
    static const int offsetmillis = 500;
    vec rays[MAXRAYS];

    struct hitmsg
    {
        int target, lifesequence, info1, info2;
        ivec dir;
    };
    vector<hitmsg> hits;

/*getweapon
 *returns the index of the weapon in hand
 * Arguments:
 *  none
 * Returns:
 *  index of weapon (zero-indexed)
 */
    ICOMMAND(getweapon, "", (), intret(player1->gunselect));

    void gunselect(int gun, gameent *d)
    {
        if(gun!=d->gunselect)
        {
            addmsg(NetMsg_GunSelect, "rci", d, gun);
            playsound(Sound_WeapLoad, d == player1 ? NULL : &d->o);
        }
        d->gunselect = gun;
    }

    void nextweapon(int dir, bool force = false)
    {
        if(player1->state!=ClientState_Alive)
        {
            return;
        }
        dir = (dir < 0 ? Gun_NumGuns-1 : 1);
        int gun = player1->gunselect;
        for(int i = 0; i < Gun_NumGuns; ++i)
        {
            gun = (gun + dir)%Gun_NumGuns;
            if(force || player1->ammo[gun])
            {
                break;
            }
        }
        if(gun != player1->gunselect)
        {
            gunselect(gun, player1);
        }
        else
        {
            playsound(Sound_NoAmmo);
        }
    }
/*nextweapon
 *changes player to an adjacent weapon, forwards if no dir is passed
 * Arguments:
 *  int *dir: direction (backwards if negative, forwards if positive)
 *  int *force: forces change if 1
 * Returns:
 *  void
 */
    ICOMMAND(nextweapon, "ii", (int *dir, int *force), nextweapon(*dir, *force!=0));

    int getweapon(const char *name)
    {
        if(isdigit(name[0]))
        {
            return parseint(name);
        }
        else
        {
            int len = strlen(name);
            for(int i = 0; i < static_cast<int>(sizeof(guns)/sizeof(guns[0])); ++i)
            {
                if(!strncasecmp(guns[i].name, name, len))
                {
                    return i;
                }
            }
        }
        return -1;
    }

    void setweapon(const char *name, bool force = false)
    {
        int gun = getweapon(name);
        if(player1->state!=ClientState_Alive || !validgun(gun))
        {
            return;
        }
        if(force || player1->ammo[gun])
        {
            gunselect(gun, player1);
        }
        else playsound(Sound_NoAmmo);
    }
    ICOMMAND(setweapon, "si", (char *name, int *force), setweapon(name, *force!=0));

    void cycleweapon(int numguns, int *guns, bool force = false)
    {
        if(numguns<=0 || player1->state!=ClientState_Alive)
        {
            return;
        }
        int offset = 0;
        for(int i = 0; i < numguns; ++i)
        {
            if(guns[i] == player1->gunselect)
            {
                offset = i+1;
                break;
            }
        }
        for(int i = 0; i < numguns; ++i)
        {
            int gun = guns[(i+offset)%numguns];
            if(gun>=0 && gun<Gun_NumGuns && (force || player1->ammo[gun]))
            {
                gunselect(gun, player1);
                return;
            }
        }
        playsound(Sound_NoAmmo);
    }
    ICOMMAND(cycleweapon, "V", (tagval *args, int numargs),
    {
         int numguns = min(numargs, 3);
         int guns[3];
         for(int i = 0; i < numguns; ++i)
         {
             guns[i] = getweapon(args[i].getstr());
         }
         cycleweapon(numguns, guns);
    });

    void weaponswitch(gameent *d)
    {
        if(d->state!=ClientState_Alive)
        {
            return;
        }
        int s = d->gunselect;
        if(s!=Gun_Pulse && d->ammo[Gun_Pulse])
        {
            s = Gun_Pulse;
        }
        else if(s!=Gun_Rail && d->ammo[Gun_Rail])
        {
            s = Gun_Rail;
        }
        gunselect(s, d);
    }

    ICOMMAND(weapon, "V", (tagval *args, int numargs),
    {
        if(player1->state!=ClientState_Alive)
        {
            return;
        }
        for(int i = 0; i < 3; ++i)
        {
            const char *name = i < numargs ? args[i].getstr() : "";
            if(name[0])
            {
                int gun = getweapon(name);
                if(validgun(gun) && gun != player1->gunselect && player1->ammo[gun])
                {
                    gunselect(gun, player1);
                    return;
                }
            }
            else
            {
                weaponswitch(player1);
                return;
            }
        }
        playsound(Sound_NoAmmo);
    });

    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest)
    {
        vec offset;
        do
        {
            offset = vec(randomfloat(1), randomfloat(1), randomfloat(1)).sub(0.5f);
        } while(offset.squaredlen() > 0.5f*0.5f);
        offset.mul((to.dist(from)/1024)*spread);
        offset.z /= 2;
        dest = vec(offset).add(to);
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, Ray_ClipMat|Ray_AlphaPoly);
        }
    }

    void createrays(int atk, const vec &from, const vec &to)             // create random spread of rays
    {
        for(int i = 0; i < attacks[atk].rays; ++i)
        {
            offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i]);
        }
    }

    enum
    {
        Bouncer_Gibs,
        Bouncer_Debris
    };

    struct bouncer : physent
    {
        int lifetime, bounces;
        float lastyaw, roll;
        bool local;
        gameent *owner;
        int bouncetype, variant;
        vec offset;
        int offsetmillis;
        int id;

        bouncer() : bounces(0), roll(0), variant(0)
        {
            type = PhysEnt_Bounce;
        }
    };

    vector<bouncer *> bouncers;

    void newbouncer(const vec &from, const vec &to, bool local, int id, gameent *owner, int type, int lifetime, int speed)
    {
        bouncer &bnc = *bouncers.add(new bouncer);
        bnc.o = from;
        bnc.radius = bnc.xradius = bnc.yradius = type==Bouncer_Debris ? 0.5f : 1.5f;
        bnc.eyeheight = bnc.radius;
        bnc.aboveeye = bnc.radius;
        bnc.lifetime = lifetime;
        bnc.local = local;
        bnc.owner = owner;
        bnc.bouncetype = type;
        bnc.id = local ? lastmillis : id;

        switch(type)
        {
            case Bouncer_Debris:
            {
                bnc.variant = randomint(4);
                break;
            }
            case Bouncer_Gibs:
            {
                bnc.variant = randomint(3);
                break;
            }
        }

        vec dir(to);
        dir.sub(from).safenormalize();
        bnc.vel = dir;
        bnc.vel.mul(speed);

        avoidcollision(&bnc, dir, owner, 0.1f);

        bnc.offset = from;
        bnc.offset.sub(bnc.o);
        bnc.offsetmillis = offsetmillis;

        bnc.resetinterp();
    }

    void bounced(physent *d, const vec &surface)
    {
        if(d->type != PhysEnt_Bounce)
        {
            return;
        }
        bouncer *b = (bouncer *)d;
        if(b->bouncetype != Bouncer_Gibs || b->bounces >= 2)
        {
            return;
        }
        b->bounces++;
        addstain(Stain_Blood, vec(b->o).sub(vec(surface).mul(b->radius)), surface, 2.96f/b->bounces, bvec(0x60, 0xFF, 0xFF), randomint(4));
    }

    void updatebouncers(int time)
    {
        for(int i = 0; i < bouncers.length(); i++) //∀ bouncers currently in the game
        {
            bouncer &bnc = *bouncers[i];
            vec old(bnc.o);
            bool stopped = false;
            // cheaper variable rate physics for debris, gibs, etc.
            for(int rtime = time; rtime > 0;)
            {
                int qtime = min(30, rtime);
                rtime -= qtime;
                //if bouncer has run out of lifetime, or bounce fxn returns true, turn on the stopping flag
                if((bnc.lifetime -= qtime)<0 || bounce(&bnc, qtime/1000.0f, 0.6f, 0.5f, 1))
                {
                    stopped = true;
                    break;
                }
            }
            if(stopped) //kill bouncer object if above check passes
            {
                delete bouncers.remove(i--);
            }
            else //time evolution
            {
                bnc.roll += old.sub(bnc.o).magnitude()/(4*RAD);
                bnc.offsetmillis = max(bnc.offsetmillis-time, 0);
            }
        }
    }

    void removebouncers(gameent *owner)
    {
        for(int i = 0; i < bouncers.length(); i++)
        {
            if(bouncers[i]->owner==owner)
            {
                delete bouncers[i];
                bouncers.remove(i--);
            }
        }
    }

    void clearbouncers()
    {
        bouncers.deletecontents();
    }

    struct projectile
    {
        vec dir, o, from, to, offset;
        float speed;
        gameent *owner;
        int atk;
        bool local;
        int offsetmillis;
        int id;
    };
    vector<projectile> projs;

    void clearprojectiles() { projs.shrink(0); }

    void newprojectile(const vec &from, const vec &to, float speed, bool local, int id, gameent *owner, int atk)
    {
        projectile &p = projs.add();
        p.dir = vec(to).sub(from).safenormalize();
        p.o = from;
        p.from = from;
        p.to = to;
        p.offset = hudgunorigin(attacks[atk].gun, from, to, owner);
        p.offset.sub(from);
        p.speed = speed;
        p.local = local;
        p.owner = owner;
        p.atk = atk;
        p.offsetmillis = offsetmillis;
        p.id = local ? lastmillis : id;
    }

    void removeprojectiles(gameent *owner)
    {
        int len = projs.length();
        for(int i = 0; i < len; ++i)
        {
            if(projs[i].owner==owner)
            {
                projs.remove(i--);
                len--;
            }
        }
    }

    VARP(blood, 0, 1, 1);

    void damageeffect(int damage, gameent *d, bool thirdperson)
    {
        vec p = d->o;
        p.z += 0.6f*(d->eyeheight + d->aboveeye) - d->eyeheight;
        if(blood)
        {
            particle_splash(Part_Blood, max(damage/10, randomint(3)+1), 1000, p, 0x60FFFF, 2.96f);
        }
    }

    void spawnbouncer(const vec &p, const vec &vel, gameent *d, int type)
    {
        vec to(randomint(100)-50, randomint(100)-50, randomint(100)-50); //x,y,z = [-50,50] to get enough steps to create a good random vector
        if(to.iszero())
        {
            to.z += 1; //if all three are zero (bad luck!), set vector to (0,0,1)
        }
        to.normalize(); //smash magnitude back to 1
        to.add(p); //add this random to input &p
        //newbouncer( from, to, local, id, owner,  type,        lifetime,             speed)
        newbouncer(   p,    to,  true,  0,  d,     type, randomint(1000)+1000, randomint(100)+20);
    }

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2 = 1)
    {
        if(at==player1 && d!=at)
        {
            extern int hitsound;
            if(hitsound && lasthit != lastmillis)
            {
                playsound(Sound_Hit);
            }
            lasthit = lastmillis;
        }

        gameent *f = (gameent *)d;

        f->lastpain = lastmillis;
        if(at->type==PhysEnt_Player && !modecheck(gamemode, Mode_Team) && at->team != f->team)
        {
            at->totaldamage += damage;
        }

        if(modecheck(gamemode, Mode_LocalOnly) || f==at)
        {
            f->hitpush(damage, vel, at, atk);
        }

        if(modecheck(gamemode, Mode_LocalOnly))
        {
            damaged(damage, f, at);
        }
        else
        {
            hitmsg &h = hits.add();
            h.target = f->clientnum;
            h.lifesequence = f->lifesequence;
            h.info1 = static_cast<int>(info1*DMF);
            h.info2 = info2;
            h.dir = f==at ? ivec(0, 0, 0) : ivec(vec(vel).mul(DNF));
            if(at==player1)
            {
                damageeffect(damage, f);
                if(f==player1)
                {
                    damageblend(damage);
                    damagecompass(damage, at ? at->o : f->o);
                    playsound(Sound_Pain2);
                }
                else
                {
                    playsound(Sound_Pain1, &f->o);
                }
            }
        }
    }

    void hitpush(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays)
    {
        hit(damage, d, at, vec(to).sub(from).safenormalize(), atk, from.dist(to), rays);
    }

    float projdist(dynent *o, vec &dir, const vec &v, const vec &vel)
    {
        vec middle = o->o;
        middle.z += (o->aboveeye-o->eyeheight)/2;
        dir = vec(middle).sub(v).add(vec(vel).mul(5)).safenormalize();

        float low = min(o->o.z - o->eyeheight + o->radius, middle.z),
              high = max(o->o.z + o->aboveeye - o->radius, middle.z);
        vec closest(o->o.x, o->o.y, std::clamp(v.z, low, high));
        return max(closest.dist(v) - o->radius, 0.0f);
    }

    void radialeffect(dynent *o, const vec &v, const vec &vel, int qdam, gameent *at, int atk)
    {
        if(o->state!=ClientState_Alive)
        {
            return;
        }
        vec dir;
        float dist = projdist(o, dir, v, vel);
        if(dist<attacks[atk].exprad)
        {
            float damage = qdam*(1-dist/EXP_DISTSCALE/attacks[atk].exprad);
            if(o==at)
            {
                damage /= EXP_SELFDAMDIV;
            }
            if(damage > 0)
            {
                hit(max(static_cast<int>(damage), 1), o, at, dir, atk, dist);
            }
        }
    }

    void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int damage, int atk)
    {
        particle_splash(Part_Spark, 200, 300, v, 0x50CFE5, 0.45f);
        playsound(Sound_PulseExplode, &v);
        particle_fireball(v, 1.15f*attacks[atk].exprad, Part_PulseBurst, static_cast<int>(attacks[atk].exprad*20), 0x50CFE5, 4.0f);
        vec debrisorigin = vec(v).sub(vec(vel).mul(5));
        adddynlight(safe ? v : debrisorigin, 2*attacks[atk].exprad, vec(1.0f, 3.0f, 4.0f), 350, 40, 0, attacks[atk].exprad/2, vec(0.5f, 1.5f, 2.0f));

        if(!local)
        {
            return;
        }
        int numdyn = numdynents;
        for(int i = 0; i < numdyn; ++i)
        {
            dynent *o = iterdynents(i);
            if(o->o.reject(v, o->radius + attacks[atk].exprad) || o==safe)
            {
                continue;
            }
            radialeffect(o, v, vel, damage, owner, atk);
        }
    }

    void pulsestain(const projectile &p, const vec &pos)
    {
        vec dir = vec(p.dir).neg();
        float rad = attacks[p.atk].exprad*0.75f;
        addstain(Stain_PulseScorch, pos, dir, rad);
        addstain(Stain_PulseGlow, pos, dir, rad, 0x50CFE5);
    }

    void projsplash(projectile &p, const vec &v, dynent *safe)
    {
        explode(p.local, p.owner, v, p.dir, safe, attacks[p.atk].damage, p.atk);
        pulsestain(p, v);
    }

    void explodeeffects(int atk, gameent *d, bool local, int id)
    {
        if(local)
        {
            return;
        }
        switch(atk)
        {
            case Attack_PulseShoot: //pulse rifle is currently the only weapon to do this
            {
                for(int i = 0; i < projs.length(); i++)
                {
                    projectile &p = projs[i];
                    if(p.atk == atk && p.owner == d && p.id == id && !p.local)
                    {
                        vec pos = vec(p.offset).mul(p.offsetmillis/static_cast<float>(offsetmillis)).add(p.o);
                        explode(p.local, p.owner, pos, p.dir, NULL, 0, atk);
                        pulsestain(p, pos);
                        projs.remove(i);
                        break;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
    /*projdamage: checks if projectile damages a particular dynent
     * Arguments:
     *  o: dynent (player ent) to check damage for
     *  p: projectile object to attempt to damage with
     *  v: the displacement vector that the projectile is currently stepping over
     * Returns:
     *  (bool) true if projectile damages dynent, false otherwise
     */
    bool projdamage(dynent *o, projectile &p, const vec &v)
    {
        if(o->state!=ClientState_Alive) //do not beat dead horses (or clients)
        {
            return false;
        }
        if(!intersect(o, p.o, v, attacks[p.atk].margin)) //do not damange unless collided
        {
            return false;
        }
        projsplash(p, v, o); //check splash
        vec dir;
        projdist(o, dir, v, p.dir);
        hit(attacks[p.atk].damage, o, p.owner, dir, p.atk, 0);
        return true;
    }

    /*explodecubes: deletes some cubes at a world vector location
     * Arguments:
     *  loc: world vector to destroy
     *  gridpower: size of cube to blow up
     * Returns:
     *  void
     */
    void explodecubes(ivec loc, int gridpower, int bias = 1)
    {
        int gridpow = static_cast<int>(pow(2,gridpower));
        //define selection boundaries that align with gridpower
        ivec minloc( loc.x - loc.x % gridpow -2*gridpow,
                     loc.y - loc.y % gridpow -2*gridpow,
                     loc.z - loc.z % gridpow -(2-bias)*gridpow);
        ivec maxlocz(3,3,4);
        ivec maxlocy(3,5,2);
        ivec maxlocx(5,3,2);
        selinfo sel;
        sel.o = minloc + ivec(gridpow,gridpow,0);
        sel.s = maxlocz;
        mpdelcube(sel, true);
        sel.o = minloc + ivec(gridpow,0,gridpow);
        sel.s = maxlocy;
        mpdelcube(sel, true);
        sel.o = minloc + ivec(0,gridpow,gridpow);
        sel.s = maxlocx;
        mpdelcube(sel, true);
    }

    /*placecube: places a cube at a world vector location
     * Arguments:
     *  loc: world vector to fill
     *  gridpower: size of cube to place
     * Returns:
     *  void
     */
    void placecube(ivec loc, int gridpower)
    {
        int gridpow = static_cast<int>(pow(2,gridpower));
        ivec minloc( loc.x - loc.x % gridpow,
                     loc.y - loc.y % gridpow,
                     loc.z - loc.z % gridpow );
        selinfo sel;
        sel.o = minloc;
        sel.s = ivec(1,1,1);
        mpplacecube(sel, 1, true);
    }

    void updateprojectiles(int time)
    {
        if(projs.empty())
        {
            return;
        }
        gameent *noside = hudplayer();
        for(int i = 0; i < projs.length(); i++) //loop through all projectiles in the game
        {
            projectile &p = projs[i];
            p.offsetmillis = max(p.offsetmillis-time, 0);
            vec dv; //displacement vector
            float dist = p.to.dist(p.o, dv);
            dv.mul(time/max(dist*1000/p.speed, static_cast<float>(time)));
            vec v = vec(p.o).add(dv); //set v as current particle location o plus dv
            bool exploded = false;
            hits.setsize(0);
            if(p.local) //if projectile belongs to a local client
            {
                vec halfdv = vec(dv).mul(0.5f), bo = vec(p.o).add(halfdv); //half the displacement vector halfdv; set bo like v except with halfdv
                float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[p.atk].margin;
                for(int j = 0; j < numdynents; ++j)
                {
                    dynent *o = iterdynents(j); //start by setting cur to current dynent in loop
                    //check if dynent in question is the owner of the projectile or is within the bounds of some other dynent (actor)
                    //if projectile is owned by a player or projectile is not within the bounds of a dynent, skip explode check
                    if(p.owner==o || o->o.reject(bo, o->radius + br))
                    {
                        continue;
                    }
                    if(projdamage(o, p, v))
                    {
                        exploded = true;
                        break;
                    } //damage check
                }
            }
            if(!exploded) //if we haven't already hit somebody, start checking for collisions with cube geometry
            {
                if(dist<4) // dist is the distance to the `to` location
                {
                    if(p.o!=p.to) // if original target was moving, reevaluate endpoint
                    {
                        if(raycubepos(p.o, p.dir, p.to, 0, Ray_ClipMat|Ray_AlphaPoly)>=4)
                        {
                            continue;
                        }
                    }
                    switch(attacks[p.atk].worldfx)
                    {
                        case 1:
                        {
                            explodecubes(static_cast<ivec>(p.o),3);
                            break;
                        }
                        case 2:
                        {
                            placecube(static_cast<ivec>(p.o),3);
                            break;
                        }
                    }
                    projsplash(p, v, NULL);
                    exploded = true;
                }
                else
                {
                    vec pos = vec(p.offset).mul(p.offsetmillis/static_cast<float>(offsetmillis)).add(v);
                    particle_splash(Part_PulseFront, 1, 1, pos, 0x50CFE5, 2.4f, 150, 20);
                    if(p.owner != noside) //noside is the hud player, so if the projectile is somebody else's
                    {
                        float len = min(20.0f, vec(p.offset).add(p.from).dist(pos)); //projectiles are at least 20u long
                        vec dir = vec(dv).normalize(),
                            tail = vec(dir).mul(-len).add(pos), //tail extends >=20u behind projectile point
                            head = vec(dir).mul(2.4f).add(pos); // head extends 2.4u ahead
                        particle_flare(tail, head, 1, Part_PulseSide, 0x50CFE5, 2.5f);
                    }
                }
            }
            if(exploded)
            {
                if(p.local)
                {
                    addmsg(NetMsg_Explode, "rci3iv", p.owner, lastmillis-maptime, p.atk, p.id-maptime,
                            hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf()); //sizeof int should always be 4 bytes
                }
                projs.remove(i--);
            }
            else
            {
                p.o = v; //if no collision stuff happened set the new position
            }
        }
    }

    /*railhit: creates a hitscan beam between points
     * Arguments:
     *  from: the origin location
     *  to: the destination location
     *  stain: whether to stain the hit point
     * Returns:
     *  void
     */
    void railhit(const vec &from, const vec &to, bool stain = true)
    {
        vec dir = vec(from).sub(to).safenormalize();
        if(stain)
        {
            addstain(Stain_RailHole, to, dir, 2.0f);
            addstain(Stain_RailGlow, to, dir, 2.5f, 0x50CFE5);
        }
        adddynlight(vec(to).madd(dir, 4), 10, vec(0.25f, 0.75f, 1.0f), 225, 75);
    }

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun;
        switch(atk)
        {
            case Attack_PulseShoot:
            {
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, Part_PulseMuzzleFlash, 0x50CFE5, 3.50f, d); //place a light that runs with the shot projectile
                }
                newprojectile(from, to, attacks[atk].projspeed, local, id, d, atk);
                break;
            }
            case Attack_EngShoot:
            {
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 250, Part_PulseMuzzleFlash, 0x50CFE5, 3.50f, d); //place a light that runs with the shot projectile
                }
                newprojectile(from, to, attacks[atk].projspeed, local, id, d, atk);
                break;
            }
            case Attack_RailShot:
            {
                particle_splash(Part_Spark, 200, 250, to, 0x50CFE5, 0.45f);
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, Part_RailTrail, 0x50CFE5, 0.5f);
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, Part_RailMuzzleFlash, 0x50CFE5, 2.75f, d);
                }
                adddynlight(hudgunorigin(gun, d->o, to, d), 35, vec(0.25f, 0.75f, 1.0f), 75, 75, DynLight_Flash, 0, vec(0, 0, 0), d); //place a light for the muzzle flash
                if(!local)
                {
                    railhit(from, to);
                }
                break;
            }
            default:
            {
                break;
            }
        }

        if(d==hudplayer())
        {
            playsound(attacks[atk].hudsound, NULL);
        }
        else
        {
            playsound(attacks[atk].sound, &d->o);
        }
    }

    float intersectdist = 1e16f;

    bool intersect(dynent *d, const vec &from, const vec &to, float margin, float &dist)   // if lineseg hits entity bounding box
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight + margin;
        top.z += d->aboveeye + margin;
        return linecylinderintersect(from, to, bottom, top, d->radius + margin, dist);
    }

    dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin, float &bestdist)
    {
        dynent *best = NULL;
        bestdist = 1e16f;
        for(int i = 0; i < numdynents; ++i)
        {
            dynent *o = iterdynents(i);
            if(o==at || o->state!=ClientState_Alive)
            {
                continue;
            }
            float dist;
            if(!intersect(o, from, to, margin, dist))
            {
                continue;
            }
            if(dist<bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
        return best;
    }

    void shorten(const vec &from, vec &target, float dist)
    {
        target.sub(from).mul(min(1.0f, dist)).add(from);
    }

    void raydamage(vec &from, vec &to, gameent *d, int atk)
    {
        dynent *o;
        float dist;
        int maxrays = attacks[atk].rays,
            margin = attacks[atk].margin;
        if(attacks[atk].rays > 1)
        {
            dynent *hits[MAXRAYS];
            for(int i = 0; i < maxrays; ++i)
            {
                if((hits[i] = intersectclosest(from, rays[i], d, margin, dist)))
                {
                    shorten(from, rays[i], dist);
                    railhit(from, rays[i], false);
                }
                else
                {
                    railhit(from, rays[i]);
                }
            }
            for(int i = 0; i < maxrays; ++i)
            {
                if(hits[i])
                {
                    o = hits[i];
                    hits[i] = NULL;
                    int numhits = 1;
                    for(int j = i+1; j < maxrays; j++)
                    {
                        if(hits[j] == o)
                        {
                            hits[j] = NULL;
                            numhits++;
                        }
                    }
                    hitpush(numhits*attacks[atk].damage, o, d, from, to, atk, numhits);
                }
            }
        }
        else if((o = intersectclosest(from, to, d, margin, dist)))
        {
            shorten(from, to, dist);
            railhit(from, to, false);
            hitpush(attacks[atk].damage, o, d, from, to, atk, 1);
        }
        else if(attacks[atk].action!=Act_Melee)
        {
            railhit(from, to);
        }
    }

    void shoot(gameent *d, const vec &targ)
    {
        int prevaction = d->lastaction, attacktime = lastmillis-prevaction;
        if(attacktime<d->gunwait)
        {
            return;
        }
        d->gunwait = 0;
        if(!d->attacking)
        {
            return;
        }
        int gun = d->gunselect,
            act = d->attacking,
            atk = guns[gun].attacks[act];
        d->lastaction = lastmillis;
        d->lastattack = atk;
        if(!d->ammo[gun])
        {
            if(d==player1)
            {
                msgsound(Sound_NoAmmo, d);
                d->gunwait = 600;
                d->lastattack = -1;
                weaponswitch(d);
            }
            return;
        }
        d->ammo[gun] -= attacks[atk].use;

        vec from = d->o,
            to = targ,
            dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        if(!(d->physstate >= PhysEntState_Slope && d->crouching && d->crouched()))
        {
            vec kickback = vec(dir).mul(attacks[atk].kickamount*-2.5f);
            d->vel.add(kickback);
        }
        float shorten = attacks[atk].range && dist > attacks[atk].range ? attacks[atk].range : 0,
              barrier = raycube(d->o, dir, dist, Ray_ClipMat|Ray_AlphaPoly);
        if(barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
        {
            shorten = barrier;
        }
        if(shorten)
        {
            to = vec(dir).mul(shorten).add(from);
        }

        if(attacks[atk].rays > 1)
        {
            createrays(atk, from, to);
        }
        else if(attacks[atk].spread)
        {
            offsetray(from, to, attacks[atk].spread, attacks[atk].range, to);
        }
        hits.setsize(0);

        if(!attacks[atk].projspeed)
        {
            raydamage(from, to, d, atk);
        }
        shoteffects(atk, from, to, d, true, 0, prevaction);

        if(d==player1 || d->ai)
        {
            addmsg(NetMsg_Shoot, "rci2i6iv", d, lastmillis-maptime, atk,
                   static_cast<int>(from.x*DMF), static_cast<int>(from.y*DMF), static_cast<int>(from.z*DMF),
                   static_cast<int>(to.x*DMF),   static_cast<int>(to.y*DMF),   static_cast<int>(to.z*DMF),
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf()); //sizeof int should always equal 4 (bytes) = 32b
        }

        d->gunwait = attacks[atk].attackdelay;
        if(attacks[atk].action != Act_Melee && d->ai)
        {
            d->gunwait += static_cast<int>(d->gunwait*(((101-d->skill)+randomint(111-d->skill))/100.f));
        }
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
    }

    void adddynlights()
    {
        for(int i = 0; i < projs.length(); i++)
        {
            projectile &p = projs[i];
            if(p.atk!=Attack_PulseShoot)
            {
                continue;
            }
            vec pos(p.o);
            pos.add(vec(p.offset).mul(p.offsetmillis/static_cast<float>(offsetmillis)));
            adddynlight(pos, 20, vec(0.25f, 0.75f, 1.0f));
        }
    }

    void renderbouncers()
    {
        float yaw, pitch;
        for(int i = 0; i < bouncers.length(); i++)
        {
            bouncer &bnc = *bouncers[i];
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/static_cast<float>(offsetmillis)));
            vec vel(bnc.vel);
            if(vel.magnitude() <= 25.0f)
            {
                yaw = bnc.lastyaw;
            }
            else
            {
                vectoyawpitch(vel, yaw, pitch);
                yaw += 90;
                bnc.lastyaw = yaw;
            }
            pitch = -bnc.roll;
            const char *mdl = NULL;
            int cull = Model_CullVFC|Model_CullDist|Model_CullOccluded;
            float fade = 1;
            if(bnc.lifetime < 250)
            {
                fade = bnc.lifetime/250.0f;
            }
            switch(bnc.bouncetype)
            {
                default:
                {
                    continue;
                }
            }
            rendermodel(mdl, Anim_Mapmodel | Anim_Loop, pos, yaw, pitch, 0, cull, NULL, NULL, 0, 0, fade);
        }
    }

    void removeweapons(gameent *d)
    {
        removebouncers(d);
        removeprojectiles(d);
    }

    void updateweapons(int curtime)
    {
        updateprojectiles(curtime);
        if(player1->clientnum>=0 && player1->state==ClientState_Alive)
        {
            shoot(player1, worldpos); // only shoot when connected to server
        }
        updatebouncers(curtime); // need to do this after the player shoots so bouncers don't end up inside player's BB next frame
    }

    void avoidweapons(ai::avoidset &obstacles, float radius)
    {
        for(int i = 0; i < projs.length(); i++)
        {
            projectile &p = projs[i];
            obstacles.avoidnear(NULL, p.o.z + attacks[p.atk].exprad + 1, p.o, radius + attacks[p.atk].exprad);
        }
    }
};

