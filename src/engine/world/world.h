
constexpr int octaversion = 33;
constexpr int currentmapversion = 1;   // bump if map format changes, see worldio.cpp

constexpr float wateramplitude = 0.4f; //max wave height
constexpr float wateroffset = 1.1f;    //wave offset from top of mat volume

enum
{
    MatSurf_NotVisible = 0,
    MatSurf_Visible,
    MatSurf_EditOnly
};

constexpr float defaulttexscale = 16.0f;

extern int worldscale, worldsize;
extern int mapversion;
extern char *maptitle;
extern int octaentsize;

extern std::vector<int> entgroup;
extern std::vector<int> outsideents;

extern void freeoctaentities(cube &c);
extern void entcancel();
extern void entselectionbox(const entity &e, vec &eo, vec &es);

namespace entities
{
    extern extentity *newentity();
    extern void deleteentity(extentity *e);
    extern vector<extentity *> &getents();
}
