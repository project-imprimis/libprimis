
#ifndef OCTACUBE_H_
#define OCTACUBE_H_

extern bool touchingface(const cube &c, int orient);
extern bool notouchingface(const cube &c, int orient);

extern bool mincubeface(const cube &cu, int orient, const ivec &co, int size, facebounds &orig);

#endif
