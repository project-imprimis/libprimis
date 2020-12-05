#ifndef HUD_H_
#define HUD_H_

extern int statrate;

extern void gl_drawmainmenu();
extern void gl_drawhud(int crosshairindex);
extern void writecrosshairs(stream *f);
#endif
