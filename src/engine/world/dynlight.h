#ifndef DYNLIGHT_H_
#define DYNLIGHT_H_

extern void updatedynlights();
extern int finddynlights();
extern bool getdynlight(int n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags);

#endif
