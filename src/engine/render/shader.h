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

extern hashnameset<GlobalShaderParamState> globalparams;

#endif
