#ifndef DYNLIGHT_H_
#define DYNLIGHT_H_

extern void updatedynlights();
extern size_t finddynlights();
extern bool getdynlight(size_t n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags);

#endif
