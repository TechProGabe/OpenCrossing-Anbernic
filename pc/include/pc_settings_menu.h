/* The Options page, shared by the title screen's Options and the in-game
 * menu (Select). Drawn in the game's own window style from the font
 * display list; see pc_settings_menu.c. */

#ifndef PC_SETTINGS_MENU_H
#define PC_SETTINGS_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

/* Copy g_pc_settings into the pending edits and reset the cursor. Call when
 * opening the page from either host. */
void pc_settings_menu_enter(void);

/* 1 while the page (or one of its questions) is open. */
int  pc_settings_menu_active(void);

/* Navigation. Each returns 1 while the page stays open, 0 once it closed. */
int  pc_settings_menu_nav_up(void);
int  pc_settings_menu_nav_down(void);
int  pc_settings_menu_nav_left(void);
int  pc_settings_menu_nav_right(void);
int  pc_settings_menu_confirm(void);
int  pc_settings_menu_cancel(void);

/* Draw into the font display list. with_dim_backdrop darkens the scene
 * behind the page. The caller has already loaded the font projection. */
void pc_settings_menu_draw(struct game_s* game, int with_dim_backdrop);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_MENU_H */
