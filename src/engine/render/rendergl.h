#ifndef RENDERGL_H_
#define RENDERGL_H_

extern int xtraverts, xtravertsva;
extern int renderw, renderh;
extern vec worldpos, camdir, camright, camup;
extern bool hasFBMSBS, hasTQ, hasDBT, hasES3, hasCI;
extern int glversion, glslversion, glcompat;
extern int mesa_swap_bug;
extern int maxdrawbufs, maxdualdrawbufs;
extern vec minimapcenter, minimapradius, minimapscale;
extern physent *camera1;                // special ent that acts as camera, same object as player1 in FPS mode

enum
{
    Draw_TexNone = 0, //unused
    Draw_TexMinimap,
    Draw_TexModelPreview,
};

extern int fov;
extern float curfov, fovy, aspect, forceaspect;
extern float nearplane;
extern int farplane;
extern float ldrscale, ldrscaleb;
extern int drawtex;
extern const matrix4 viewmatrix, invviewmatrix;

inline const matrix4 viewmatrix(vec(-1, 0, 0), vec(0, 0, 1), vec(0, -1, 0));
inline const matrix4 invviewmatrix(vec(-1, 0, 0), vec(0, 0, -1), vec(0, 1, 0));

extern matrix4 cammatrix, projmatrix, camprojmatrix, invcammatrix, invcamprojmatrix, invprojmatrix;
extern int fog;
extern bvec fogcolor;
extern vec curfogcolor;
extern int wireframe;
extern int usetexgather;

extern int glerr;
extern int intel_texalpha_bug;
extern void glerror(const char *file, int line, GLenum error);

inline void glerror()
{
    if(glerr)
    {
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
        {
            glerror(__FILE__, __LINE__, error);
        }
    }
}
extern void mousemove(int dx, int dy);
extern void gl_checkextensions();
extern void gl_init();
extern void gl_resize();
extern void gl_setupframe(bool force = false);
extern void gl_drawframe();
extern void cleanupgl();
extern void enablepolygonoffset(GLenum type);
extern void disablepolygonoffset(GLenum type);
extern bool calcspherescissor(const vec &center, float size, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2);
extern bool calcbbscissor(const ivec &bbmin, const ivec &bbmax, float &sx1, float &sy1, float &sx2, float &sy2);
extern bool calcspotscissor(const vec &origin, float radius, const vec &dir, int spot, const vec &spotx, const vec &spoty, float &sx1, float &sy1, float &sx2, float &sy2, float &sz1, float &sz2);
extern void screenquad();
extern void screenquad(float sw, float sh);
extern void screenquad(float sw, float sh, float sw2, float sh2);
extern void screenquadoffset(float x, float y, float w, float h);
extern void hudquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1);
extern void debugquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1);
extern float calcfrustumboundsphere(float nearplane, float farplane,  const vec &pos, const vec &view, vec &center);
extern void zerofogcolor();
extern void resetfogcolor();
extern float calcfogdensity(float dist);
extern float calcfogcull();
extern vec calcmodelpreviewpos(const vec &radius, float &yaw);

extern matrix4 hudmatrix;
extern void resethudmatrix();
extern void pushhudmatrix();
extern void flushhudmatrix(bool flushparams = true);
extern void pophudmatrix(bool flush = true, bool flushparams = true);
extern void pushhudscale(float sx, float sy = 0);
extern void pushhudtranslate(float tx, float ty, float sx = 0, float sy = 0);

namespace modelpreview
{
    extern void start(int x, int y, int w, int h, bool background = true, bool scissor = false);
    extern void end();
}

extern void masktiles(uint *tiles, float sx1, float sy1, float sx2, float sy2);
#endif
