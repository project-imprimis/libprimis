#ifndef AA_H_
#define AA_H_

class GBuffer;

extern matrix4 nojittermatrix; /// matrix without aa jitter applied

/**
 * @brief Sets up relevant AA methods
 *
 * Only enables the AA mode that has been turned on. MSAA is not handled, only
 * screenspace AA methods.
 *
 * @param buf g-buffer with rendering information to use
 * @param w width of aa buffer in pixels
 * @param h height of aa buffer in pixels
 */
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

/**
 * @brief Shows smaa/tqaa debug information if possible
 *
 * Draws the debug information for smaa/tqaa if their debug vars are enabled.
 * If one or more of these were shown, returns true.
 *
 * @return true if any debug was shown
 */
extern bool debugaa();

/**
 * @brief Cleans up screenspace AA methods.
 *
 * For any one or more of smaa, tqaa, fxaa modes enabled, deletes their textures and disables
 * their operation.
 */
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
