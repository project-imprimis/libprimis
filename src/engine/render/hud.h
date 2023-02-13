#ifndef HUD_H_
#define HUD_H_

extern int statrate;
extern int showhud;

extern float conscale;

extern void gl_drawmainmenu();
extern void gl_drawhud(int crosshairindex, void(* hud2d)());
extern void writecrosshairs(std::fstream& f);
extern void resethudshader();
#endif
