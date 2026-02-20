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
extern void compactvslots(cube *c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern void reloadtextures();
extern void cleanuptextures();
extern bool settexture(const char *name, int clamp = 0);

//for imagedata manipulation
extern void scaletexture(const uchar * RESTRICT src, uint sw, uint sh, uint bpp, uint pitch, uchar * RESTRICT dst, uint dw, uint dh);
extern void reorienttexture(const uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy);
extern GLenum texformat(int bpp);

struct Slot;
struct VSlot;

// management of texture slots
// each texture slot can have multiple texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Texture final
{
    enum
    {
        IMAGE      = 0,
        TYPE       = 0xFF,

        STUB       = 1<<8,
        TRANSIENT  = 1<<9,
        ALPHA      = 1<<11,
        MIRROR     = 1<<12
    };

    const char *name;
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

    /**
     * @brief Returns the type of slot object that this is.
     *
     * This is an enum value in the enum in slot.h in the slot object.
     *
     * @return SlotType_Material enum value
     */
    int type() const final;

    /**
     * @brief Returns string indicating what material slot this is.
     *
     * The return string is one of the four tempformatstring array entries. The
     * string returned has the format "material string <name>"
     *
     * @return pointer to c string containing this slot's name
     */
    const char *name() const final;

    /**
     * @brief Filler implementation of emptyvslot.
     *
     * Does not return an empty vslot like for the VSlot object. Returns
     * reference to `this` object instead regardless of its status.
     *
     * @return reference to `this` object
     */
    VSlot &emptyvslot() final;

    int cancombine(int) const final;

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

struct TexRotation final
{
    bool flipx, flipy, swapxy;
};

extern const std::array<TexRotation, 8> texrotations;
extern Texture *notexture;
extern int maxvsuniforms;

extern SDL_Surface *loadsurface(const char *name);

extern MatSlot &lookupmaterialslot(int slot, bool load = true);

extern void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta);
extern void packvslot(std::vector<uchar> &buf, const VSlot &src);

extern Slot dummyslot;
extern DecalSlot dummydecalslot;

#endif
