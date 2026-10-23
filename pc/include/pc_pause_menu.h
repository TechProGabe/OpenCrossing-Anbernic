/* In-game Options: Select pauses the game and opens the Options page
 * (pc_settings_menu.c); leaving the page resumes. */
#ifndef PC_PAUSE_MENU_H
#define PC_PAUSE_MENU_H

#include "pc_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

extern int g_pc_paused;
/* Set by the title screen while its own menu is up: Select does nothing there. */
extern int g_pc_title_main_menu_visible;
/* The buttons that closed the menu are still held; padmgr hides them until released. */
extern int g_pc_pause_input_drain;

void pc_pause_menu_toggle(void);
/* Feed SDL events while paused; 1 = consumed. */
int  pc_pause_menu_handle_event(const SDL_Event* e);
/* graph.c, after game_main: draws the page into this frame's font list. */
void pc_pause_menu_draw(struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif /* PC_PAUSE_MENU_H */
