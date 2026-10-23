/* pc_pause_menu.c - in-game Options.
 *
 * Select pauses the game (m_play.c skips Game_play_move, padmgr.c hides
 * the pads) and opens the same Options page the title screen uses, drawn
 * over the paused scene. Closing the page resumes. Based on
 * flyngmt/ACGC-PC-Port's pause menu, minus its Resume/Settings/Quit page:
 * one settings screen, not two. */
#include "pc_pause_menu.h"
#include "pc_settings_menu.h"

#include "m_font.h"
#include "graph.h"

#include <stdio.h>

int g_pc_paused = 0;
int g_pc_title_main_menu_visible = 0;
int g_pc_pause_input_drain = 0;

extern int g_pc_nes_active; /* pc_gx.c: pausing an NES game crashes it */

static void pause_close(void) {
    g_pc_paused = 0;
    g_pc_pause_input_drain = 1;
}

void pc_pause_menu_toggle(void) {
    if (g_pc_paused) {
        /* Select again: same as Back, so unapplied changes still ask */
        if (!pc_settings_menu_cancel()) pause_close();
        return;
    }
    if (g_pc_title_main_menu_visible || g_pc_nes_active) return;
    g_pc_paused = 1;
    pc_settings_menu_enter();
}

typedef enum {
    ACT_NONE = 0,
    ACT_UP,
    ACT_DOWN,
    ACT_LEFT,
    ACT_RIGHT,
    ACT_CONFIRM,
    ACT_CANCEL,
} MenuAction;

static void handle_action(MenuAction act) {
    int open = 1;
    switch (act) {
        case ACT_UP:      open = pc_settings_menu_nav_up(); break;
        case ACT_DOWN:    open = pc_settings_menu_nav_down(); break;
        case ACT_LEFT:    open = pc_settings_menu_nav_left(); break;
        case ACT_RIGHT:   open = pc_settings_menu_nav_right(); break;
        case ACT_CONFIRM: open = pc_settings_menu_confirm(); break;
        case ACT_CANCEL:  open = pc_settings_menu_cancel(); break;
        default: break;
    }
    if (!open) pause_close();
}

static MenuAction translate_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:       return ACT_UP;
        case SDLK_DOWN:     return ACT_DOWN;
        case SDLK_LEFT:     return ACT_LEFT;
        case SDLK_RIGHT:    return ACT_RIGHT;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:    return ACT_CONFIRM;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE: return ACT_CANCEL;
    }
    return ACT_NONE;
}

static MenuAction translate_pad_button(int button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    return ACT_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return ACT_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return ACT_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return ACT_RIGHT;
        case SDL_CONTROLLER_BUTTON_A:          return ACT_CONFIRM;
        case SDL_CONTROLLER_BUTTON_B:          return ACT_CANCEL;
    }
    return ACT_NONE;
}

/* Left-stick nav with a per-axis latch so one push = one step. */
#define STICK_NAV_PRESS   18000
#define STICK_NAV_RELEASE 12000

static MenuAction translate_pad_axis(int axis, int value) {
    static int latch_x = 0, latch_y = 0;
    int* latch;
    MenuAction neg, pos;

    if (axis == SDL_CONTROLLER_AXIS_LEFTX)      { latch = &latch_x; neg = ACT_LEFT; pos = ACT_RIGHT; }
    else if (axis == SDL_CONTROLLER_AXIS_LEFTY) { latch = &latch_y; neg = ACT_UP;   pos = ACT_DOWN; }
    else return ACT_NONE;

    if (*latch != 0) {
        if (value > -STICK_NAV_RELEASE && value < STICK_NAV_RELEASE) *latch = 0;
        return ACT_NONE;
    }
    if (value <= -STICK_NAV_PRESS) { *latch = -1; return neg; }
    if (value >=  STICK_NAV_PRESS) { *latch =  1; return pos; }
    return ACT_NONE;
}

int pc_pause_menu_handle_event(const SDL_Event* e) {
    if (!g_pc_paused) return 0;

    switch (e->type) {
        case SDL_KEYDOWN:
            if (!e->key.repeat) handle_action(translate_key(e->key.keysym.sym));
            return 1;
        case SDL_CONTROLLERBUTTONDOWN:
            handle_action(translate_pad_button(e->cbutton.button));
            return 1;
        case SDL_CONTROLLERAXISMOTION:
            handle_action(translate_pad_axis(e->caxis.axis, e->caxis.value));
            return 1;
    }
    return 0;
}

void pc_pause_menu_draw(struct game_s* game) {
    if (!g_pc_paused || game == NULL || game->graph == NULL) return;
    if (!pc_settings_menu_active()) { /* closed from inside */
        pause_close();
        return;
    }

    /* Font ortho projection + identity modelview, else the page inherits
     * whatever projection the scene ended its frame with. */
    mFont_SetMatrix(game->graph, mFont_MODE_FONT);
    pc_settings_menu_draw(game, /*with_dim_backdrop=*/1);
    mFont_UnSetMatrix(game->graph, mFont_MODE_FONT);
}
