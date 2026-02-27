#ifndef LIGHT_H_
#define LIGHT_H_

struct vertinfo;

struct surfaceinfo;

extern bvec ambient, skylight, sunlight;
extern float ambientscale, skylightscale, sunlightscale;
extern float sunlightyaw, sunlightpitch;
extern vec sunlightdir;

extern void clearlights();
extern void initlights();
extern void clearlightcache(int id = -1);

/**
 * @brief Fills allof the cube's surfaces with empty surfaceinfo objects.
 *
 * Creates a cubeext (where the surfaces are stored) for the cube if no cubeext
 * exists.
 *
 * @param c the cube to modify
 */
extern void brightencube(cube &c);
extern void setsurface(cube &c, int orient, const surfaceinfo &surf, const vertinfo *verts, int numverts);

class PackNode final
{
    public:
        PackNode(ushort x, ushort y, ushort w, ushort h) :  w(w), h(h), child1(0), child2(0), x(x), y(y), available(std::min(w, h)) {}

        void reset();

        bool resize(int nw, int nh);

        ~PackNode();

        bool insert(ushort &tx, ushort &ty, ushort tw, ushort th);
        void reserve(ushort tx, ushort ty, ushort tw, ushort th);

        int availablespace() const;
        vec2 dimensions() const;

        //debugging printouts, not used in program logic

        //i: recursion depth
        void printchildren(int i = 0) const;

        //i: depth to print out
        void print(int i) const;

    private:
        ushort w, h;
        PackNode *child1, *child2;
        ushort x, y;
        int available;

        /**
         * @brief Non-recursively discards children.
         *
         * Frees the heap allocated child1 and child2 pointers. If those packnodes
         * point to other children, that memory will be leaked.
         */
        void discardchildren()
        {
            if(child1)
            {
                delete child1;
                child1 = nullptr;
            }
            if(child2)
            {
                delete child2;
                child2 = nullptr;
            }
        }

        void forceempty()
        {
            discardchildren();
            available = 0;
        }
};

extern PackNode shadowatlaspacker;

#endif
