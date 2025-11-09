#ifndef _UNIX_SDL_H
#define _UNIX_SDL_H

typedef struct sdl_blit_params {
    int x;
    int y;
    int w;
    int h;
} sdl_blit_params;

extern void sdl_close(void);
extern int  sdl_inits(void);
extern int  sdl_inith(void);
extern int  sdl_initho(void);
extern int  sdl_pause(void);
extern void sdl_resize(int x, int y);
extern void sdl_enable(int enable);
extern void sdl_set_fs(int fs);
extern void sdl_reload(void);
extern void sdl_blit(int x, int y, int w, int h);
extern void sdl_reinit_texture(void);

extern void ui_window_title_real(void);


#endif /*_UNIX_SDL_H*/
