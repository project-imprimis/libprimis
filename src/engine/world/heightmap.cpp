/* heightmap.cpp: terrain-like cube pushing functionality
 *
 * to help the creation of natural terrain like geometry with cubes, heightmapping
 * allows for convenient pushing and pulling of areas of geometry
 *
 * heightmapping is merely a different way of modifying cubes, and under the hood
 * (and to other clients) behaves like bulk modification of cubes; the underlying
 * geometry is still just cubes
 *
 * for this reason, heightmapping can be done along any of the main axes, though
 * only along one at a time
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"

#include "octaedit.h"
#include "octaworld.h"
#include "world.h"


class hmap
{
    public:
        void cancel()
        {
            textures.clear();
        }

        void hmapselect()
        {
            const cube &c = rootworld.lookupcube(cur);
            if(!c.texture)
            {
                return;
            }
            int t = c.texture[orient],
                i = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), t));
            if(i == std::distance(textures.begin(), textures.end()))
            {
                textures.push_back(t);
            }
            else
            {
                textures.erase(textures.begin() + i);
            }
        }

        bool isheightmap(int o, bool empty, const cube &c) const
        {
            return havesel ||
                (empty && c.isempty()) ||
                textures.empty() ||
                (std::find(textures.begin(), textures.end(), c.texture[o]) != textures.end());
        }

        void clearhbrush()
        {
            for(auto &i : brush)
            {
                i.fill(0);
            }
            brushmaxx = brushmaxy = 0;
            brushminx = brushminy = maxbrush;
            paintbrush = false;
        }

        void hbrushvert(const int *x, const int *y, const int * const v)
        {
            int x1, y1;
            x1 = *x + maxbrush/2 - brushx + 1; // +1 for automatic padding
            y1 = *y + maxbrush/2 - brushy + 1;
            if(x1<0 || y1<0 || x1>=maxbrush || y1>=maxbrush)
            {
                return;
            }
            brush[x1][y1] = std::clamp(*v, 0, 8);
            paintbrush = paintbrush || (brush[x1][y1] > 0);
            brushmaxx = std::min(maxbrush-1, std::max(brushmaxx, x1+1));
            brushmaxy = std::min(maxbrush-1, std::max(brushmaxy, y1+1));
            brushminx = std::max(0,          std::min(brushminx, x1-1));
            brushminy = std::max(0,          std::min(brushminy, y1-1));
        }

        void run(int dir, int mode)
        {
            d  = DIMENSION(sel.orient);
            dc = DIM_COORD(sel.orient);
            dcr= dc ? 1 : -1;
            dr = dir>0 ? 1 : -1;
         //   biasup = mode == dir<0;
            biasup = dir < 0;
            int cx = (sel.corner&1 ? 0 : -1),
                cy = (sel.corner&2 ? 0 : -1);
            hws= (rootworld.mapsize()>>gridpower);
            gx = (cur[R[d]] >> gridpower) + cx - maxbrush/2;
            gy = (cur[C[d]] >> gridpower) + cy - maxbrush/2;
            gz = (cur[D[d]] >> gridpower);
            fs = dc ? 4 : 0;
            fg = dc ? gridsize : -gridsize;
            mx = std::max(0, -gx); // ripple range
            my = std::max(0, -gy);
            nx = std::min(maxbrush-1, hws-gx) - 1;
            ny = std::min(maxbrush-1, hws-gy) - 1;
            if(havesel)
            {   // selection range
                bmx = mx = std::max(mx, (sel.o[R[d]]>>gridpower)-gx);
                bmy = my = std::max(my, (sel.o[C[d]]>>gridpower)-gy);
                bnx = nx = std::min(nx, (sel.s[R[d]]+(sel.o[R[d]]>>gridpower))-gx-1);
                bny = ny = std::min(ny, (sel.s[C[d]]+(sel.o[C[d]]>>gridpower))-gy-1);
            }
            bool paintme = paintbrush;
            if(havesel && mode<0) // -ve means smooth selection
            {
                paintme = false;
            }
            else
            {   // brush range
                bmx = std::max(mx, brushminx);
                bmy = std::max(my, brushminy);
                bnx = std::min(nx, brushmaxx-1);
                bny = std::min(ny, brushmaxy-1);
            }
            nz = rootworld.mapsize()-gridsize;
            mz = 0;
            hundo.s = ivec(d,1,1,5);
            hundo.orient = sel.orient;
            hundo.grid = gridsize;
            forcenextundo();

            changes.grid = gridsize;
            changes.s = changes.o = cur;
            std::memset(map, 0, sizeof map);
            std::memset(flags, 0, sizeof flags);

            selecting = true;
            select(std::clamp(maxbrush/2-cx, bmx, bnx),
                   std::clamp(maxbrush/2-cy, bmy, bny),
                   dc ? gz : hws - gz);
            selecting = false;
            if(paintme)
            {
                paint();
            }
            else
            {
                smooth();
            }
            rippleandset();                       // pull up points to cubify, and set
            changes.s.sub(changes.o).shr(gridpower).add(1);
            rootworld.changed(changes);
        }
    private:
        std::vector<int> textures;
        //max brush consts: number of cubes on end that can be heightmap brushed at once
        static constexpr int maxbrush  = 64,
                             maxbrushc = 63;

        std::array<std::array<int, maxbrush>, maxbrush> brush;//2d array of heights for heightmap brushs
        int brushx = variable("hbrushx", 0, maxbrush/2, maxbrush, &brushx, nullptr, 0); //max width for a brush
        int brushy = variable("hbrushy", 0, maxbrush/2, maxbrush, &brushy, nullptr, 0); //max length for a brush
        bool paintbrush = 0;
        int brushmaxx = 0,
            brushminx = maxbrush,
            brushmaxy = 0,
            brushminy = maxbrush;

        static constexpr int painted = 1,
                             nothmap = 2,
                             mapped  = 16;
        uchar  flags[maxbrush][maxbrush];
        cube   *cmap[maxbrushc][maxbrushc][4];
        int  mapz[maxbrushc][maxbrushc],
             map [maxbrush][maxbrush];

        selinfo changes;
        bool selecting; //flag used by select() for continuing to adj cubes
        int d,   //dimension
            dc,  //dimension coordinate
            dr,  //dimension sign
            dcr, //dimension coordinate sign
            biasup, //if dir < 0
            hws, //heightmap [gridpower] world size
            fg;  //+/- gridpower depending on dc
        int gx, gy, gz,
            mx, my, mz,
            nx, ny, nz,
            bmx, bmy,
            bnx, bny;
        uint fs; //face
        selinfo hundo;

        cube *getcube(ivec t, int f)
        {
            t[d] += dcr*f*gridsize;
            if(t[d] > nz || t[d] < mz)
            {
                return nullptr;
            }
            cube *c = &rootworld.lookupcube(t, gridsize);
            if(c->children)
            {
                forcemip(*c, false);
            }
            c->discardchildren(true);
            if(!isheightmap(sel.orient, true, *c))
            {
                return nullptr;
            }
            //x
            if     (t.x < changes.o.x) changes.o.x = t.x;
            else if(t.x > changes.s.x) changes.s.x = t.x;
            //y
            if     (t.y < changes.o.y) changes.o.y = t.y;
            else if(t.y > changes.s.y) changes.s.y = t.y;
            //z
            if     (t.z < changes.o.z) changes.o.z = t.z;
            else if(t.z > changes.s.z) changes.s.z = t.z;
            return c;
        }

        uint getface(const cube *c, int d) const
        {
            return  0x0f0f0f0f & ((dc ? c->faces[d] : 0x88888888 - c->faces[d]) >> fs);
        }

        void pushside(cube &c, int d, int x, int y, int z) const
        {
            ivec a;
            getcubevector(c, d, x, y, z, a);
            a[R[d]] = 8 - a[R[d]];
            setcubevector(c, d, x, y, z, a);
        }

        void addpoint(int x, int y, int z, int v)
        {
            if(!(flags[x][y] & mapped))
            {
                map[x][y] = v + (z*8);
            }
            flags[x][y] |= mapped;
        }

        void select(int x, int y, int z)
        {
            if((nothmap & flags[x][y]) || (painted & flags[x][y]))
            {
                return;
            }
            ivec t(d, x+gx, y+gy, dc ? z : hws-z);
            t.shl(gridpower);

            // selections may damage; must makeundo before
            hundo.o = t;
            hundo.o[D[d]] -= dcr*gridsize*2;
            makeundo(hundo);

            cube **c = cmap[x][y];
            for(int k = 0; k < 4; ++k)
            {
                c[k] = nullptr;
            }
            c[1] = getcube(t, 0);
            if(!c[1] || !(c[1]->isempty()))
            {   // try up
                c[2] = c[1];
                c[1] = getcube(t, 1);
                if(!c[1] || c[1]->isempty())
                {
                    c[0] = c[1];
                    c[1] = c[2];
                    c[2] = nullptr;
                }
                else
                {
                    z++;
                    t[d]+=fg;
                }
            }
            else // drop down
            {
                z--;
                t[d]-= fg;
                c[0] = c[1];
                c[1] = getcube(t, 0);
            }

            if(!c[1] || c[1]->isempty())
            {
                flags[x][y] |= nothmap;
                return;
            }
            flags[x][y] |= painted;
            mapz [x][y]  = z;
            if(!c[0])
            {
                c[0] = getcube(t, 1);
            }
            if(!c[2])
            {
                c[2] = getcube(t, -1);
            }
            c[3] = getcube(t, -2);
            c[2] = !c[2] || c[2]->isempty() ? nullptr : c[2];
            c[3] = !c[3] || c[3]->isempty() ? nullptr : c[3];

            uint face = getface(c[1], d);
            if(face == 0x08080808 && (!c[0] || !(c[0]->isempty())))
            {
                flags[x][y] |= nothmap;
                return;
            }
            if(c[1]->faces[R[d]] == facesolid)   // was single
            {
                face += 0x08080808;
            }
            else                                 // was pair
            {
                face += c[2] ? getface(c[2], d) : 0x08080808;
            }
            face += 0x08080808;                  // c[3]
            uchar *f = reinterpret_cast<uchar*>(&face);
            addpoint(x,   y,   z, f[0]);
            addpoint(x+1, y,   z, f[1]);
            addpoint(x,   y+1, z, f[2]);
            addpoint(x+1, y+1, z, f[3]);

            if(selecting) // continue to adjacent cubes
            {
                if(x>bmx)
                {
                    select(x-1, y, z);
                }
                if(x<bnx)
                {
                    select(x+1, y, z);
                }
                if(y>bmy)
                {
                    select(x, y-1, z);
                }
                if(y<bny)
                {
                    select(x, y+1, z);
                }
            }
        }

        void ripple(int x, int y, int z, bool force)
        {
            if(force)
            {
                select(x, y, z);
            }
            if((nothmap & flags[x][y]) || !(painted & flags[x][y]))
            {
                return;
            }

            bool changed = false;
            int *o[4], best, par,
                q = 0;
            for(int i = 0; i < 2; ++i)
            {
                for(int j = 0; j < 2; ++j)
                {
                    o[i+j*2] = &map[x+i][y+j];
                }
            }

            #define PULL_HEIGHTMAP(I, LT, GT, M, N, A) \
            do \
            { \
                best = I; \
                for(int i = 0; i < 4; ++i) \
                { \
                    if(*o[i] LT best) \
                    { \
                        best = *o[q = i] - M; \
                    } \
                } \
                par = (best&(~7)) + N; \
                /* dual layer for extra smoothness */ \
                if(*o[q^3] GT par && !(*o[q^1] LT par || *o[q^2] LT par)) \
                { \
                    if(*o[q^3] GT par A 8 || *o[q^1] != par || *o[q^2] != par) \
                    { \
                        *o[q^3] = (*o[q^3] GT par A 8 ? par A 8 : *o[q^3]); \
                        *o[q^1] = *o[q^2] = par; \
                        changed = true; \
                    } \
                /* single layer */ \
                } \
                else { \
                    for(int j = 0; j < 4; ++j) \
                    { \
                        if(*o[j] GT par) \
                        { \
                            *o[j] = par; \
                            changed = true; \
                        } \
                    } \
                } \
            } while(0)

            if(biasup)
            {
                PULL_HEIGHTMAP(0, >, <, 1, 0, -);
            }
            else
            {
                PULL_HEIGHTMAP(rootworld.mapsize()*8, <, >, 0, 8, +);
            }

            #undef PULL_HEIGHTMAP

            cube * const * const c  = cmap[x][y];
            int e[2][2],
                notempty = 0;

            for(int k = 0; k < 4; ++k)
            {
                if(c[k])
                {
                    for(int i = 0; i < 2; ++i)
                    {
                        for(int j = 0; j < 2; ++j)
                        {
                            {
                                e[i][j] = std::min(8, map[x+i][y+j] - (mapz[x][y]+3-k)*8);
                                notempty |= e[i][j] > 0;
                            }
                        }
                    }
                    if(notempty)
                    {
                        c[k]->texture[sel.orient] = c[1]->texture[sel.orient];
                        setcubefaces(*c[k], facesolid);
                        for(int i = 0; i < 2; ++i)
                        {
                            for(int j = 0; j < 2; ++j)
                            {
                                int f = e[i][j];
                                if(f<0 || (f==0 && e[1-i][j]==0 && e[i][1-j]==0))
                                {
                                    f=0;
                                    pushside(*c[k], d, i, j, 0);
                                    pushside(*c[k], d, i, j, 1);
                                }
                                EDGE_SET(CUBE_EDGE(*c[k], d, i, j), dc, dc ? f : 8-f);
                            }
                        }
                    }
                    else
                    {
                        setcubefaces(*c[k], faceempty);
                    }
                }
            }
            if(!changed)
            {
                return;
            }
            if(x>mx) ripple(x-1, y, mapz[x][y], true);
            if(x<nx) ripple(x+1, y, mapz[x][y], true);
            if(y>my) ripple(x, y-1, mapz[x][y], true);
            if(y<ny) ripple(x, y+1, mapz[x][y], true);

    //============================================================== DIAGONAL_RIPPLE
    #define DIAGONAL_RIPPLE(a,b,exp) \
        if(exp) { \
                if(flags[x a][ y] & painted) \
                { \
                    ripple(x a, y b, mapz[x a][y], true); \
                } \
                else if(flags[x][y b] & painted) \
                { \
                    ripple(x a, y b, mapz[x][y b], true); \
                } \
            }
            DIAGONAL_RIPPLE(-1, -1, (x>mx && y>my)); // do diagonals because adjacents
            DIAGONAL_RIPPLE(-1, +1, (x>mx && y<ny)); //    won't unless changed
            DIAGONAL_RIPPLE(+1, +1, (x<nx && y<ny));
            DIAGONAL_RIPPLE(+1, -1, (x<nx && y>my));
        }

    #undef DIAGONAL_RIPPLE
    //==============================================================================

        void paint()
        {
            for(int x=bmx; x<=bnx+1; x++)
            {
                for(int y=bmy; y<=bny+1; y++)
                {
                    map[x][y] -= dr * brush[x][y];
                }
            }
        }

        void smooth()
        {
            int sum, div;
            for(int x=bmx; x<=bnx-2; x++)
            {
                for(int y=bmy; y<=bny-2; y++)
                {
                    sum = 0;
                    div = 9;
                    for(int i = 0; i < 3; ++i)
                    {
                        for(int j = 0; j < 3; ++j)
                        {
                            if(flags[x+i][y+j] & mapped)
                            {
                                sum += map[x+i][y+j];
                            }
                            else
                            {
                                div--;
                            }
                        }
                    }
                    if(div)
                    {
                        map[x+1][y+1] = sum / div;
                    }
                }
            }
        }

        void rippleandset()
        {
            for(int x=bmx; x<=bnx; x++)
            {
                for(int y=bmy; y<=bny; y++)
                {
                    ripple(x, y, gz, false);
                }
            }
        }
} heightmapper;

// free functions wrappers of member functions to bind commands to
//imply existence of singleton instance of hmap
void hmapcancel()
{
    heightmapper.cancel();
}

void hmapselect()
{
    heightmapper.hmapselect();
}

void clearhbrush()
{
    heightmapper.clearhbrush();
}

void hbrushvert(const int *x, const int *y, const int *v)
{
    heightmapper.hbrushvert(x, y, v);
}

//engine interface functions
bool isheightmap(int o, bool empty, const cube &c)
{
    return heightmapper.isheightmap(o, empty, c);
}

void heightmaprun(int dir, int mode)
{
    heightmapper.run(dir, mode);
}

void initheightmapcmds()
{
    addcommand("hmapcancel", reinterpret_cast<identfun>(hmapcancel), "", Id_Command);
    addcommand("hmapselect", reinterpret_cast<identfun>(hmapselect), "", Id_Command);
    addcommand("clearhbrush", reinterpret_cast<identfun>(clearhbrush), "", Id_Command);
    addcommand("hbrushvert", reinterpret_cast<identfun>(hbrushvert), "iii", Id_Command);
}
