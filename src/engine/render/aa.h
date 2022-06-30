#ifndef AA_H_
#define AA_H_

class GBuffer;

extern matrix4 nojittermatrix;

extern void setupaa(int w, int h);
extern void jitteraa();
extern bool multisampledaa();

namespace aamask
{
    void set(bool val);
    void enable(int stencil = 0);
    void disable();
}

extern void doaa(GLuint outfbo, GBuffer gbuffer);
extern bool debugaa();
extern void cleanupaa();

enum
{
    AA_Unused = 0,
    AA_Luma,
    AA_Masked,
    AA_Split,
    AA_SplitLuma,
    AA_SplitMasked,
};

#endif
