#ifndef AA_H_
#define AA_H_
extern matrix4 nojittermatrix;

extern void setupaa(int w, int h);
extern void jitteraa();
extern bool multisampledaa();
extern void setaavelocityparams(GLenum tmu = GL_TEXTURE0);
extern void setaamask(bool val);
extern void enableaamask(int stencil = 0);
extern void disableaamask();
extern void doaa(GLuint outfbo, void (*resolve)(GLuint, int));
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
