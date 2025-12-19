#ifndef HUD_H_
#define HUD_H_

extern int statrate; //time between stat updates to show up, in ms
extern int showhud; //bool-like flag, true if hud is being shown

extern float conscale; // size of console and readouts

extern void gl_drawhud(int crosshairindex, void(* hud2d)());

/**
 * @brief Writes crosshairs to the specified fstream.
 *
 * Writes each crosshair as a CubeScript command to the specified fstream.
 *
 * @param f the fstream to write to
 */
extern void writecrosshairs(std::fstream& f);

/**
 * @brief Resets HUD shader and color.
 *
 * Sets the HUD shader color to white (1,1,1).
 */
extern void resethudshader();
#endif
