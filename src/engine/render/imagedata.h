#ifndef IMAGEDATA_H_
#define IMAGEDATA_H_

class ImageData
{
    public:
        ImageData();
        ImageData(int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE);
        ~ImageData();

        int w, h, bpp, levels, align, pitch;
        GLenum compressed;
        uchar *data;

        int calclevelsize(int level) const;

        void addglow(ImageData &g, const vec &glowcolor);
        void mergespec(ImageData &s);
        void mergedepth(ImageData &z);
        void mergealpha(ImageData &s);
        void collapsespec();
        void forcergbimage();
        void swizzleimage();
        void scaleimage(int w, int h);
        void texmad(const vec &mul, const vec &add);
        void texpremul();

        bool texturedata(const char *tname, bool msg = true, int *compress = nullptr, int *wrap = nullptr, const char *tdir = nullptr, int ttype = 0);
        bool texturedata(Slot &slot, Slot::Tex &tex, bool msg = true, int *compress = nullptr, int *wrap = nullptr);

    private:
        void *owner;
        void (*freefunc)(void *);

        void setdata(uchar *ndata, int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE);
        int calcsize() const;
        void disown();
        void cleanup();
        void replace(ImageData &d);
        void wraptex(SDL_Surface *s);

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
        void texblend(ImageData &s, ImageData &m);
        void texnormal(int emphasis);
};

#endif

