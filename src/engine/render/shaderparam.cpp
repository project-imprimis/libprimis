// texture.cpp: texture slot management

#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "imagedata.h"
#include "octarender.h"
#include "renderwindow.h"
#include "shaderparam.h"
#include "texture.h"

#include "world/light.h"
#include "world/material.h"
#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/world.h"

#include "interface/console.h"
#include "interface/control.h"

//globalshaderparam

GlobalShaderParam::GlobalShaderParam(const char *name) : name(name), param(nullptr) {}

GlobalShaderParamState *GlobalShaderParam::getglobalparam(const char *name) const
{
    GlobalShaderParamState *param = globalparams.access(name);
    if(!param)
    {
        param = &globalparams[name];
        param->name = name;
        std::memset(param->buf, -1, sizeof(param->buf));
        param->version = -1;
    }
    return param;
}

GlobalShaderParamState *GlobalShaderParam::resolve()
{
    if(!param)
    {
        param = getglobalparam(name);
    }
    param->changed();
    return param;
}

void GlobalShaderParam::setf(float x, float y, float z, float w)
{
    GlobalShaderParamState *g = resolve();
    g->fval[0] = x;
    g->fval[1] = y;
    g->fval[2] = z;
    g->fval[3] = w;
}

void GlobalShaderParam::set(const vec &v, float w)
{
    setf(v.x, v.y, v.z, w);
}

void GlobalShaderParam::set(const vec2 &v, float z, float w)
{
    setf(v.x, v.y, z, w);
}

void GlobalShaderParam::set(const vec4<float> &v)
{
    setf(v.x, v.y, v.z, v.w);
}

void GlobalShaderParam::set(const plane &p)
{
    setf(p.x, p.y, p.z, p.offset);
}

void GlobalShaderParam::set(const matrix2 &m)
{
    std::memcpy(resolve()->fval, m.a.v, sizeof(m));
}

void GlobalShaderParam::set(const matrix3 &m)
{
    std::memcpy(resolve()->fval, m.a.v, sizeof(m));
}

void GlobalShaderParam::set(const matrix4 &m)
{
    std::memcpy(resolve()->fval, m.a.v, sizeof(m));
}

void GlobalShaderParam::seti(int x, int y, int z, int w)
{
    GlobalShaderParamState *g = resolve();
    g->ival[0] = x;
    g->ival[1] = y;
    g->ival[2] = z;
    g->ival[3] = w;
}
void GlobalShaderParam::set(const ivec &v, int w)
{
    seti(v.x, v.y, v.z, w);
}

void GlobalShaderParam::set(const ivec2 &v, int z, int w)
{
    seti(v.x, v.y, z, w);
}

void GlobalShaderParam::set(const vec4<int> &v)
{
    seti(v.x, v.y, v.z, v.w);
}

void GlobalShaderParam::setu(uint x, uint y, uint z, uint w)
{
    GlobalShaderParamState *g = resolve();
    g->uval[0] = x;
    g->uval[1] = y;
    g->uval[2] = z;
    g->uval[3] = w;
}

//localshaderparam

LocalShaderParam::LocalShaderParam(const char *name) : name(name), loc(-1) {}

LocalShaderParamState *LocalShaderParam::resolve()
{
    Shader *s = Shader::lastshader;
    if(!s)
    {
        return nullptr;
    }
    if(!(s->localparamremap.size() > static_cast<uint>(loc)))
    {
        if(loc == -1)
        {
            loc = getlocalparam(name);
        }
        if(!(s->localparamremap.size() > static_cast<uint>(loc)))
        {
            return nullptr;
        }
    }
    uchar remap = s->localparamremap[loc];
    return (s->localparams.size() > remap) ? &s->localparams[remap] : nullptr;
}

void LocalShaderParam::setf(float x, float y, float z, float w)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        switch(b->format)
        {
            case GL_BOOL:
            case GL_FLOAT:
            {
                glUniform1f(b->loc, x);
                break;
            }
            case GL_BOOL_VEC2:
            case GL_FLOAT_VEC2:
            {
                glUniform2f(b->loc, x, y);
                break;
            }
            case GL_BOOL_VEC3:
            case GL_FLOAT_VEC3:
            {
                glUniform3f(b->loc, x, y, z);
                break;
            }
            case GL_BOOL_VEC4:
            case GL_FLOAT_VEC4:
            {
                glUniform4f(b->loc, x, y, z, w);
                break;
            }
            case GL_INT:
            {
                glUniform1i(b->loc, static_cast<int>(x));
                break;
            }
            case GL_INT_VEC2:
            {
                glUniform2i(b->loc, static_cast<int>(x), static_cast<int>(y));
                break;
            }
            case GL_INT_VEC3:
            {
                glUniform3i(b->loc, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
                break;
            }
            case GL_INT_VEC4:
            {
                glUniform4i(b->loc, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), static_cast<int>(w));
                break;
            }
            case GL_UNSIGNED_INT:
            {
                glUniform1ui(b->loc, static_cast<uint>(x));
                break;
            }
            case GL_UNSIGNED_INT_VEC2:
            {
                glUniform2ui(b->loc, static_cast<uint>(x), static_cast<uint>(y));
                break;
            }
            case GL_UNSIGNED_INT_VEC3:
            {
                glUniform3ui(b->loc, static_cast<uint>(x), static_cast<uint>(y), static_cast<uint>(z));
                break;
            }
            case GL_UNSIGNED_INT_VEC4:
            {
                glUniform4ui(b->loc, static_cast<uint>(x), static_cast<uint>(y), static_cast<uint>(z), static_cast<uint>(w));
                break;
            }
        }
    }
}

void LocalShaderParam::set(const vec &v, float w)
{
    setf(v.x, v.y, v.z, w);
}

void LocalShaderParam::set(const vec2 &v, float z, float w)
{
    setf(v.x, v.y, z, w);
}

void LocalShaderParam::set(const vec4<float> &v)
{
    setf(v.x, v.y, v.z, v.w);
}

void LocalShaderParam::set(const plane &p)
{
    setf(p.x, p.y, p.z, p.offset);
}

void LocalShaderParam::setv(const float *f, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform1fv(b->loc, n, f);
    }
}

void LocalShaderParam::setv(const vec *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform3fv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setv(const vec2 *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform2fv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setv(const vec4<float> *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform4fv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setv(const plane *p, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform4fv(b->loc, n, p->v);
    }
}

void LocalShaderParam::setv(const matrix2 *m, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniformMatrix2fv(b->loc, n, GL_FALSE, m->a.v);
    }
}

void LocalShaderParam::setv(const matrix3 *m, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniformMatrix3fv(b->loc, n, GL_FALSE, m->a.v);
    }
}

void LocalShaderParam::setv(const matrix4 *m, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniformMatrix4fv(b->loc, n, GL_FALSE, m->a.v);
    }
}

void LocalShaderParam::set(const matrix2 &m)
{
    setv(&m);
}

void LocalShaderParam::set(const matrix3 &m)
{
    setv(&m);
}

void LocalShaderParam::set(const matrix4 &m)
{
    setv(&m);
}

template<class T>
void sett(T x, T y, T z, T w, LocalShaderParam & sh)
{
    ShaderParamBinding *b = sh.resolve();
    if(b)
    {
        switch(b->format)
        {
            case GL_FLOAT:
            {
                glUniform1f(b->loc, x);
                break;
            }
            case GL_FLOAT_VEC2:
            {
                glUniform2f(b->loc, x, y);
                break;
            }
            case GL_FLOAT_VEC3:
            {
                glUniform3f(b->loc, x, y, z);
                break;
            }
            case GL_FLOAT_VEC4:
            {
                glUniform4f(b->loc, x, y, z, w);
                break;
            }
            case GL_BOOL:
            case GL_INT:
            {
                glUniform1i(b->loc, x);
                break;
            }
            case GL_BOOL_VEC2:
            case GL_INT_VEC2:
            {
                glUniform2i(b->loc, x, y);
                break;
            }
            case GL_BOOL_VEC3:
            case GL_INT_VEC3:
            {
                glUniform3i(b->loc, x, y, z);
                break;
            }
            case GL_BOOL_VEC4:
            case GL_INT_VEC4:
            {
                glUniform4i(b->loc, x, y, z, w);
                break;
            }
            case GL_UNSIGNED_INT:
            {
                glUniform1ui(b->loc, x);
                break;
            }
            case GL_UNSIGNED_INT_VEC2:
            {
                glUniform2ui(b->loc, x, y);
                break;
            }
            case GL_UNSIGNED_INT_VEC3:
            {
                glUniform3ui(b->loc, x, y, z);
                break;
            }
            case GL_UNSIGNED_INT_VEC4:
            {
                glUniform4ui(b->loc, x, y, z, w);
                break;
            }
        }
    }
}
void LocalShaderParam::seti(int x, int y, int z, int w)
{
    sett<int>(x, y, z, w, *this);
}

void LocalShaderParam::set(const ivec &v, int w)
{
    seti(v.x, v.y, v.z, w);
}

void LocalShaderParam::set(const ivec2 &v, int z, int w)
{
    seti(v.x, v.y, z, w);
}

void LocalShaderParam::set(const vec4<int> &v)
{
    seti(v.x, v.y, v.z, v.w);
}

void LocalShaderParam::setv(const int *i, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform1iv(b->loc, n, i);
    }
}
void LocalShaderParam::setv(const ivec *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform3iv(b->loc, n, v->v);
    }
}
void LocalShaderParam::setv(const ivec2 *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform2iv(b->loc, n, v->v);
    }
}
void LocalShaderParam::setv(const vec4<int> *v, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform4iv(b->loc, n, v->v);
    }
}

void LocalShaderParam::setu(uint x, uint y, uint z, uint w)
{
    sett<uint>(x, y, z, w, *this);
}

void LocalShaderParam::setv(const uint *u, int n)
{
    ShaderParamBinding *b = resolve();
    if(b)
    {
        glUniform1uiv(b->loc, n, u);
    }
}
