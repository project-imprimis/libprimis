/* rendersky.cpp: skybox and sky environment rendering
 *
 * Libprimis supports standard static cubemap skyboxes as well as cloud layers,
 * which are both static (though capable of translation/rotation)
 *
 * as well as a parameterized sky generation system, referred to the code as the
 * atmo" functionality
 *
 * as expected for distant scenery, like the sky and clouds are, there is no support
 * for parallax (cloud layers do not move relative to background sky as the player
 * moves)
 */
#include "../libprimis-headers/cube.h"
#include "../../shared/geomexts.h"
#include "../../shared/glemu.h"
#include "../../shared/glexts.h"
#include "../../shared/stream.h"

#include "octarender.h"
#include "rendergl.h"
#include "renderlights.h"
#include "rendersky.h"
#include "renderva.h"
#include "shader.h"
#include "shaderparam.h"
#include "texture.h"

#include "interface/console.h"
#include "interface/control.h"

#include "world/light.h"
#include "world/octaedit.h"
#include "world/octaworld.h"
#include "world/raycube.h"

VARFR(skyshadow, 0, 0, 1, clearshadowcache());   //toggles rsm features in renderva.cpp

bool explicitsky = false;

// internally relevant functionality
namespace
{
    VARNR(skytexture, useskytexture, 0, 0, 1);       //toggles rendering sky texture instead of nothing on skytex'd geometry

    std::array<const Texture *, 6> sky = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    void loadsky(const char *basename, std::array<const Texture *, 6> &texs)
    {

        struct cubemapside
        {
            GLenum target;
            const char *name;
            bool flipx, flipy, swapxy;
        };
        static const cubemapside cubemapsides[6] =
        {
            { GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "lf", false, true,  true  },
            { GL_TEXTURE_CUBE_MAP_POSITIVE_X, "rt", true,  false, true  },
            { GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "bk", false, false, false },
            { GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "ft", true,  true,  false },
            { GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "dn", true,  false, true  },
            { GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "up", true,  false, true  },
        };

        const char *wildcard = std::strchr(basename, '*');
        for(int i = 0; i < 6; ++i) //six sides for a cubemap
        {
            const char *side = cubemapsides[i].name;
            string name;
            copystring(name, makerelpath("media/sky", basename));
            if(wildcard)
            {
                char *chop = std::strchr(name, '*');
                if(chop)
                {
                    *chop = '\0';
                    concatstring(name, side);
                    concatstring(name, wildcard+1);
                }
                texs[i] = textureload(name, 3, true, false);
            }
            else
            {
                DEF_FORMAT_STRING(ext, "_%s.jpg", side);
                concatstring(name, ext);
                if((texs[i] = textureload(name, 3, true, false))==notexture)
                {
                    std::strcpy(name+std::strlen(name)-3, "png");
                    texs[i] = textureload(name, 3, true, false);
                }
            }
            if(texs[i]==notexture)
            {
                conoutf(Console_Error, "could not load side %s of sky texture %s", side, basename);
            }
        }
    }

    Texture *cloudoverlay = nullptr;

    Texture *loadskyoverlay(const char *basename)
    {
        const char *ext = std::strrchr(basename, '.');
        string name;
        copystring(name, makerelpath("media/sky", basename));
        Texture *t = notexture;
        if(ext)
        {
            t = textureload(name, 0, true, false);
        }
        else
        {
            concatstring(name, ".jpg");
            if((t = textureload(name, 0, true, false)) == notexture)
            {
                std::strcpy(name+std::strlen(name)-3, "png");
                t = textureload(name, 0, true, false);
            }
        }
        if(t==notexture)
        {
            conoutf(Console_Error, "could not load sky overlay texture %s", basename);
        }
        return t;
    }

    SVARFR(skybox, "",
    {
        if(skybox[0])
        {
            loadsky(skybox, sky);
        }
    });                                             //path to skybox
    CVARR(skyboxcolor, 0xFFFFFF);                   //color to multiply skybox texture by
    FVARR(skyboxoverbright, 1, 2, 16);              //amount by which skybox can exceed 0xFFFFFF
    FVARR(skyboxoverbrightmin, 0, 1, 16);
    FVARR(skyboxoverbrightthreshold, 0, 0.7f, 1);
    FVARR(skyboxspin, -720, 0, 720);                //skybox spin rate in degrees per second
    VARR (skyboxyaw, 0, 0, 360);                    //skybox rotation offset in degrees

    //cloud layer variables
    FVARR(cloudclip, 0, 0.5f, 1);
    SVARFR(cloudlayer, "",
    {
        if(cloudlayer[0])
        {
            cloudoverlay = loadskyoverlay(cloudlayer);
        }
    });
    FVARR(cloudoffsetx, 0, 0, 1); //offset of cloud texture: 1 is equal to 1 tex-width x
    FVARR(cloudoffsety, 0, 0, 1); //offset of cloud texture: 1 is equal to 1 tex-width y
    FVARR(cloudscrollx, -16, 0, 16);
    FVARR(cloudscrolly, -16, 0, 16);
    FVARR(cloudscale, 0.001, 1, 64);
    FVARR(cloudspin, -720, 0, 720);
    VARR (cloudyaw, 0, 0, 360);
    FVARR(cloudheight, -1, 0.2f, 1);
    FVARR(cloudfade, 0, 0.2f, 1);
    FVARR(cloudalpha, 0, 1, 1);
    VARR (cloudsubdiv, 4, 16, 64);
    CVARR(cloudcolor, 0xFFFFFF);

    void drawenvboxface(float s0, float t0, int x0, int y0, int z0,
                        float s1, float t1, int x1, int y1, int z1,
                        float s2, float t2, int x2, int y2, int z2,
                        float s3, float t3, int x3, int y3, int z3,
                        const Texture *tex)
    {
        glBindTexture(GL_TEXTURE_2D, (tex ? tex : notexture)->id);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x3, y3, z3); gle::attribf(s3, t3);
        gle::attribf(x2, y2, z2); gle::attribf(s2, t2);
        gle::attribf(x0, y0, z0); gle::attribf(s0, t0);
        gle::attribf(x1, y1, z1); gle::attribf(s1, t1);
        xtraverts += gle::end();
    }

    void drawenvbox(const std::array<const Texture *, 6> &sky, float z1clip = 0.0f, float z2clip = 1.0f, int faces = 0x3F)
    {
        if(z1clip >= z2clip)
        {
            return;
        }
        float v1 = 1-z1clip,
              v2 = 1-z2clip;
        int w  = farplane/2,
            z1 = static_cast<int>(std::ceil(2*w*(z1clip-0.5f))),
            z2 = static_cast<int>(std::ceil(2*w*(z2clip-0.5f)));

        gle::defvertex();
        gle::deftexcoord0();

        //daw the six faces of the skybox's cubemap
        if(faces&0x01)
        {
            drawenvboxface(1.0f, v2,  -w, -w, z2,
                           0.0f, v2,  -w,  w, z2,
                           0.0f, v1,  -w,  w, z1,
                           1.0f, v1,  -w, -w, z1, sky[0]);
        }
        if(faces&0x02)
        {
            drawenvboxface(0.0f, v1, w, -w, z1,
                           1.0f, v1, w,  w, z1,
                           1.0f, v2, w,  w, z2,
                           0.0f, v2, w, -w, z2, sky[1]);
        }
        if(faces&0x04)
        {
            drawenvboxface(0.0f, v1, -w, -w, z1,
                           1.0f, v1,  w, -w, z1,
                           1.0f, v2,  w, -w, z2,
                           0.0f, v2, -w, -w, z2, sky[2]);
        }
        if(faces&0x08)
        {
            drawenvboxface(0.0f, v1,  w,  w, z1,
                           1.0f, v1, -w,  w, z1,
                           1.0f, v2, -w,  w, z2,
                           0.0f, v2,  w,  w, z2, sky[3]);
        }
        if(z1clip <= 0 && faces&0x10)
        {
            drawenvboxface(1.0f, 1.0f, -w,  w,  -w,
                           1.0f, 0.0f,  w,  w,  -w,
                           0.0f, 0.0f,  w, -w,  -w,
                           0.0f, 1.0f, -w, -w,  -w, sky[4]);
        }
        if(z2clip >= 1 && faces&0x20)
        {
            drawenvboxface(1.0f, 1.0f,  w,  w, w,
                           1.0f, 0.0f, -w,  w, w,
                           0.0f, 0.0f, -w, -w, w,
                           0.0f, 1.0f,  w, -w, w, sky[5]);
        }
    }

    void drawenvoverlay(const Texture *overlay = nullptr, float tx = 0, float ty = 0)
    {
        int w = farplane/2;
        float z   = w*cloudheight,
              tsz = 0.5f*(1-cloudfade)/cloudscale,
              psz = w*(1-cloudfade);
        glBindTexture(GL_TEXTURE_2D, (overlay ? overlay : notexture)->id);
        vec color = cloudcolor.tocolor();
        gle::color(color, cloudalpha);
        gle::defvertex();
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_FAN);
        for(int i = 0; i < cloudsubdiv+1; ++i)
        {
            vec p(1, 1, 0);
            p.rotate_around_z((-2.0f*M_PI*i)/cloudsubdiv);
            gle::attribf(p.x*psz, p.y*psz, z);
            gle::attribf(tx - p.x*tsz, ty + p.y*tsz);
        }
        xtraverts += gle::end();
        float tsz2 = 0.5f/cloudscale;
        gle::defvertex();
        gle::deftexcoord0();
        gle::defcolor(4);
        gle::begin(GL_TRIANGLE_STRIP);
        for(int i = 0; i < cloudsubdiv+1; ++i)
        {
            vec p(1, 1, 0);
            p.rotate_around_z((-2.0f*M_PI*i)/cloudsubdiv);
            gle::attribf(p.x*psz, p.y*psz, z);
            gle::attribf(tx - p.x*tsz, ty + p.y*tsz);
            gle::attrib(color, cloudalpha);
            gle::attribf(p.x*w, p.y*w, z);
            gle::attribf(tx - p.x*tsz2, ty + p.y*tsz2);
            gle::attrib(color, 0.0f);
        }
        xtraverts += gle::end();
    }

    /* === "atmo" parameterized, procedurally generated sky === */
    VARR(atmo, 0, 0, 1);
    FVARR(atmoplanetsize, 1e-3f, 8, 1e3f);
    FVARR(atmoheight, 1e-3f, 1, 1e3f);
    FVARR(atmobright, 0, 4, 16);
    CVAR1R(atmosunlight, 0);
    FVARR(atmosunlightscale, 0, 1, 16);
    FVARR(atmosundisksize, 0, 1, 10);
    FVARR(atmosundiskbright, 0, 1, 16);
    FVARR(atmohaze, 0, 0.03f, 1);
    CVAR0R(atmohazefade, 0xAEACA9);
    FVARR(atmohazefadescale, 0, 1, 1);
    FVARR(atmoclarity, 0, 0.2f, 10);
    FVARR(atmodensity, 1e-3f, 0.99f, 10);
    FVARR(atmoalpha, 0, 1, 1);

    void drawatmosphere()
    {
        SETSHADER(atmosphere);

        matrix4 sunmatrix = cammatrix.inverse();
        sunmatrix.settranslation(0, 0, 0);
        sunmatrix.mul(projmatrix.inverse());
        LOCALPARAM(sunmatrix, sunmatrix);

        LOCALPARAM(sunlight, (!atmosunlight.iszero() ? atmosunlight.tocolor().mul(atmosunlightscale) : sunlight.tocolor().mul(sunlightscale)).mul(atmobright*ldrscale));
        LOCALPARAM(sundir, sunlightdir);

        vec sundiskparams;
        sundiskparams.y = -(1 - 0.0075f * atmosundisksize);
        sundiskparams.x = 1/(1 + sundiskparams.y);
        sundiskparams.y *= sundiskparams.x;
        sundiskparams.z = atmosundiskbright;
        LOCALPARAM(sundiskparams, sundiskparams);

        const float earthradius     = 6.371e6f, //radius of earth in meters
                    earthatmoheight = 100e3f; //atmospheric height (100km)
        float planetradius = earthradius*atmoplanetsize,
              atmoradius   = planetradius + earthatmoheight*atmoheight;
        LOCALPARAMF(atmoradius, planetradius, atmoradius*atmoradius, atmoradius*atmoradius - planetradius*planetradius);

        float gm = (1 - atmohaze)*0.2f + 0.75f;
        LOCALPARAMF(gm, gm);

        vec lambda(680e-9f, 550e-9f, 450e-9f),
            betar = vec(lambda).square().square().recip().mul(1.86e-31f / atmodensity),
            betam = vec(lambda).recip().mul(2*M_PI).square().mul(atmohazefade.tocolor().mul(atmohazefadescale)).mul(1.36e-19f * std::max(atmohaze, 1e-3f)),
            betarm = vec(betar).div(1+atmoclarity).add(betam);
        betar.div(betarm).mul(3/(16*M_PI));
        betam.div(betarm).mul((1-gm)*(1-gm)/(4*M_PI));
        LOCALPARAM(betar, betar);
        LOCALPARAM(betam, betam);
        LOCALPARAM(betarm, betarm.div(M_LN2));

        LOCALPARAMF(atmoalpha, atmoalpha);

        gle::defvertex();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(-1, 1, 1);
        gle::attribf(1, 1, 1);
        gle::attribf(-1, -1, 1);
        gle::attribf(1, -1, 1);
        xtraverts += gle::end();
    }

    /* === general sky rendering === */
    VAR(showsky, 0, 1, 1);
    VAR(clampsky, 0, 1, 1);
}

// externally relevant functionality

bool limitsky()
{
    return explicitsky && (useskytexture || editmode);
}

void drawskybox(bool clear)
{
    bool limited = false;
    if(limitsky())
    {
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            if(va->sky && va->occluded < Occlude_BB &&
               ((va->skymax.x >= 0 && view.isvisiblebb(va->skymin, ivec(va->skymax).sub(va->skymin)) != ViewFrustumCull_NotVisible) ||
                !insideworld(camera1->o)))
            {
                limited = true;
                break;
            }
        }
    }
    if(limited)
    {
        glDisable(GL_DEPTH_TEST);
    }
    else
    {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
    }
    if(clampsky)
    {
        glDepthRange(1, 1); //set gl depth range min and max to 1 (all far away)
    }
    if(clear || (!skybox[0] && (!atmo || atmoalpha < 1)))
    {
        vec skyboxcol = skyboxcolor.tocolor().mul(ldrscale); //local skyboxcol was skyboxcolor before skyboxcolour -> skyboxcolor uniformity change
        glClearColor(skyboxcol.x, skyboxcol.y, skyboxcol.z, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    if(skybox[0])
    {
        if(ldrscale < 1 && (skyboxoverbrightmin != 1 || (skyboxoverbright > 1 && skyboxoverbrightthreshold < 1)))
        {
            SETSHADER(skyboxoverbright);
            LOCALPARAMF(overbrightparams, skyboxoverbrightmin, std::max(skyboxoverbright, skyboxoverbrightmin), skyboxoverbrightthreshold);
        }
        else
        {
            SETSHADER(skybox);
        }
        gle::color(skyboxcolor);

        matrix4 skymatrix = cammatrix,
                skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((skyboxspin*lastmillis/1000.0f+skyboxyaw)/RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvbox(sky);
    }
    if(atmo && (!skybox[0] || atmoalpha < 1))
    {
        if(atmoalpha < 1)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        drawatmosphere();

        if(atmoalpha < 1)
        {
            glDisable(GL_BLEND);
        }
    }
    if(cloudlayer[0] && cloudheight)
    {
        SETSHADER(skybox);

        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        matrix4 skymatrix = cammatrix,
                skyprojmatrix;
        skymatrix.settranslation(0, 0, 0);
        skymatrix.rotate_around_z((cloudspin*lastmillis/1000.0f+cloudyaw)/RAD);
        skyprojmatrix.mul(projmatrix, skymatrix);
        LOCALPARAM(skymatrix, skyprojmatrix);

        drawenvoverlay(cloudoverlay, cloudoffsetx + cloudscrollx * lastmillis/1000.0f, cloudoffsety + cloudscrolly * lastmillis/1000.0f);

        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
    }
    if(clampsky)
    {
        glDepthRange(0, 1); //return depth range to normal
    }
    if(limited)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }
}
