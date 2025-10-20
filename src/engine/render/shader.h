#ifndef SHADER_H_
#define SHADER_H_

enum
{
    Shader_Default    = 0,
    Shader_World      = 1 << 0,
    Shader_Refract    = 1 << 2,
    Shader_Option     = 1 << 3,
    Shader_Dynamic    = 1 << 4,
    Shader_Triplanar  = 1 << 5,

    Shader_Invalid    = 1 << 8,
    Shader_Deferred   = 1 << 9
};

extern float blursigma;
extern Shader *nullshader, *hudshader, *hudnotextureshader, *nocolorshader, *foggednotextureshader, *ldrnotextureshader;
constexpr int maxblurradius = 7;

/**
 * @brief Gets or creates a local shader param.
 *
 * Tries to get the index for the passed localparam in localparams. If there is
 * no matching member, creates one with the passed name at the end of localparams.
 *
 * @param name the name to look for
 *
 * @return the index of the local param
 */
extern size_t getlocalparam(const std::string &name);

/**
 * @brief Sets up blur weights passed by parameter.
 *
 * @param radius sets number of weights & offsets for blurring to be made
 * @param weights array of length at least radius + 1
 * @param offsets array of length at least radius + 1
 */
extern void setupblurkernel(int radius, float *weights, float *offsets);
extern void setblurshader(int pass, int size, int radius, const float *weights, const float *offsets, GLenum target = GL_TEXTURE_2D);

extern Shader *lookupshaderbyname(std::string_view name);

/** @brief Get a shader by name string.
 *
 * The shader is searched for in the `shaders` global map.
 * If the shader is deferred, force it to be used
 *
 * @param name a string refering to the name of the shader to use
 *
 * @return pointer to a Shader object corresponding to the name passed
 */
extern Shader *useshaderbyname(std::string_view name);
extern Shader *generateshader(std::string_view name, const char *cmd, ...);

/**
 * @brief Clears the slot shader and its parameters.
 *
 * Sets the slotshader to nullptr. Does not free it.
 * Clears all associated params with that slotshader.
 */
extern void resetslotshader();
extern void setslotshader(Slot &s);
extern void linkslotshader(Slot &s, bool load = true);
extern void linkvslotshader(VSlot &s, bool load = true);
extern void linkslotshaders();
extern bool shouldreuseparams(const Slot &s, const VSlot &p);
extern void setupshaders();
extern void reloadshaders();
extern void cleanupshaders();

#endif
