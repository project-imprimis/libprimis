#ifndef IMAGEDATA_H_
#define IMAGEDATA_H_

class ImageData
{
    public:
        ImageData();
        ImageData(int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE);
        ~ImageData();

        int w, // image's width
            h, // image's height
            bpp, //bits per image pixel
            levels,
            align,
            pitch;
        GLenum compressed; //type of GL compression
        uchar *data; //the raw array of pixel data

        int calclevelsize(int level) const;

        void addglow(const ImageData &g, const vec &glowcolor);
        void mergespec(const ImageData &s);
        void mergedepth(const ImageData &z);
        void mergealpha(const ImageData &s);
        void collapsespec();
        void forcergbimage();
        void swizzleimage();
        void scaleimage(int w, int h);
        void texmad(const vec &mul, const vec &add);
        void texpremul();

        bool texturedata(const char *tname, bool msg = true, int * const compress = nullptr, int * const wrap = nullptr, const char *tdir = nullptr, int ttype = 0);
        bool texturedata(Slot &slot, Slot::Tex &tex, bool msg = true, int *compress = nullptr, int *wrap = nullptr);

    private:
        void *owner; //the owner of the pixel data, generally an SDL_Surface
        void (*freefunc)(void *); //the function that is called to free the surface associated with the object, SDL_FreeSurface()

        void setdata(uchar *ndata, int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE);
        int calcsize() const;
        void disown();
        void cleanup();
        void replace(ImageData &d);
        void wraptex(SDL_Surface *s); //wraps a SDL_Surface's data in this object

        void reorientnormals(uchar * RESTRICT src, int sw, int sh, int bpp, int stride, uchar * RESTRICT dst, bool flipx, bool flipy, bool swapxy);

        void texreorient(bool flipx, bool flipy, bool swapxy, int type = 0);
        void texrotate(int numrots, int type = 0);
        void texoffset(int xoffset, int yoffset);
        void texcrop(int x, int y, int w, int h);
        void texcolorify(const vec &color, vec weights);
        void texcolormask(const vec &color1, const vec &color2);
        void texdup(int srcchan, int dstchan);
        void texmix(int c1, int c2, int c3, int c4);
        void texgrey();
        void texagrad(float x2, float y2, float x1, float y1);
        void texblend(const ImageData &s0, const ImageData &m0);
        void texnormal(int emphasis);
};

#endif

