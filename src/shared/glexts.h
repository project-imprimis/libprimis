/**
 * @file glexts.h
 * @brief
 */

#ifndef GLEXTS_H_
#define GLEXTS_H_

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_EXT_timer_query
#define GL_EXT_timer_query 1
#define GL_TIME_ELAPSED_EXT               0x88BF
typedef llong GLint64EXT;
typedef ullong GLuint64EXT;
typedef void (APIENTRYP PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64EXT *params);
#endif

// GL_EXT_framebuffer_multisample
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample_;

extern PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample_;

#ifndef GL_EXT_framebuffer_multisample_blit_scaled
#define GL_EXT_framebuffer_multisample_blit_scaled 1
#define GL_SCALED_RESOLVE_FASTEST_EXT     0x90BA
#endif

#ifndef GL_ARB_texture_rg
#define GL_ARB_texture_rg 1
#define GL_RG                             0x8227
#define GL_RG_INTEGER                     0x8228
#define GL_R8                             0x8229
#define GL_R16                            0x822A
#define GL_RG8                            0x822B
#define GL_RG16                           0x822C
#define GL_R16F                           0x822D
#define GL_R32F                           0x822E
#define GL_RG16F                          0x822F
#define GL_RG32F                          0x8230
#define GL_R8I                            0x8231
#define GL_R8UI                           0x8232
#define GL_R16I                           0x8233
#define GL_R16UI                          0x8234
#define GL_R32I                           0x8235
#define GL_R32UI                          0x8236
#define GL_RG8I                           0x8237
#define GL_RG8UI                          0x8238
#define GL_RG16I                          0x8239
#define GL_RG16UI                         0x823A
#define GL_RG32I                          0x823B
#define GL_RG32UI                         0x823C
#endif

#ifndef GL_EXT_texture_compression_latc
#define GL_EXT_texture_compression_latc 1
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT              0x8C70
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT        0x8C72
#endif

#ifndef GL_ARB_texture_compression_rgtc
#define GL_ARB_texture_compression_rgtc 1
#define GL_COMPRESSED_RED_RGTC1           0x8DBB
#define GL_COMPRESSED_RG_RGTC2            0x8DBD
#endif

#ifndef GL_EXT_depth_bounds_test
#define GL_EXT_depth_bounds_test 1
#define GL_DEPTH_BOUNDS_TEST_EXT          0x8890
#define GL_DEPTH_BOUNDS_EXT               0x8891
typedef void (APIENTRYP PFNGLDEPTHBOUNDSEXTPROC) (GLclampd zmin, GLclampd zmax);
#endif
extern PFNGLDEPTHBOUNDSEXTPROC glDepthBounds_;

#ifndef GL_ARB_map_buffer_range
#define GL_ARB_map_buffer_range 1
#define GL_MAP_READ_BIT                   0x0001
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT       0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT         0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT         0x0020
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
#endif

#ifndef GL_INVALID_INDEX
#define GL_INVALID_INDEX                  0xFFFFFFFFu
#endif

#ifndef GL_EXT_texture_integer
#define GL_EXT_texture_integer 1
typedef void (APIENTRYP PFNGLCLEARCOLORIIEXTPROC) (GLint red, GLint green, GLint blue, GLint alpha);
typedef void (APIENTRYP PFNGLCLEARCOLORIUIEXTPROC) (GLuint red, GLuint green, GLuint blue, GLuint alpha);
#endif

#ifndef GL_ARB_half_float_vertex
#define GL_ARB_half_float_vertex 1
#define GL_HALF_FLOAT                     0x140B
#endif

#ifndef GL_ARB_texture_swizzle
#define GL_ARB_texture_swizzle 1
#define GL_TEXTURE_SWIZZLE_R              0x8E42
#define GL_TEXTURE_SWIZZLE_G              0x8E43
#define GL_TEXTURE_SWIZZLE_B              0x8E44
#define GL_TEXTURE_SWIZZLE_A              0x8E45
#define GL_TEXTURE_SWIZZLE_RGBA           0x8E46
#endif

#ifndef GL_ARB_occlusion_query2
#define GL_ARB_occlusion_query2 1
#define GL_ANY_SAMPLES_PASSED             0x8C2F
#endif

#ifndef GL_ARB_blend_func_extended
#define GL_ARB_blend_func_extended 1
#define GL_SRC1_COLOR                     0x88F9
#define GL_ONE_MINUS_SRC1_COLOR           0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA           0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS   0x88FC
typedef void (APIENTRYP PFNGLBINDFRAGDATALOCATIONINDEXEDPROC) (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);
#endif

#ifndef GL_VERSION_4_3
typedef void (APIENTRYP PFNGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
#endif

// GL_ARB_copy_image
extern PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData_;

#endif /* GLEXTS_H_ */
