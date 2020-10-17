
enum                            // hardcoded texture numbers
{
    Default_Sky = 0,
    Default_Geom,
    Default_NumDefaults
};

constexpr int octaversion = 33;

struct octaheader
{
    char magic[4];              // "OCTA"
    int version;                // any >8bit quantity is little endian
    int headersize;             // sizeof(header)
    int worldsize;
    int numents;
    int numvars;
    int numvslots;
};

constexpr int currentmapversion = 1;   // bump if map format changes, see worldio.cpp

struct mapheader
{
    char magic[4];              // "TMAP"
    int version;                // any >8bit quantity is little endian
    int headersize;             // sizeof(header)
    int worldsize;
    int numents;
    int numpvs;                 // no longer used, kept for backwards compatibility
    int blendmap;               // also no longer used
    int numvars;
    int numvslots;
};

constexpr float wateramplitude = 0.4f; //max wave height
constexpr float wateroffset = 1.1f;    //wave offset from top of mat volume

enum
{
    MatSurf_NotVisible = 0,
    MatSurf_Visible,
    MatSurf_EditOnly
};

constexpr float defaulttexscale = 16.0f;

struct vertex
{
    vec pos;
    bvec4 norm;
    vec tc;
    bvec4 tangent;
};

