#ifndef DYNLIGHT_H_
#define DYNLIGHT_H_

extern void updatedynlights();

/**
 * @brief Finds nearby dynamic lights visible to the player.
 *
 * Finds which dynamic lights are near enough and are visible to the player, and
 * returns the number of lights (and sets `closedynlights` vector contents to the
 * appropriate nearby light ents).
 *
 * The distance at which a dynlight is considered close is controlled by
 * `dynlightdist`.
 *
 * @return number of closeby dynlights
 */
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
