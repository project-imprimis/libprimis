/* material.cpp: octree handled volume-based region flagging
 *
 * the material system in libprimis relies on the octree system; as a result all
 * material volumes are compositions of rectangular prisms
 *
 * regions of the octree world can be flagged as containing specific "materials"
 * some of these are rendered and visible (glass, water) while some are not visible
 * to users directly
 *
 * nonvisible materials influence how actors interact with the world: for example,
 * clipping and noclipping materials affect collision (by either creating invisible
 * walls or causing the engine to ignore collision with surfaces).
 *
 * the material data is saved in world files along with the octree geometry (see
 * worldio.cpp)
 */

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"

#include "material.h"
#include "octaedit.h"
#include "octaworld.h"
#include "world.h"

#include "render/octarender.h"
#include "render/rendergl.h"
#include "render/renderlights.h"
#include "render/renderva.h"
#include "render/water.h"
#include "render/shader.h"
#include "render/shaderparam.h"
#include "render/texture.h"

std::array<std::vector<materialsurface>, 4> watersurfs, waterfallsurfs, glasssurfs;

//internally relevant functionality

namespace
{
    std::vector<materialsurface> editsurfs;

    class QuadNode
    {
        public:
            QuadNode(int x, int y, int size) : x(x), y(y), size(size), filled(0)
            {
                for(int i = 0; i < 4; ++i)
                {
                    child[i] = 0;
                }
            }

            ~QuadNode()
            {
                clear();
            }

            void insert(int mx, int my, int msize)
            {
                if(size == msize)
                {
                    filled = 0xF;
                    return;
                }
                int csize = size>>1,
                    i = 0;
                if(mx >= x+csize)
                {
                    i |= 1;
                }
                if(my >= y+csize)
                {
                    i |= 2;
                }
                if(csize == msize)
                {
                    filled |= (1 << i);
                    return;
                }
                if(!child[i])
                {
                    child[i] = new QuadNode(i&1 ? x+csize : x, i&2 ? y+csize : y, csize);
                }
                child[i]->insert(mx, my, msize);
                for(int j = 0; j < 4; ++j)
                {
                    if(child[j])
                    {
                        if(child[j]->filled == 0xF)
                        {
                            if(child[j])
                            {
                                delete child[j];
                                child[j] = nullptr;
                            }
                            filled |= (1 << j);
                        }
                    }
                }
            }

            void genmatsurfs(ushort mat, uchar orient, uchar visible, int z, materialsurface *&matbuf)
            {
                if(filled == 0xF)
                {
                    genmatsurf(mat, orient, visible, x, y, z, size, matbuf);
                }
                else if(filled)
                {
                    int csize = size>>1;
                    for(int i = 0; i < 4; ++i)
                    {
                        if(filled & (1 << i))
                        {
                            genmatsurf(mat, orient, visible, i&1 ? x+csize : x, i&2 ? y+csize : y, z, csize, matbuf);
                        }

                    }
                }
                for(int i = 0; i < 4; ++i)
                {
                    if(child[i])
                    {
                        child[i]->genmatsurfs(mat, orient, visible, z, matbuf);
                    }
                }
            }
        private:
            int x, y, size;
            uint filled;
            std::array<QuadNode *, 4> child;

            void clear()
            {
                for(int i = 0; i < 4; ++i)
                {
                    if(child[i])
                    {
                        delete child[i];
                        child[i] = nullptr;
                    }
                }
            }

            void genmatsurf(ushort mat, uchar orient, uchar visible, int x, int y, int z, int size, materialsurface *&matbuf)
            {
                materialsurface &m = *matbuf++;
                m.material = mat;
                m.orient = orient;
                m.visible = visible;
                m.csize = size;
                m.rsize = size;
                int dim = DIMENSION(orient);
                m.o[C[dim]] = x;
                m.o[R[dim]] = y;
                m.o[dim] = z;
            }
    };

    static void drawmaterial(const materialsurface &m, float offset)
    {
        if(gle::attribbuf.empty())
        {
            gle::defvertex();
            gle::begin(GL_TRIANGLE_FAN);
        }
        float x = m.o.x,
              y = m.o.y,
              z = m.o.z,
              csize = m.csize,
              rsize = m.rsize;
        switch(m.orient)
        {
            case 0:
            {
                gle::attribf(x + offset, y + rsize, z + csize);
                gle::attribf(x + offset, y + rsize, z);
                gle::attribf(x + offset, y, z);
                gle::attribf(x + offset, y, z + csize);
                break;
            }
            case 1:
            {
                gle::attribf(x - offset, y + rsize, z + csize);
                gle::attribf(x - offset, y, z + csize);
                gle::attribf(x - offset, y, z);
                gle::attribf(x - offset, y + rsize, z);
                break;
            }
            case 2:
            {
                gle::attribf(x + csize, y + offset, z + rsize);
                gle::attribf(x, y + offset, z + rsize);
                gle::attribf(x, y + offset, z);
                gle::attribf(x + csize, y + offset, z);
                break;
            }
            case 3:
            {
                gle::attribf(x, y - offset, z);
                gle::attribf(x, y - offset, z + rsize);
                gle::attribf(x + csize, y - offset, z + rsize);
                gle::attribf(x + csize, y - offset, z);
                break;
            }
            case 4:
            {
                gle::attribf(x, y, z + offset);
                gle::attribf(x, y + csize, z + offset);
                gle::attribf(x + rsize, y + csize, z + offset);
                gle::attribf(x + rsize, y, z + offset);
                break;
            }
            case 5:
            {
                gle::attribf(x, y, z - offset);
                gle::attribf(x + rsize, y, z - offset);
                gle::attribf(x + rsize, y + csize, z - offset);
                gle::attribf(x, y + csize, z - offset);
                break;
            }
        }
        gle::end();
    }

    const struct material
    {
        const char *name;
        ushort id;
    } materials[] =
    {
        {"air", Mat_Air},
        {"water", Mat_Water}, {"water1", Mat_Water}, {"water2", Mat_Water+1}, {"water3", Mat_Water+2}, {"water4", Mat_Water+3},
        {"glass", Mat_Glass}, {"glass1", Mat_Glass}, {"glass2", Mat_Glass+1}, {"glass3", Mat_Glass+2}, {"glass4", Mat_Glass+3},
        {"clip", Mat_Clip},
        {"noclip", Mat_NoClip},
        {"gameclip", Mat_GameClip},
        {"death", Mat_Death},
        {"alpha", Mat_Alpha}
    };

    int visiblematerial(const cube &c, int orient, const ivec &co, int size, ushort matmask = MatFlag_Volume)
    {
        ushort mat = c.material&matmask;
        switch(mat)
        {
            case Mat_Air:
            {
                 break;
            }
            case Mat_Water:
            {
                if(visibleface(c, orient, co, size, mat, Mat_Air, matmask))
                {
                    return (orient != Orient_Bottom ? MatSurf_Visible : MatSurf_EditOnly);
                }
                break;
            }
            case Mat_Glass:
            {
                if(visibleface(c, orient, co, size, Mat_Glass, Mat_Air, matmask))
                {
                    return MatSurf_Visible;
                }
                break;
            }
            default:
            {
                if(visibleface(c, orient, co, size, mat, Mat_Air, matmask))
                {
                    return MatSurf_EditOnly;
                }
                break;
            }
        }
        return MatSurf_NotVisible;
    }

    bool mergematcmp(const materialsurface &x, const materialsurface &y)
    {
        int dim = DIMENSION(x.orient),
            c   = C[dim],
            r   = R[dim];
        if(x.o[r] + x.rsize < y.o[r] + y.rsize)
        {
            return true;
        }
        if(x.o[r] + x.rsize > y.o[r] + y.rsize)
        {
            return false;
        }
        return x.o[c] < y.o[c];
    }

    //m: pointer to array of materialsurfaces
    //sz: size of the array passed
    int mergematr(materialsurface *m, int sz, materialsurface &n)
    {
        int dim = DIMENSION(n.orient),
            c = C[dim],
            r = R[dim];
        for(int i = sz-1; i >= 0; --i)
        {
            if(m[i].o[r] + m[i].rsize < n.o[r])
            {
                break;
            }
            if(m[i].o[r] + m[i].rsize == n.o[r] && m[i].o[c] == n.o[c] && m[i].csize == n.csize)
            {
                n.o[r] = m[i].o[r];
                n.rsize += m[i].rsize;
                std::memmove(&m[i], &m[i+1], (sz - (i+1)) * sizeof(materialsurface));
                return 1;
            }
        }
        return 0;
    }

    int mergematc(const materialsurface &m, materialsurface &n)
    {
        int dim = DIMENSION(n.orient),
            c   = C[dim],
            r   = R[dim];
        if(m.o[r] == n.o[r] && m.rsize == n.rsize && m.o[c] + m.csize == n.o[c])
        {
            n.o[c] = m.o[c];
            n.csize += m.csize;
            return 1;
        }
        return 0;
    }

    //m: pointer to array of materialsurfaces
    //sz: size of the array passed
    int mergemat(materialsurface *m, int sz, materialsurface &n)
    {
        for(bool merged = false; sz; merged = true)
        {
            int rmerged = mergematr(m, sz, n);
            sz -= rmerged;
            if(!rmerged && merged)
            {
                break;
            }
            if(!sz)
            {
                break;
            }
            int cmerged = mergematc(m[sz-1], n);
            sz -= cmerged;
            if(!cmerged)
            {
                break;
            }
        }
        m[sz++] = n;
        return sz;
    }

    //m: pointer to array of materialsurfaces
    //sz: size of the array passed
    int mergemats(materialsurface *m, int sz)
    {
        std::sort(m, m+sz, mergematcmp);

        int nsz = 0;
        for(int i = 0; i < sz; ++i)
        {
            nsz = mergemat(m, nsz, m[i]);
        }
        return nsz;
    }

    bool optmatcmp(const materialsurface &x, const materialsurface &y)
    {
        if(x.material < y.material)
        {
            return true;
        }
        if(x.material > y.material)
        {
            return false;
        }
        if(x.orient > y.orient)
        {
            return true;
        }
        if(x.orient < y.orient)
        {
            return false;
        }
        int dim = DIMENSION(x.orient);
        return x.o[dim] < y.o[dim];
    }

    void preloadglassshaders(bool force = false)
    {
        static bool needglass = false;
        if(force)
        {
            needglass = true;
        }
        if(!needglass)
        {
            return;
        }
        useshaderbyname("glass");
    }

    void sorteditmaterials()
    {

        std::array<int, 3> sortdim {0, 1, 2};
        ivec sortorigin;

        //allows for sorting of materialsurface objects
        //intended to meet the standards of c++ stl `Compare`
        auto editmatcmp = [&sortdim, &sortorigin] (const materialsurface &x, const materialsurface &y)
        {
            int xdim = DIMENSION(x.orient),
                ydim = DIMENSION(y.orient);
            for(const int &i : sortdim)
            {
                int xmin, xmax, ymin, ymax;
                xmin = xmax = x.o[i];
                if(i==C[xdim])
                {
                    xmax += x.csize;
                }
                else if(i==R[i])
                {
                    xmax += x.rsize;
                }
                ymin = ymax = y.o[i];
                if(i==C[ydim])
                {
                    ymax += y.csize;
                }
                else if(i==R[ydim])
                {
                    ymax += y.rsize;
                }
                if(xmax > ymin && ymax > xmin)
                {
                    continue;
                }
                int c = sortorigin[i];
                if(c > xmin && c < xmax)
                {
                    return true;
                }
                if(c > ymin && c < ymax)
                {
                    return false;
                }
                xmin = std::abs(xmin - c);
                xmax = std::abs(xmax - c);
                ymin = std::abs(ymin - c);
                ymax = std::abs(ymax - c);
                if(std::max(xmin, xmax) < std::min(ymin, ymax))
                {
                    return true;
                }
                else if(std::max(ymin, ymax) <= std::min(xmin, xmax))
                {
                    return false;
                }
            }
            if(x.material != y.material)
            {
                return x.material < y.material;
            }
            return false;
        };

        sortorigin = ivec(camera1->o);
        vec dir = camdir().abs();
        if(dir[sortdim[2]] > dir[sortdim[1]])
        {
            std::swap(sortdim[2], sortdim[1]);
        }
        if(dir[sortdim[1]] > dir[sortdim[0]])
        {
            std::swap(sortdim[1], sortdim[0]);
        }
        if(dir[sortdim[2]] > dir[sortdim[1]])
        {
            std::swap(sortdim[2], sortdim[1]);
        }
        std::sort(editsurfs.begin(), editsurfs.end(), editmatcmp);
    }

    void rendermatgrid()
    {
        enablepolygonoffset(GL_POLYGON_OFFSET_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        int lastmat = -1;
        for(int i = static_cast<int>(editsurfs.size()); --i >=0;) //note reverse iteration
        {
            const materialsurface &m = editsurfs[i];
            if(m.material != lastmat)
            {
                xtraverts += gle::end();
                bvec color;
                switch(m.material&~MatFlag_Index)
                {   //colors of materials lines in edit mode
                    case Mat_Water:
                    {
                        color = bvec( 0,  0, 85);
                        break; // blue
                    }
                    case Mat_Clip:
                    {
                        color = bvec(85,  0,  0);
                        break; // red
                    }
                    case Mat_Glass:
                    {
                        color = bvec( 0, 85, 85);
                        break; // cyan
                    }
                    case Mat_NoClip:
                    {
                        color = bvec( 0, 85,  0);
                        break; // green
                    }
                    case Mat_GameClip:
                    {
                        color = bvec(85, 85,  0);
                        break; // yellow
                    }
                    case Mat_Death:
                    {
                        color = bvec(40, 40, 40);
                        break; // black
                    }
                    case Mat_Alpha:
                    {
                        color = bvec(85,  0, 85);
                        break; // pink
                    }
                    default:
                    {
                        continue;
                    }
                }
                gle::colorf(color.x*ldrscaleb(), color.y*ldrscaleb(), color.z*ldrscaleb());
                lastmat = m.material;
            }
            drawmaterial(m, -0.1f);
        }
        xtraverts += gle::end();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
    }

    void drawglass(const materialsurface &m, float offset, float glassxscale, float glassyscale, const vec normal = vec(0,0,0))
    {
        if(gle::attribbuf.empty())
        {
            gle::defvertex();
            if(normal != vec(0,0,0))
            {
                gle::defnormal();
            }
            gle::deftexcoord0();
        }
        float x = m.o.x,
              y = m.o.y,
              z = m.o.z,
              csize = m.csize,
              rsize = m.rsize;
        gle::begin(GL_TRIANGLE_FAN);
        if(normal != vec(0, 0, 0))
        {
            vec n = normal;
            switch (m.orient)
            {
                case 0:
                    {
                        vec v(x + offset, y + rsize, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x + offset, y + rsize, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x + offset, y, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x + offset, y, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    break;
                case 1:
                    {
                        vec v(x - offset, y + rsize, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x - offset, y, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x - offset, y, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x - offset, y + rsize, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    break;
                case 2:
                    {
                        vec v(x + csize, y + offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x, y + offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x, y + offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x + csize, y + offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    break;
                case 3:
                    {
                        vec v(x, y - offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x, y - offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x + csize, y - offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x + csize, y - offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    break;
                case 4:
                    {
                        vec v(x, y, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x, y + csize, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y + csize, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    break;
                case 5:
                    {
                        vec v(x, y, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y + csize, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x, y + csize, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(n.x, n.y, n.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    break;
                }
            }
        else
        {
            switch (m.orient)
            {
                case 0:
                    {
                        vec v(x + offset, y + rsize, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x + offset, y + rsize, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x + offset, y, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x + offset, y, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    break;
                case 1:
                    {
                        vec v(x - offset, y + rsize, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x - offset, y, z + csize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x - offset, y, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    {
                        vec v(x - offset, y + rsize, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.y, -glassyscale * v.z);
                    }
                    break;
                case 2:
                    {
                        vec v(x + csize, y + offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x, y + offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x, y + offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x + csize, y + offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    break;
                case 3:
                    {
                        vec v(x, y - offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x, y - offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x + csize, y - offset, z + rsize);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    {
                        vec v(x + csize, y - offset, z);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, -glassyscale * v.z);
                    }
                    break;
                case 4:
                    {
                        vec v(x, y, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x, y + csize, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y + csize, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y, z + offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    break;
                case 5:
                    {
                        vec v(x, y, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x + rsize, y + csize, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    {
                        vec v(x, y + csize, z - offset);
                        gle::attribf(v.x, v.y, v.z);
                        gle::attribf(glassxscale * v.x, glassyscale * v.y);
                    }
                    break;
            }
        }
        gle::end();
    }

    //these are the variables defined for each specific glass material (there are 4)
    #define GLASSVARS(name) \
        CVAR0R(name##color, 0xB0D8FF); \
        FVARR(name##refract, 0, 0.1f, 1e3f); \
        VARR(name##spec, 0, 150, 200);

    GLASSVARS(glass)
    GLASSVARS(glass2)
    GLASSVARS(glass3)
    GLASSVARS(glass4)

    #undef GLASSVARS

    GETMATIDXVAR(glass, color, const bvec &) //this is the getglasscolor() function
    GETMATIDXVAR(glass, refract, float)// this is the getglassrefract() function
    GETMATIDXVAR(glass, spec, int)// this is the getglassspec() function

    void renderglass()
    {
        for(int k = 0; k < 4; ++k)
        {
            const std::vector<materialsurface> &surfs = glasssurfs[k];
            if(surfs.empty())
            {
                continue;
            }

            MatSlot &gslot = lookupmaterialslot(Mat_Glass+k);

            Texture *tex = gslot.sts.size() ? gslot.sts[0].t : notexture;
            float glassxscale = defaulttexscale/(tex->xs*gslot.scale),
                  glassyscale = defaulttexscale/(tex->ys*gslot.scale);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, tex->id);
            glActiveTexture(GL_TEXTURE0);

            float refractscale = (0.5f/255)/ldrscale;
            const bvec &col = getglasscolor(k);
            float refract = getglassrefract(k);
            int spec = getglassspec(k);
            GLOBALPARAMF(glassrefract, col.x*refractscale, col.y*refractscale, col.z*refractscale, refract*viewh);
            GLOBALPARAMF(glassspec, spec/100.0f);

            for(const materialsurface &m: surfs)
            {
                drawglass(m, 0.1f, glassxscale, glassyscale, matnormals(m.orient));
            }
            xtraverts += gle::end();
        }
    }

    //sets the given material bounding vectors using the provided materialsurface
    //used by calcmatbb to set vertex array glass/water min/max fields
    void addmatbb(ivec &matmin, ivec &matmax, const materialsurface &m)
    {
        int dim = DIMENSION(m.orient);
        ivec mmin(m.o),
             mmax(m.o);
        if(DIM_COORD(m.orient))
        {
            mmin[dim] -= 2;
        }
        else
        {
            mmax[dim] += 2;
        }
        mmax[R[dim]] += m.rsize;
        mmax[C[dim]] += m.csize;
        matmin.min(mmin);
        matmax.max(mmax);
    }
}

// externally relevant functionality

vec matnormals(int i)
{
    const vec matnormals[6] =
    {
        vec(-1, 0, 0),
        vec( 1, 0, 0),
        vec(0, -1, 0),
        vec(0,  1, 0),
        vec(0, 0, -1),
        vec(0, 0,  1)
    };

    if(i < 0 || i > 6)
    {
        return vec(0,0,0);
    }
    else
    {
        return matnormals[i];
    }
}

/* findmaterial
 *
 * given a material name, returns the bitmask ID of the material as an integer
 */
int findmaterial(const char *name)
{
    for(int i = 0; i < static_cast<int>(sizeof(materials)/sizeof(material)); ++i)
    {
        if(!std::strcmp(materials[i].name, name))
        {
            return materials[i].id;
        }
    }
    return -1;
}

/* findmaterialname
 *
 * given a material id, returns the name of the material as a string (char *)
 */
const char *findmaterialname(int mat)
{
    for(int i = 0; i < static_cast<int>(sizeof(materials)/sizeof(materials[0])); ++i)
    {
        if(materials[i].id == mat)
        {
            return materials[i].name;
        }
    }
    return nullptr;
}

/* getmaterialdesc
 *
 * given a material id, returns the description of the material (char *)
 */
const char *getmaterialdesc(int mat, const char *prefix)
{
    static const ushort matmasks[] = { MatFlag_Volume|MatFlag_Index, MatFlag_Clip, Mat_Death, Mat_Alpha };
    static string desc;
    desc[0] = '\0';
    for(int i = 0; i < static_cast<int>(sizeof(matmasks)/sizeof(matmasks[0])); ++i)
    {
        if(mat&matmasks[i])
        {
            const char *matname = findmaterialname(mat&matmasks[i]);
            if(matname)
            {
                concatstring(desc, desc[0] ? ", " : prefix);
                concatstring(desc, matname);
            }
        }
    }
    return desc;
}

void genmatsurfs(const cube &c, const ivec &co, int size, std::vector<materialsurface> &matsurfs)
{
    static const std::array<ushort, 4> matmasks =
    {
        MatFlag_Volume|MatFlag_Index,
        MatFlag_Clip,
        Mat_Death,
        Mat_Alpha
    };
    for(int i = 0; i < 6; ++i)
    {
        for(const ushort &matmask : matmasks)
        {
            int vis = visiblematerial(c, i, co, size, matmask&~MatFlag_Index);
            if(vis != MatSurf_NotVisible)
            {
                materialsurface m;
                m.material = c.material&matmask;
                m.orient = i;
                m.visible = vis;
                m.o = co;
                m.csize = m.rsize = size;
                if(DIM_COORD(i))
                {
                    m.o[DIMENSION(i)] += size;
                }
                matsurfs.push_back(m);
                break;
            }
        }
    }
}

//sets the vertex array's material bounding box depending upon the material surfaces given
void calcmatbb(vtxarray *va, const ivec &co, int size, const std::vector<materialsurface> &matsurfs)
{
    va->watermax = va->glassmax = co;
    va->watermin = va->glassmin = ivec(co).add(size);
    for(const materialsurface &m : matsurfs)
    {
        switch(m.material&MatFlag_Volume)
        {
            case Mat_Water:
            {
                if(m.visible == MatSurf_EditOnly)
                {
                    continue;
                }
                addmatbb(va->watermin, va->watermax, m);
                break;
            }
            case Mat_Glass:
            {
                addmatbb(va->glassmin, va->glassmax, m);
                break;
            }
            default:
            {
                continue;
            }
        }
    }
}

int optimizematsurfs(materialsurface *matbuf, int matsurfs)
{
    std::sort(matbuf, matbuf+matsurfs, optmatcmp);
    const materialsurface *cur = matbuf,
                          *end = matbuf+matsurfs;
    while(cur < end)
    {
         const materialsurface *start = cur++;
         int dim = DIMENSION(start->orient);
         while(cur < end &&
               cur->material == start->material &&
               cur->orient == start->orient &&
               cur->visible == start->visible &&
               cur->o[dim] == start->o[dim])
        {
            ++cur;
        }
        if(!IS_LIQUID(start->material&MatFlag_Volume) || start->orient != Orient_Top || !vertwater)
        {
            if(start!=matbuf)
            {
                std::memmove(matbuf, start, (cur-start)*sizeof(materialsurface));
            }
            matbuf += mergemats(matbuf, cur-start);
        }
        else if(cur-start>=4)
        {
            QuadNode vmats(0, 0, rootworld.mapsize());
            for(int i = 0; i < cur-start; ++i)
            {
                vmats.insert(start[i].o[C[dim]], start[i].o[R[dim]], start[i].csize);
            }
            vmats.genmatsurfs(start->material, start->orient, start->visible, start->o[dim], matbuf);
        }
        else
        {
            if(start!=matbuf)
            {
                std::memmove(matbuf, start, (cur-start)*sizeof(materialsurface));
            }
            matbuf += cur-start;
        }
    }
    return matsurfs - (end-matbuf);
}

//treats `rootworld` as const
void setupmaterials(int start, int len)
{
    int hasmat = 0;
    if(!len)
    {
        len = valist.size();
    }
    for(int i = start; i < len; i++)
    {
        vtxarray *va = valist[i]; //only modifies va->matbuf entries of valist
        materialsurface *skip = nullptr;
        for(int j = 0; j < va -> matsurfs; ++j)
        {
            materialsurface &m = va->matbuf[j];
            int matvol = m.material&MatFlag_Volume;
            if(IS_LIQUID(matvol) && m.orient!=Orient_Bottom && m.orient!=Orient_Top)
            {
                m.ends = 0;
                int dim = DIMENSION(m.orient),
                    coord = DIM_COORD(m.orient);
                ivec o(m.o);
                o.z -= 1;
                o[dim] += coord ? 1 : -1;
                int minc = o[dim^1],
                    maxc = minc + (C[dim]==2 ? m.rsize : m.csize);
                ivec co;
                int csize;
                while(o[dim^1] < maxc)
                {
                    const cube &c = rootworld.lookupcube(o, 0, co, csize);
                    if(IS_LIQUID(c.material&MatFlag_Volume))
                    {
                        m.ends |= 1;
                        break;
                    }
                    o[dim^1] += csize;
                }
                o[dim^1] = minc;
                o.z += R[dim]==2 ? m.rsize : m.csize;
                o[dim] -= coord ? 2 : -2;
                while(o[dim^1] < maxc)
                {
                    const cube &c = rootworld.lookupcube(o, 0, co, csize);
                    if(visiblematerial(c, Orient_Top, co, csize))
                    {
                        m.ends |= 2;
                        break;
                    }
                    o[dim^1] += csize;
                }
            }
            else if(matvol==Mat_Glass)
            {
                int dim = DIMENSION(m.orient);
                vec center(m.o);
                center[R[dim]] += m.rsize/2;
                center[C[dim]] += m.csize/2;
            }
            if(matvol)
            {
                hasmat |= 1<<m.material;
            }
            m.skip = 0;
            if(skip && m.material == skip->material && m.orient == skip->orient && skip->skip < 0xFFFF)
            {
                skip->skip++;
            }
            else
            {
                skip = &m;
            }
        }
    }
    if(hasmat&(0xF<<Mat_Water))
    {
        loadcaustics(true);
        preloadwatershaders(true);
        for(int i = 0; i < 4; ++i)
        {
            if(hasmat&(1<<(Mat_Water+i)))
            {
                lookupmaterialslot(Mat_Water+i);
            }
        }
    }
    if(hasmat&(0xF<<Mat_Glass))
    {
        preloadglassshaders(true);
        for(int i = 0; i < 4; ++i)
        {
            if(hasmat&(1<<(Mat_Glass+i)))
            {
                lookupmaterialslot(Mat_Glass+i);
            }
        }
    }
}

VARP(showmat, 0, 1, 1); //toggles rendering material faces

GBuffer::MaterialInfo GBuffer::findmaterials() const
{
    MaterialInfo mi;
    editsurfs.clear();
    for(int i = 0; i < 4; ++i)
    {
        glasssurfs[i].clear();
        watersurfs[i].clear();
        waterfallsurfs[i].clear();
    }
    mi.matliquidsx1 = mi.matliquidsy1 = mi.matsolidsx1 = mi.matsolidsy1 = mi.matrefractsx1 = mi.matrefractsy1 = 1;
    mi.matliquidsx2 = mi.matliquidsy2 = mi.matsolidsx2 = mi.matsolidsy2 = mi.matrefractsx2 = mi.matrefractsy2 = -1;
    std::memset(mi.matliquidtiles, 0, sizeof(mi.matliquidtiles));
    std::memset(mi.matsolidtiles, 0, sizeof(mi.matsolidtiles));
    int hasmats = 0;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->matsurfs || va->occluded >= Occlude_BB || va->curvfc >= ViewFrustumCull_Fogged)
        {
            continue;
        }
        if(editmode && showmat && !drawtex)
        {
            for(int i = 0; i < va->matsurfs; ++i)
            {
                editsurfs.push_back(va->matbuf[i]);
            }
            continue;
        }
        float sx1, sy1, sx2, sy2;
        if(va->watermin.x <= va->watermax.x && calcbbscissor(va->watermin, va->watermax, sx1, sy1, sx2, sy2))
        {
            mi.matliquidsx1 = std::min(mi.matliquidsx1, sx1);
            mi.matliquidsy1 = std::min(mi.matliquidsy1, sy1);
            mi.matliquidsx2 = std::max(mi.matliquidsx2, sx2);
            mi.matliquidsy2 = std::max(mi.matliquidsy2, sy2);
            masktiles(mi.matliquidtiles, sx1, sy1, sx2, sy2);
            mi.matrefractsx1 = std::min(mi.matrefractsx1, sx1);
            mi.matrefractsy1 = std::min(mi.matrefractsy1, sy1);
            mi.matrefractsx2 = std::max(mi.matrefractsx2, sx2);
            mi.matrefractsy2 = std::max(mi.matrefractsy2, sy2);
            for(int i = 0; i < va->matsurfs; ++i)
            {
                materialsurface &m = va->matbuf[i];
                //skip if only rendering edit mat boxes or non-water mat
                if((m.material&MatFlag_Volume) != Mat_Water || m.visible == MatSurf_EditOnly)
                {
                    i += m.skip;
                    continue;
                }
                hasmats |= 4|1;
                if(m.orient == Orient_Top)
                {
                    for(int i = 0; i < 1 + m.skip; ++i)
                    {
                        watersurfs[m.material&MatFlag_Index].push_back((&m)[i]);
                    }
                }
                else
                {
                    for(int i = 0; i < 1 + m.skip; ++i)
                    {
                        waterfallsurfs[m.material&MatFlag_Index].push_back((&m)[i]);
                    }
                }
                i += m.skip;
            }
        }
        if(va->glassmin.x <= va->glassmax.x && calcbbscissor(va->glassmin, va->glassmax, sx1, sy1, sx2, sy2))
        {
            mi.matsolidsx1 = std::min(mi.matsolidsx1, sx1);
            mi.matsolidsy1 = std::min(mi.matsolidsy1, sy1);
            mi.matsolidsx2 = std::max(mi.matsolidsx2, sx2);
            mi.matsolidsy2 = std::max(mi.matsolidsy2, sy2);
            masktiles(mi.matsolidtiles, sx1, sy1, sx2, sy2);
            mi.matrefractsx1 = std::min(mi.matrefractsx1, sx1);
            mi.matrefractsy1 = std::min(mi.matrefractsy1, sy1);
            mi.matrefractsx2 = std::max(mi.matrefractsx2, sx2);
            mi.matrefractsy2 = std::max(mi.matrefractsy2, sy2);
            for(int i = 0; i < va->matsurfs; ++i)
            {
                materialsurface &m = va->matbuf[i];
                if((m.material&MatFlag_Volume) != Mat_Glass)
                {
                    i += m.skip;
                    continue;
                }
                hasmats |= 4|2;
                for(int i = 0; i < 1 + m.skip; ++i)
                {
                    glasssurfs[m.material&MatFlag_Index].push_back((&m)[i]);
                }
                i += m.skip;
            }
        }
    }
    mi.hasmats = hasmats;
    return mi;
}

void GBuffer::rendermaterialmask() const
{
    glDisable(GL_CULL_FACE);
    for(const std::vector<materialsurface> &k : glasssurfs)
    {
        for(const materialsurface &i : k)
        {
            drawmaterial(i, 0.1f);
        }
    }
    for(const std::vector<materialsurface> &k : watersurfs)
    {
        for(const materialsurface &i : k)
        {
            drawmaterial(i, wateroffset);
        }
    }
    for(const std::vector<materialsurface> &k : waterfallsurfs)
    {
        for(const materialsurface &i : k)
        {
            drawmaterial(i, 0.1f);
        }
    }
    xtraverts += gle::end();
    glEnable(GL_CULL_FACE);
}

void GBuffer::renderliquidmaterials() const
{
    glDisable(GL_CULL_FACE);

    renderwater();
    renderwaterfalls();

    glEnable(GL_CULL_FACE);
}

void rendersolidmaterials()
{
    glDisable(GL_CULL_FACE);

    renderglass();

    glEnable(GL_CULL_FACE);
}

void rendereditmaterials()
{
    if(editsurfs.empty())
    {
        return;
    }
    sorteditmaterials();

    glDisable(GL_CULL_FACE);

    zerofogcolor();

    foggednotextureshader->set();

    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glEnable(GL_BLEND);

    int lastmat = -1;
    for(const materialsurface &m : editsurfs)
    {
        if(lastmat!=m.material)
        {
            xtraverts += gle::end();
            bvec color;
            switch(m.material&~MatFlag_Index)
            {
                //note inverted colors
                case Mat_Water:
                {
                    color = bvec(255, 128,   0);
                    break; // blue
                }
                case Mat_Clip:
                {
                    color = bvec(  0, 255, 255);
                    break; // red
                }
                case Mat_Glass:
                {
                    color = bvec(255,   0,   0);
                    break; // cyan
                }
                case Mat_NoClip:
                {
                    color = bvec(255,   0, 255);
                    break; // green
                }
                case Mat_GameClip:
                {
                    color = bvec(  0,   0, 255);
                    break; // yellow
                }
                case Mat_Death:
                {
                    color = bvec(192, 192, 192);
                    break; // black
                }
                case Mat_Alpha:
                {
                    color = bvec(  0, 255,   0);
                    break; // pink
                }
                default:
                {
                    continue;
                }
            }
            gle::color(color);
            lastmat = m.material;
        }
        drawmaterial(m, -0.1f);
    }

    xtraverts += gle::end();
    glDisable(GL_BLEND);
    resetfogcolor();
    rendermatgrid();
    glEnable(GL_CULL_FACE);
}

void renderminimapmaterials()
{
    glDisable(GL_CULL_FACE);
    renderwater();
    glEnable(GL_CULL_FACE);
}

