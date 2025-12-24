#ifndef IMAGEDATA_H_
#define IMAGEDATA_H_

class ImageData final
{
    public:
        ImageData();
        ImageData(int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE);
        ~ImageData();

        int levels,
            align,
            pitch;
        GLenum compressed; //type of GL compression
        uchar *data; //the raw array of pixel data

        /**
         * @brief Returns the width of the image.
         *
         * @return width in pixels of the image
         */
        int width() const;

        /**
         * @brief Returns the height of the image.
         *
         * @return height in pixels of the image
         */
        int height() const;

        /**
         * @brief Returns the bit depth of the image.
         *
         * @return depth of each pixel
         */
        int depth() const;

        /**
         * @brief Returns size of nth level of this image,
         *
         * Returns size of the level specified, which is the align-corrected size of the texture
         * at the `level`th mipmap, multiplied by the bytes per pixel
         *
         * @param level the mipmap level to calculate
         */
        int calclevelsize(int level) const;

        void addglow(const ImageData &g, const vec &glowcolor);
        void mergespec(const ImageData &s);
        void mergedepth(const ImageData &z);
        void mergealpha(const ImageData &s);

        /**
         * @brief Replaces this ImageData with a new one with a single channel.
         *
         * Replaces this object with a single level replacement texture, collapsing down
         * from 4 channels by averaging if 4 exist.
         */
        void collapsespec();

        /**
         * @brief Replaces this ImageData with one widened to 3 channels (r,g,b)
         *
         * Has no effect and does not replace this ImageData if at least 3 channels
         * are already present.
         *
         * Replaces this ImageData with one containing three channels all identically
         * set to the value of the first channel of the existing texture. (This
         * means an RG texture will lose its green texture channel information,
         * for example). Intended for use on grayscale textures.
         */
        void forcergbimage();

        /**
         * @brief Creates a new image that increases the bit depth of the image.
         *
         * If there is two channels in the original image, replaces it with a four
         * channel image where the first channel is copied to the rgb channels, and
         * the second channel is copied to the a channel.
         *
         * If there is one channel in the original image, replaces it with a three
         * channel image where all three channels are copies of the original channel.
         */
        void swizzleimage();

        /**
         * Replaces this ImageData with a new one at the specified size
         *
         * @param w the width of the new image
         * @param h the height of the new image
         */
        void scaleimage(int w, int h);
        void texmad(const vec &mul, const vec &add);
        void texpremul();

        bool texturedata(const char *tname, bool msg = true, int * compress = nullptr, int * wrap = nullptr, const char *tdir = nullptr, int ttype = 0);
        bool texturedata(const Slot &slot, const Slot::Tex &tex, bool msg = true, int *compress = nullptr, int *wrap = nullptr);

    private:
        int bpp, /// bytes per image pixel
            w,   /// image's width in pixels
            h;   /// image's height in pixels

        void *owner; /// the owner of the pixel data, generally an SDL_Surface
        void (*freefunc)(void *); /// the function that is called to free the surface associated with the object, SDL_FreeSurface()

        void setdata(uchar *ndata, int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE);

        /**
         * @brief Returns size of the image
         *
         * If image has no alignment information, returns the number of bytes in the image
         * (height times width times bit depth).
         *
         * Otherwise, scales size down proportionaly to the value of align. Adds sum of
         * each power-of-two mipmap up to the total number of levels.
         *
         * @return number of bytes needed to store image
         */
        int calcsize() const;
        /**
         * @brief Clears ownership of this object's pointers.
         *
         * Clears the objects pointers that the image data points to, to allow them to be exclusively
         * pointed to by another object. This is used when calling replace() such that the old object
         * does not have references to the new object's data fields.
         */
        void disown();

        /**
         * @brief Cleans up this object heap allocated data.
         *
         * Notifies parent function pointed to by this object's freefunc, if it has been
         * set. This is expected to clean up data pointed to by the data object.
         *
         * If this object has no parent, the object can free its own data array. The
         * pointer to this array, data, will be nullified.
         *
         * Sets data, owner, and freefunc fn ptr to nullptr.
         */
        void cleanup();

        /**
         * @brief Replaces this object with another.
         *
         * Deletes the data associated with the current ImageData object
         * and makes the object point to the one passed by parameter.
         *
         * Transfers heap allocated resources to this object and leaves the original
         * object with NULL handles.
         *
         * @param d the imagedata to replace this object with.
         */
        void replace(ImageData &d);

        /**
         * @brief Tells this object to wrap the specified SDL surface
         *
         * Takes ownership of this SDL surface. The surface's free function is bound
         * to this imagedata so its lifetime will be cleaned up by the paret ImageData.
         *
         * @param s the surface to wrap
         */
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

        static bool matchstring(std::string_view s, size_t len, std::string_view d);
};

#endif

