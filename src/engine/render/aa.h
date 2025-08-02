#ifndef AA_H_
#define AA_H_

class GBuffer;

extern matrix4 nojittermatrix;

extern void setupaa(GBuffer &buf, int w, int h);
extern void jitteraa();

namespace aamask
{
    void set(bool val);
    void enable(int stencil = 0);
    void disable();
}

/**
 * @brief executes one type of screenspace aa
 *
 * only one screenspace aa can be used at a time, and smaa will always be used
 * instead of fxaa or tqaa; fxaa will always be used instead of tqaa
 *
 * does not apply to multisample aa, msaa is not a screenspace aa
 *
 * method pointer resolve is used to setup the fbo for the specified aa
 *
 * @param outfbo the framebuffer object to apply aa to
 * @param gbuffer the gbuffer to apply hdr with
 */
extern void doaa(GLuint outfbo, GBuffer &gbuffer);
extern bool debugaa();
extern void cleanupaa();

enum AAFlag
{
    AA_Unused = 0,
    AA_Luma,
    AA_Masked,
    AA_Split,
    AA_SplitLuma,
    AA_SplitMasked,
};

#endif
