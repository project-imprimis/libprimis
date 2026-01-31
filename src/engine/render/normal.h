#ifndef NORMAL_H_
#define NORMAL_H_

/**
 * @brief Queries for a smoothgroup id, by id.
 *
 * Returns the id passed if that id is associated with a smoothgroup.
 * Returns -1 (failure) if you try to ask for an id greater than 10,000.
 * Returns the size of the smoothgroups vector if id below 0 passed.
 *
 * @param id the id to query
 * @param angle the value to set the angle to, if possible
 *
 * @return the smoothgroup id passed if id is valid index value, -1 if too large, or size of smoothgroups if negative
 */
extern int smoothangle(int id, int angle);

extern void clearnormals();
extern void resetsmoothgroups();


#endif
