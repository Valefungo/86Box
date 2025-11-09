#ifndef _UNIX_OSD_H
#define _UNIX_OSD_H

#include <SDL.h>

// state management
extern void osd_init(void);
extern void osd_deinit(void);

// open or close the osd
extern int osd_open(SDL_Event event);
extern int osd_close(SDL_Event event);

// open or close the performance monitor, toggle mode
extern int osd_perfmon_toggle(SDL_Event event);

// show a timed message
extern void osd_toast(const char *header, const char *message);
extern void osd_toast_f(const char *formt, ...);

// keyboard event handler
extern int osd_handle(SDL_Event event);

// draw the osd interface, if it's open
extern void osd_present(void);

// draw the performance monitor, if it's open
extern void osd_perfmon_present(void);

// draw the performance monitor, if it's open
extern void osd_toast_present(void);

// future ui
extern void osd_ui_sb_update_icon_state(int tag, int state);
extern void osd_ui_sb_update_icon(int tag, int active);
extern void osd_ui_sb_update_icon_write(int tag, int active);
extern void osd_ui_sb_update_icon_wp(int tag, int state);

#endif /*_UNIX_OSD_H*/

