#ifndef TEXTURE_H_
#define TEXTURE_H_

extern int hwtexsize, hwcubetexsize, hwmaxaniso, maxtexsize, hwtexunits, hwvtexunits;

extern Texture *textureload(const char *name, int clamp = 0, bool mipit = true, bool msg = true);
extern bool floatformat(GLenum format);
extern void loadshaders();
extern void createtexture(int tnum, int w, int h, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D, int pw = 0, int ph = 0, int pitch = 0, bool resize = true, GLenum format = GL_FALSE, bool swizzle = false);
extern void create3dtexture(int tnum, int w, int h, int d, const void *pixels, int clamp, int filter, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_3D, bool swizzle = false);
extern bool reloadtexture(const char *name);
extern void clearslots();
extern void compacteditvslots();
extern void compactvslots(cube * const c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern void reloadtextures();
extern void cleanuptextures();
extern bool settexture(const char *name, int clamp = 0);

//for imagedata manipulation
extern void scaletexture(const uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh);
extern void reorienttexture(const uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy);
extern GLenum texformat(int bpp, bool swizzle = false);

struct Slot;
struct VSlot;

// management of texture slots
// each texture slot can have multiple texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Texture
{
    enum
    {
        IMAGE      = 0,
        CUBEMAP    = 1,
        TYPE       = 0xFF,

        STUB       = 1<<8,
        TRANSIENT  = 1<<9,
        COMPRESSED = 1<<10,
        ALPHA      = 1<<11,
        MIRROR     = 1<<12,
        FLAGS      = 0xFF00
    };

    char *name;
    int type, w, h, xs, ys, bpp, clamp;
    bool mipmap, canreduce;
    GLuint id;
    uchar *alphamask;

    Texture() : alphamask(nullptr) {}
    const uchar * loadalphamask();
    float ratio() const;
    bool reload(); //if the texture does not have a valid GL texture, attempts to reload it

};

enum TextureLayers
{
    Tex_Diffuse = 0,
    Tex_Normal,
    Tex_Glow,

    Tex_Spec,
    Tex_Depth,
    Tex_Alpha,
    Tex_Unknown,
};

struct MatSlot final : Slot, VSlot
{
    MatSlot();

    int type() const override final
    {
        return SlotType_Material;
    }
    const char *name() const override final;

    VSlot &emptyvslot() override final
    {
        return *this;
    }

    int cancombine(int) const override final
    {
        return -1;
    }

    void reset()
    {
        Slot::reset();
        VSlot::reset();
    }

    void cleanup()
    {
        Slot::cleanup();
        VSlot::cleanup();
    }
};

struct texrotation
{
    bool flipx, flipy, swapxy;
};

extern const texrotation texrotations[8];
extern Texture *notexture;
extern int maxvsuniforms, maxfsuniforms;

extern SDL_Surface *loadsurface(const char *name);

extern MatSlot &lookupmaterialslot(int slot, bool load = true);

extern void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta);
extern void packvslot(std::vector<uchar> &buf, const VSlot &src);

extern Slot dummyslot;
extern VSlot dummyvslot;
extern DecalSlot dummydecalslot;

#endif
