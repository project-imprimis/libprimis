#ifndef DYNLIGHT_H_
#define DYNLIGHT_H_

extern void updatedynlights();
extern size_t finddynlights();

/**
 * @brief Gets the nth dynlight near camera and sets references to its values
 *
 *  @param n the nth closest dynamic light
 *  @param o a reference to set as the location of the specified dynlight
 *  @param radius a reference to set as the radius of the specified dynlight
 *  @param color a reference to set as the color of the specifeid dynlight
 *  @param spot a reference to the spotlight information of the dynlight
 *  @param dir a reference to set as the direction the dynlight is pointing
 *  @param flags a reference to the flag bitmap for the dynlight
 *
 *  @return true if light at position n was found, false otherwise
 *
 */
extern bool getdynlight(size_t n, vec &o, float &radius, vec &color, vec &dir, int &spot, int &flags);

#endif
