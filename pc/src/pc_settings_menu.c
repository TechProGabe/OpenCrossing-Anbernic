/* pc_settings_menu.c - the Options page: one settings screen for the title
 * screen's Options and the in-game menu (Select).
 *
 * Adapted from OpenCrossing-Xbox's xbox_settings_menu.c, which grew out of
 * flyngmt/ACGC-PC-Port's pc_settings_menu.c. The page is drawn like the
 * game's own windows: a cream sheet in a wood frame with a green name tag,
 * ruled like a notebook, the game font in brown ink, and a help line for
 * the selected row. Shapes are untextured, vertex-coloured triangles in the
 * font display list, in the game's 320x240 font space. Changes wait for
 * Apply; leaving with unapplied changes asks first.
 *
 * Only the game font's glyphs render: no '/', '+', '*', '[', ';' or '#' in
 * any string here. */
#include "pc_settings_menu.h"
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_keybindings.h"
#include "pc_text_draw.h"

#include "game.h"
#include "graph.h"
#include "m_font.h"
#include "m_rcp.h"
#include "main.h" /* SCREEN_WIDTH_F */
#include "THA_GA.h"
#include "libforest/gbi_extensions.h" /* G_TRIN_INDEPEND */

#include <math.h>
#include <stdio.h>
#include <string.h>

extern int  g_pc_fps_target;
extern int  g_pc_no_framelimit;
extern int  g_pc_scale_mode;
extern void pc_dynamic_fps_reset(void);
extern int  pc_pad_dpad_as_stick_active(void);

enum {
    ITEM_FPS_TARGET,
    ITEM_RENDER_SCALE,
    ITEM_SCALE_MODE,
    ITEM_FPS_COUNTER,
    ITEM_NES_ASPECT,
    ITEM_MASTER_VOLUME,
    ITEM_MUSIC_VOLUME,
    ITEM_SFX_VOLUME,
    ITEM_VOICE_VOLUME,
    ITEM_SWAP_AB,
    ITEM_DPAD_STICK,
    ITEM_LEFT_DEADZONE,
    ITEM_RIGHT_DEADZONE,
    ITEM_ZOOM,
    ITEM_ENDLESS_ACRES,
    ITEM_BELLS_TO_WALLET,
    ITEM_SHOP_UPGRADE,
    ITEM_RESETTI,
    ITEM_SHADOWS,
    ITEM_PARTICLES,
    ITEM_ACRE_DRAW,
    ITEM_CULLING,
    ITEM_LOGGING,
};

typedef struct {
    const char* label;
    int         id;
} Item;

static const Item tab_video_items[] = {
    { "FPS target",   ITEM_FPS_TARGET },
    { "Render scale", ITEM_RENDER_SCALE },
    { "Upscaling",    ITEM_SCALE_MODE },
    { "FPS counter",  ITEM_FPS_COUNTER },
    { "NES picture",  ITEM_NES_ASPECT },
};
static const Item tab_audio_items[] = {
    { "Master",        ITEM_MASTER_VOLUME },
    { "Music",         ITEM_MUSIC_VOLUME },
    { "Sound effects", ITEM_SFX_VOLUME },
    { "Voices",        ITEM_VOICE_VOLUME },
};
static const Item tab_input_items[] = {
    { "Swap A and B",   ITEM_SWAP_AB },
    { "D-pad walks",    ITEM_DPAD_STICK },
    { "Stick deadzone", ITEM_LEFT_DEADZONE },
    { "C-stick zone",   ITEM_RIGHT_DEADZONE },
    { "Camera zoom",    ITEM_ZOOM },
};
static const Item tab_game_items[] = {
    { "Endless acres",   ITEM_ENDLESS_ACRES },
    { "Bells to wallet", ITEM_BELLS_TO_WALLET },
    { "Shop upgrade",    ITEM_SHOP_UPGRADE },
    { "Resetti",         ITEM_RESETTI },
};
static const Item tab_perf_items[] = {
    { "Shadows",   ITEM_SHADOWS },
    { "Particles", ITEM_PARTICLES },
    { "Acre draw", ITEM_ACRE_DRAW },
    { "Culling",   ITEM_CULLING },
    { "Logging",   ITEM_LOGGING },
};

typedef struct {
    const char* name;
    const Item* items;
    int         count;
} Tab;

#define TAB_ITEMS(a) (a), (int)(sizeof(a) / sizeof((a)[0]))
static const Tab s_tabs[] = {
    { "Video", TAB_ITEMS(tab_video_items) },
    { "Audio", TAB_ITEMS(tab_audio_items) },
    { "Input", TAB_ITEMS(tab_input_items) },
    { "Game",  TAB_ITEMS(tab_game_items) },
    { "Perf",  TAB_ITEMS(tab_perf_items) },
};
#define TAB_COUNT ((int)(sizeof(s_tabs) / sizeof(s_tabs[0])))

typedef enum {
    SUB_SETTINGS,
    SUB_CONFIRM_BACK,
} SubPage;

static int     s_active;
static SubPage s_sub = SUB_SETTINGS;
static int     s_tab;
static int     s_sel = -1;  /* -1 = the tab row */
static int     s_back_sel;  /* 0 = Keep editing, 1 = Discard */
static int     s_logged_room;

/* pending edits, committed on Apply */
static PCSettings s_pending;
static int        s_dirty;

/* ---- pending state ---- */

static void recompute_dirty(void) {
    s_dirty = memcmp(&s_pending, &g_pc_settings, sizeof s_pending) != 0;
}

static int step_clamp(int v, int step, int dir, int lo, int hi) {
    v += dir > 0 ? step : -step;
    return v < lo ? lo : v > hi ? hi : v;
}

static int step_wrap(int v, int dir, int n) {
    return (v + (dir > 0 ? 1 : n - 1)) % n;
}

/* index of v in a list, or fallback */
static int list_index(const int* list, int n, int v, int fallback) {
    int i;
    for (i = 0; i < n; i++)
        if (list[i] == v) return i;
    return fallback;
}

static const int k_render_scales[] = { 25, 50, 75, 100 };
/* shadow_quality values from most to least: all, player and NPCs, player, off */
static const int k_shadow_order[] = { 0, 3, 1, 2 };

static void item_cycle(int id, int dir) {
    PCSettings* p = &s_pending;
    switch (id) {
        case ITEM_FPS_TARGET:
            /* the list runs 60, 50, 40, 30, 20, Unlimited, Dynamic */
            p->fps_target = step_wrap(p->fps_target < 0 || p->fps_target > 6 ? 6 : p->fps_target, dir, 7);
            break;
        case ITEM_RENDER_SCALE: {
            int i = list_index(k_render_scales, 4, p->render_scale, 3);
            p->render_scale = k_render_scales[dir > 0 ? (i < 3 ? i + 1 : 3) : (i > 0 ? i - 1 : 0)];
            break;
        }
        case ITEM_SCALE_MODE:      p->scale_mode ^= 1; break;
        case ITEM_FPS_COUNTER:     p->show_fps ^= 1; break;
        case ITEM_NES_ASPECT:      p->nes_aspect ^= 1; break;
        case ITEM_MASTER_VOLUME:   p->master_volume = step_clamp(p->master_volume, 10, dir, 0, 100); break;
        case ITEM_MUSIC_VOLUME:    p->bgm_volume = step_clamp(p->bgm_volume, 10, dir, 0, 100); break;
        case ITEM_SFX_VOLUME:      p->sfx_volume = step_clamp(p->sfx_volume, 10, dir, 0, 100); break;
        case ITEM_VOICE_VOLUME:    p->voice_volume = step_clamp(p->voice_volume, 10, dir, 0, 100); break;
        case ITEM_SWAP_AB:         p->swap_ab_xy ^= 1; break;
        case ITEM_DPAD_STICK:      p->dpad_as_stick = step_wrap(p->dpad_as_stick, dir, 3); break;
        case ITEM_LEFT_DEADZONE:   p->left_deadzone = step_clamp(p->left_deadzone, 5, dir, 0, 50); break;
        case ITEM_RIGHT_DEADZONE:  p->right_deadzone = step_clamp(p->right_deadzone, 5, dir, 0, 50); break;
        case ITEM_ZOOM:            p->zoom_enabled ^= 1; break;
        case ITEM_ENDLESS_ACRES:   p->borderless_acres ^= 1; break;
        case ITEM_BELLS_TO_WALLET: p->bells_to_wallet ^= 1; break;
        case ITEM_SHOP_UPGRADE:    p->shop_no_visitor ^= 1; break;
        case ITEM_RESETTI:         p->disable_resetti ^= 1; break;
        case ITEM_SHADOWS: {
            int i = list_index(k_shadow_order, 4, p->shadow_quality, 0);
            p->shadow_quality = k_shadow_order[step_wrap(i, dir, 4)];
            break;
        }
        case ITEM_PARTICLES:       p->particle_quality = step_clamp(p->particle_quality, 1, dir, 0, 4); break;
        case ITEM_ACRE_DRAW:       p->reduce_acre_draw = step_wrap(p->reduce_acre_draw, dir, 3); break;
        case ITEM_CULLING:         p->frustum_cull ^= 1; break;
        case ITEM_LOGGING:         p->verbose ^= 1; break;
    }
    recompute_dirty();
}

static const char* on_off(int v) { return v ? "On" : "Off"; }

/* the value alone: the page draws the arrows */
static void item_format(int id, char* buf, size_t n) {
    const PCSettings* p = &s_pending;
    switch (id) {
        case ITEM_FPS_TARGET: {
            static const char* const names[] = { "60", "50", "40", "30", "20", "Unlimited", "Dynamic" };
            int t = p->fps_target < 0 || p->fps_target > 6 ? 6 : p->fps_target;
            snprintf(buf, n, "%s", names[t]);
            break;
        }
        case ITEM_RENDER_SCALE:    snprintf(buf, n, "%d%%", p->render_scale); break;
        case ITEM_SCALE_MODE:      snprintf(buf, n, "%s", p->scale_mode ? "Center" : "Stretch"); break;
        case ITEM_FPS_COUNTER:     snprintf(buf, n, "%s", on_off(p->show_fps)); break;
        case ITEM_NES_ASPECT:      snprintf(buf, n, "%s", p->nes_aspect ? "4:3" : "Stretch"); break;
        case ITEM_MASTER_VOLUME:   snprintf(buf, n, "%d%%", p->master_volume); break;
        case ITEM_MUSIC_VOLUME:    snprintf(buf, n, "%d%%", p->bgm_volume); break;
        case ITEM_SFX_VOLUME:      snprintf(buf, n, "%d%%", p->sfx_volume); break;
        case ITEM_VOICE_VOLUME:    snprintf(buf, n, "%d%%", p->voice_volume); break;
        case ITEM_SWAP_AB:         snprintf(buf, n, "%s", on_off(p->swap_ab_xy)); break;
        case ITEM_DPAD_STICK:
            if (p->dpad_as_stick == 2)
                snprintf(buf, n, "Auto, %s", pc_pad_dpad_as_stick_active() ? "on" : "off");
            else
                snprintf(buf, n, "%s", on_off(p->dpad_as_stick));
            break;
        case ITEM_LEFT_DEADZONE:   snprintf(buf, n, "%d%%", p->left_deadzone); break;
        case ITEM_RIGHT_DEADZONE:  snprintf(buf, n, "%d%%", p->right_deadzone); break;
        case ITEM_ZOOM:            snprintf(buf, n, "%s", on_off(p->zoom_enabled)); break;
        case ITEM_ENDLESS_ACRES:   snprintf(buf, n, "%s", on_off(p->borderless_acres)); break;
        case ITEM_BELLS_TO_WALLET: snprintf(buf, n, "%s", on_off(p->bells_to_wallet)); break;
        case ITEM_SHOP_UPGRADE:    snprintf(buf, n, "%s", p->shop_no_visitor ? "No visitor" : "Original"); break;
        case ITEM_RESETTI:         snprintf(buf, n, "%s", p->disable_resetti ? "Disabled" : "Enabled"); break;
        case ITEM_SHADOWS: {
            static const char* const names[] = { "All", "Player", "Off", "Player, NPCs" };
            int s = p->shadow_quality < 0 || p->shadow_quality > 3 ? 0 : p->shadow_quality;
            snprintf(buf, n, "%s", names[s]);
            break;
        }
        case ITEM_PARTICLES: {
            static const char* const names[] = { "Off", "25%", "50%", "75%", "Full" };
            int q = p->particle_quality < 0 || p->particle_quality > 4 ? 4 : p->particle_quality;
            snprintf(buf, n, "%s", names[q]);
            break;
        }
        case ITEM_ACRE_DRAW: {
            static const char* const names[] = { "Full", "Cross", "Current" };
            int a = p->reduce_acre_draw < 0 || p->reduce_acre_draw > 2 ? 0 : p->reduce_acre_draw;
            snprintf(buf, n, "%s", names[a]);
            break;
        }
        case ITEM_CULLING:         snprintf(buf, n, "%s", on_off(p->frustum_cull)); break;
        case ITEM_LOGGING:         snprintf(buf, n, "%s", p->verbose ? "Verbose" : "Errors"); break;
        default:                   buf[0] = '\0'; break;
    }
}

static int item_changed(int id) {
    const PCSettings* p = &s_pending;
    const PCSettings* g = &g_pc_settings;
    switch (id) {
        case ITEM_FPS_TARGET:      return p->fps_target != g->fps_target;
        case ITEM_RENDER_SCALE:    return p->render_scale != g->render_scale;
        case ITEM_SCALE_MODE:      return p->scale_mode != g->scale_mode;
        case ITEM_FPS_COUNTER:     return p->show_fps != g->show_fps;
        case ITEM_NES_ASPECT:      return p->nes_aspect != g->nes_aspect;
        case ITEM_MASTER_VOLUME:   return p->master_volume != g->master_volume;
        case ITEM_MUSIC_VOLUME:    return p->bgm_volume != g->bgm_volume;
        case ITEM_SFX_VOLUME:      return p->sfx_volume != g->sfx_volume;
        case ITEM_VOICE_VOLUME:    return p->voice_volume != g->voice_volume;
        case ITEM_SWAP_AB:         return p->swap_ab_xy != g->swap_ab_xy;
        case ITEM_DPAD_STICK:      return p->dpad_as_stick != g->dpad_as_stick;
        case ITEM_LEFT_DEADZONE:   return p->left_deadzone != g->left_deadzone;
        case ITEM_RIGHT_DEADZONE:  return p->right_deadzone != g->right_deadzone;
        case ITEM_ZOOM:            return p->zoom_enabled != g->zoom_enabled;
        case ITEM_ENDLESS_ACRES:   return p->borderless_acres != g->borderless_acres;
        case ITEM_BELLS_TO_WALLET: return p->bells_to_wallet != g->bells_to_wallet;
        case ITEM_SHOP_UPGRADE:    return p->shop_no_visitor != g->shop_no_visitor;
        case ITEM_RESETTI:         return p->disable_resetti != g->disable_resetti;
        case ITEM_SHADOWS:         return p->shadow_quality != g->shadow_quality;
        case ITEM_PARTICLES:       return p->particle_quality != g->particle_quality;
        case ITEM_ACRE_DRAW:       return p->reduce_acre_draw != g->reduce_acre_draw;
        case ITEM_CULLING:         return p->frustum_cull != g->frustum_cull;
        case ITEM_LOGGING:         return p->verbose != g->verbose;
    }
    return 0;
}

static int cur_item_count(void) { return s_tabs[s_tab].count; }
static int idx_apply(void)      { return cur_item_count(); }
static int idx_back(void)       { return cur_item_count() + 1; }

/* Commit the pending edits, with the side effects each setting needs (the
 * same ones the old overlay menu ran on change). */
static void apply_pending(void) {
    static const int fps_hz[7] = { 60, 50, 40, 30, 20, 0, 60 };
    int fps_changed, swap_changed, window_changed;

    if (!s_dirty) return;
    fps_changed = s_pending.fps_target != g_pc_settings.fps_target;
    swap_changed = s_pending.swap_ab_xy != g_pc_settings.swap_ab_xy;
    window_changed = s_pending.render_scale != g_pc_settings.render_scale;

    g_pc_settings = s_pending;

    if (swap_changed) {
        /* the bindings carry the swap, so the mapping list reflects it */
        PCInputCode t;
        t = g_pc_keybindings.a; g_pc_keybindings.a = g_pc_keybindings.b; g_pc_keybindings.b = t;
        t = g_pc_keybindings.x; g_pc_keybindings.x = g_pc_keybindings.y; g_pc_keybindings.y = t;
        pc_keybindings_save();
    }
    if (fps_changed) {
        int v = g_pc_settings.fps_target < 0 || g_pc_settings.fps_target > 6 ? 6 : g_pc_settings.fps_target;
        g_pc_fps_target = fps_hz[v];
        g_pc_no_framelimit = v == 5;
        pc_dynamic_fps_reset();
    }
    g_pc_scale_mode = g_pc_settings.scale_mode;

    /* pc_settings_apply also re-sets fullscreen and the window size, which
     * can flicker on some firmwares: only for a new render scale. */
    if (window_changed) pc_settings_apply();
    pc_settings_apply_borderless_acres();
    pc_settings_save();
    s_dirty = 0;
    printf("[SETTINGS] applied\n");
}

/* ======================================================================
 * API (pc_settings_menu.h)
 * ====================================================================== */

void pc_settings_menu_enter(void) {
    s_pending = g_pc_settings;
    s_dirty = 0;
    s_tab = 0;
    s_sel = -1;
    s_sub = SUB_SETTINGS;
    s_active = 1;
    s_logged_room = 0;
}

int pc_settings_menu_active(void) { return s_active; }

int pc_settings_menu_nav_up(void) {
    if (!s_active) return 0;
    if (s_sub != SUB_SETTINGS) return 1;
    if (s_sel == 0) s_sel = -1;
    else if (s_sel > 0) s_sel--;
    return 1;
}

int pc_settings_menu_nav_down(void) {
    if (!s_active) return 0;
    if (s_sub != SUB_SETTINGS) return 1;
    if (s_sel == -1) s_sel = 0;
    else if (s_sel < idx_back()) s_sel++;
    return 1;
}

static int nav_horizontal(int dir) {
    if (!s_active) return 0;
    if (s_sub == SUB_CONFIRM_BACK) {
        s_back_sel ^= 1;
        return 1;
    }
    if (s_sel == -1) {
        if (dir < 0 && s_tab > 0) s_tab--;
        else if (dir > 0 && s_tab < TAB_COUNT - 1) s_tab++;
    } else if (s_sel < cur_item_count()) {
        item_cycle(s_tabs[s_tab].items[s_sel].id, dir);
    } else if (s_sel == idx_apply() && dir > 0) { /* Apply and Back sit side by side */
        s_sel = idx_back();
    } else if (s_sel == idx_back() && dir < 0) {
        s_sel = idx_apply();
    }
    return 1;
}

int pc_settings_menu_nav_left(void)  { return nav_horizontal(-1); }
int pc_settings_menu_nav_right(void) { return nav_horizontal(+1); }

int pc_settings_menu_confirm(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_CONFIRM_BACK) {
        if (s_back_sel == 1) { /* Discard */
            s_active = 0;
            return 0;
        }
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_sel == -1) {
        s_sel = 0;
    } else if (s_sel < cur_item_count()) {
        item_cycle(s_tabs[s_tab].items[s_sel].id, +1);
    } else if (s_sel == idx_apply()) {
        apply_pending();
    } else if (s_sel == idx_back()) {
        if (s_dirty) {
            s_sub = SUB_CONFIRM_BACK;
            s_back_sel = 0;
            return 1;
        }
        s_active = 0;
        return 0;
    }
    return 1;
}

int pc_settings_menu_cancel(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_CONFIRM_BACK) {
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_dirty) {
        s_sub = SUB_CONFIRM_BACK;
        s_back_sel = 0;
        return 1;
    }
    s_active = 0;
    return 0;
}

/* ======================================================================
 * Drawing kit: shapes and text in the font display list, 320x240 font space
 * ====================================================================== */

extern void mFont_gppSetMode(Gfx** gfx_pp); /* m_font.c */

typedef struct { u8 r, g, b, a; } Col;

/* the game's paper, wood and leaf colours */
static const Col k_backdrop  = {  34,  22,  10, 150 };
static const Col k_shadow    = {  40,  24,   8, 105 };
static const Col k_paper_top = { 255, 251, 236, 255 };
static const Col k_paper_bot = { 247, 236, 206, 255 };
static const Col k_wood      = { 150, 102,  58, 255 };
static const Col k_rule      = { 230, 214, 178, 255 };
static const Col k_ink       = {  84,  56,  30, 255 };
static const Col k_ink_soft  = { 128,  98,  66, 255 };
static const Col k_ink_faint = { 178, 154, 118, 255 };
static const Col k_cream     = { 255, 250, 232, 255 };
static const Col k_leaf_top  = { 128, 194,  82, 255 };
static const Col k_leaf_bot  = {  88, 154,  52, 255 };
static const Col k_leaf_edge = {  58, 110,  38, 255 };
static const Col k_tan_top   = { 240, 226, 192, 255 };
static const Col k_tan_bot   = { 226, 208, 168, 255 };
static const Col k_tan_edge  = { 194, 168, 124, 255 };
static const Col k_hl_top    = { 255, 242, 166, 255 };
static const Col k_hl_bot    = { 255, 216, 116, 255 };
static const Col k_hl_edge   = { 228, 150,  52, 255 };
static const Col k_changed   = { 212,  82,  34, 255 }; /* a value not applied yet */

/* Shapes collect into one vertex load (the 5-bit indices reach 32) and go
 * out as one triangle list when it fills or before any text. */
#define UI_MAX_V 32
#define UI_MAX_T 48
#define UI_ARC   4 /* segments per rounded corner */

static struct {
    struct game_s* game;
    int nv, nt;
    s16 x[UI_MAX_V], y[UI_MAX_V];
    Col c[UI_MAX_V];
    u8  t[UI_MAX_T][3];
    unsigned anim; /* frames drawn: the cursor arrows bob */
} s_ui;

/* untextured, vertex colour, blended, no Z: undone by the next text draw
 * (mFont_gppSetMode) and at the end of the page */
static Gfx s_shape_mode[] = {
    gsDPPipeSync(),
    gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_OFF),
    gsSPClearGeometryMode(G_ZBUFFER | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
                          G_TEXTURE_GEN_LINEAR | G_LOD),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPEndDisplayList(),
};

/* gSPNTriangles 5-bit packets, as pc_text_draw.c builds them: the first
 * carries 3 triangles, each next one 4 */
static void ui_trin(Gfx** pp, int n, const u8 (*t)[3]) {
    Gfx* g = *pp;
    int v[12], k, j;
    for (j = 0; j < 9; j++) v[j] = j / 3 < n ? t[j / 3][j % 3] : 0;
    g->words.w1 = ((unsigned)(v[0] & 0x1F) << 4) | ((unsigned)(v[1] & 0x1F) << 9) |
                  ((unsigned)(v[2] & 0x1F) << 14) | ((unsigned)(v[3] & 0x1F) << 19) |
                  ((unsigned)(v[4] & 0x1F) << 24) | ((unsigned)(v[5] & 0x07) << 29);
    g->words.w0 = ((unsigned)((v[5] >> 3) & 0x03) << 0) | ((unsigned)(v[6] & 0x1F) << 2) |
                  ((unsigned)(v[7] & 0x1F) << 7) | ((unsigned)(v[8] & 0x1F) << 12) |
                  ((unsigned)((n - 1) & 0x7F) << 17) | ((unsigned)G_TRIN_INDEPEND << 24);
    g++;
    for (k = 3; k < n; k += 4) {
        for (j = 0; j < 12; j++) v[j] = k + j / 3 < n ? t[k + j / 3][j % 3] : 0;
        g->words.w1 = ((unsigned)(v[0] & 0x1F) << 4) | ((unsigned)(v[1] & 0x1F) << 9) |
                      ((unsigned)(v[2] & 0x1F) << 14) | ((unsigned)(v[3] & 0x1F) << 19) |
                      ((unsigned)(v[4] & 0x1F) << 24) | ((unsigned)(v[5] & 0x07) << 29);
        g->words.w0 = ((unsigned)((v[5] >> 3) & 0x03) << 0) | ((unsigned)(v[6] & 0x1F) << 2) |
                      ((unsigned)(v[7] & 0x1F) << 7) | ((unsigned)(v[8] & 0x1F) << 12) |
                      ((unsigned)(v[9] & 0x1F) << 17) | ((unsigned)(v[10] & 0x1F) << 22) |
                      ((unsigned)(v[11] & 0x1F) << 27);
        g++;
    }
    *pp = g;
}

static void ui_flush(void) {
    int nv = s_ui.nv, nt = s_ui.nt, i;
    GRAPH* graph;
    Vtx* vtx;
    Gfx* gfx;
    s_ui.nv = s_ui.nt = 0;
    if (nv == 0 || nt == 0 || !s_ui.game) return;
    graph = s_ui.game->graph;
    vtx = GRAPH_ALLOC_TYPE(graph, Vtx, nv);
    for (i = 0; i < nv; i++) {
        Vtx_t* v = &vtx[i].v;
        v->ob[0] = s_ui.x[i];
        v->ob[1] = s_ui.y[i];
        v->ob[2] = 0;
        v->flag = 1; /* as the font's vertices: no shared matrix */
        v->tc[0] = v->tc[1] = 0;
        v->cn[0] = s_ui.c[i].r;
        v->cn[1] = s_ui.c[i].g;
        v->cn[2] = s_ui.c[i].b;
        v->cn[3] = s_ui.c[i].a;
    }
    OPEN_DISP(graph);
    gfx = NOW_FONT_DISP;
    gSPDisplayList(gfx++, s_shape_mode);
    gSPVertex(gfx++, vtx, nv, 0);
    ui_trin(&gfx, nt, (const u8 (*)[3])s_ui.t);
    SET_FONT_DISP(gfx);
    CLOSE_DISP(graph);
}

static void ui_room(int nv, int nt) {
    if (s_ui.nv + nv > UI_MAX_V || s_ui.nt + nt > UI_MAX_T) ui_flush();
}

/* a vertex at font-space x,y (the font's ortho: centred, y up, 16 units a pixel) */
static int ui_v(f32 x, f32 y, Col c) {
    int i = s_ui.nv++;
    s_ui.x[i] = (s16)floorf((x - SCREEN_WIDTH_F * 0.5f) * mFont_SCALE_F + 0.5f);
    s_ui.y[i] = (s16)floorf((SCREEN_HEIGHT_F * 0.5f - y) * mFont_SCALE_F + 0.5f);
    s_ui.c[i] = c;
    return i;
}

static void ui_t(int a, int b, int c) {
    u8* t = s_ui.t[s_ui.nt++];
    t[0] = (u8)a;
    t[1] = (u8)b;
    t[2] = (u8)c;
}

static Col col_mix(Col a, Col b, f32 t) {
    Col c;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    c.r = (u8)(a.r + (b.r - a.r) * t);
    c.g = (u8)(a.g + (b.g - a.g) * t);
    c.b = (u8)(a.b + (b.b - a.b) * t);
    c.a = (u8)(a.a + (b.a - a.a) * t);
    return c;
}

static void ui_quad(f32 x0, f32 y0, f32 x1, f32 y1, Col top, Col bot) {
    int a, b, c, d;
    ui_room(4, 2);
    a = ui_v(x0, y0, top);
    b = ui_v(x1, y0, top);
    c = ui_v(x1, y1, bot);
    d = ui_v(x0, y1, bot);
    ui_t(a, b, c);
    ui_t(a, c, d);
}

static void ui_tri(f32 ax, f32 ay, f32 bx, f32 by, f32 cx, f32 cy, Col col) {
    int a, b, c;
    ui_room(3, 1);
    a = ui_v(ax, ay, col);
    b = ui_v(bx, by, col);
    c = ui_v(cx, cy, col);
    ui_t(a, b, c);
}

/* a rounded rectangle, top colour to bottom colour: a fan around its centre */
static void ui_rrect(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, Col top, Col bot) {
    static const f32 k_cos[UI_ARC + 1] = { 1.0f, 0.92388f, 0.70711f, 0.38268f, 0.0f };
    const int n = 4 * (UI_ARC + 1);
    f32 w = x1 - x0, h = y1 - y0;
    int center, first, q, k;
    if (w <= 0.0f || h <= 0.0f) return;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    if (r < 0.5f) {
        ui_quad(x0, y0, x1, y1, top, bot);
        return;
    }
    ui_room(n + 1, n);
    center = ui_v((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, col_mix(top, bot, 0.5f));
    first = s_ui.nv;
    for (q = 0; q < 4; q++)
        for (k = 0; k <= UI_ARC; k++) {
            f32 c = k_cos[k] * r, s = k_cos[UI_ARC - k] * r, px, py;
            switch (q) {
                case 0:  px = x0 + r - c; py = y0 + r - s; break; /* top left */
                case 1:  px = x1 - r + s; py = y0 + r - c; break; /* top right */
                case 2:  px = x1 - r + c; py = y1 - r + s; break; /* bottom right */
                default: px = x0 + r - s; py = y1 - r + c; break; /* bottom left */
            }
            ui_v(px, py, col_mix(top, bot, (py - y0) / h));
        }
    for (k = 0; k < n; k++) ui_t(center, first + k, first + (k + 1) % n);
}

/* a rounded box with an edge e wide */
static void ui_box(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, f32 e, Col edge, Col top, Col bot) {
    ui_rrect(x0, y0, x1, y1, r, edge, edge);
    ui_rrect(x0 + e, y0 + e, x1 - e, y1 - e, r > e ? r - e : 0.0f, top, bot);
}

/* a cursor arrow pointing left (dir < 0) or right, tip at x */
static void ui_arrow(f32 x, f32 cy, int dir, Col c) {
    f32 b = dir < 0 ? x + 6.0f : x - 6.0f;
    ui_tri(x, cy, b, cy - 5.0f, b, cy + 5.0f, c);
}

/* 0..2 units and back over 32 frames */
static f32 ui_bob(void) {
    unsigned t = s_ui.anim & 31;
    return (f32)(t < 16 ? t : 32 - t) * (2.0f / 16.0f);
}

/* the whole screen, widescreen included */
static void ui_backdrop(GRAPH* graph, Col c) {
    Gfx* gfx;
    OPEN_DISP(graph);
    gfx = NOW_FONT_DISP;
    gDPNoOpTag(gfx++, PC_NOOP_WIDESCREEN_STRETCH);
    gDPPipeSync(gfx++);
    gDPSetOtherMode(gfx++,
        G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT |
        G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP |
        G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
        G_AC_NONE | G_ZS_PRIM | G_RM_XLU_SURF | G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, c.r, c.g, c.b, c.a);
    gfx = gfx_gSPTextureRectangle1(gfx, 0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gDPNoOpTag(gfx++, PC_NOOP_WIDESCREEN_STRETCH_OFF);
    SET_FONT_DISP(gfx);
    CLOSE_DISP(graph);
}

/* ---- text: the game font, cells 16 units tall ---- */

static f32 ui_text_w(const char* s, f32 scale) { return (f32)pc_text_width(s) * scale; }

static void ui_text(const char* s, f32 x, f32 y, Col c, f32 scale) {
    ui_flush(); /* shapes drawn so far go under the text */
    pc_text_draw(s_ui.game, s, x, y, c.r, c.g, c.b, c.a, scale);
}

static void ui_text_c(const char* s, f32 cx, f32 y, Col c, f32 scale) {
    ui_text(s, cx - ui_text_w(s, scale) * 0.5f, y, c, scale);
}

/* centred, shrunk to fit max_w (not below 0.75), on the same middle line */
static void ui_text_fit(const char* s, f32 cx, f32 y, Col c, f32 max_w) {
    f32 w = ui_text_w(s, 1.0f), sc = 1.0f;
    if (w > max_w && w > 0.0f) {
        sc = max_w / w;
        if (sc < 0.75f) sc = 0.75f;
    }
    ui_text_c(s, cx, y + (1.0f - sc) * 8.0f, c, sc);
}

/* ---- the game's window parts ---- */

#define SHEET_X0 18.0f
#define SHEET_X1 302.0f
#define SHEET_CX 160.0f
#define INNER_W  256.0f /* text room inside the sheet */

/* a sheet of paper in a wood frame, with a soft shadow */
static void ui_sheet(f32 x0, f32 y0, f32 x1, f32 y1) {
    ui_rrect(x0 + 3.0f, y0 + 4.0f, x1 + 3.0f, y1 + 4.0f, 14.0f, k_shadow, k_shadow);
    ui_box(x0, y0, x1, y1, 14.0f, 3.0f, k_wood, k_paper_top, k_paper_bot);
}

/* the green name tag over a sheet's top edge, as on the game's speech bubbles */
static void ui_name_tag(const char* s, f32 x, f32 y) {
    f32 w = ui_text_w(s, 1.0f) + 26.0f;
    ui_rrect(x + 2.0f, y + 3.0f, x + w + 2.0f, y + 22.0f, 9.5f, k_shadow, k_shadow);
    ui_box(x, y, x + w, y + 19.0f, 9.5f, 2.0f, k_leaf_edge, k_leaf_top, k_leaf_bot);
    ui_text(s, x + 13.0f, y + 1.0f, k_cream, 1.0f);
}

/* a ruled line across the sheet */
static void ui_rule(f32 y) {
    ui_quad(30.0f, y, SHEET_X1 - 12.0f, y + 1.0f, k_rule, k_rule);
}

enum { BTN_PLAIN, BTN_GO, BTN_OFF };

/* a pill button, 18 tall; sel: the cursor is on it */
static void ui_button(const char* s, f32 cx, f32 y, f32 w, int sel, int style) {
    f32 x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, y1 = y + 18.0f;
    Col text = style == BTN_GO ? k_cream : style == BTN_OFF ? k_ink_faint : k_ink;
    if (sel) ui_rrect(x0 - 2.5f, y - 2.5f, x1 + 2.5f, y1 + 2.5f, 11.5f, k_hl_edge, k_hl_edge);
    if (style == BTN_GO)
        ui_box(x0, y, x1, y1, 9.0f, 2.0f, k_leaf_edge, k_leaf_top, k_leaf_bot);
    else if (sel)
        ui_box(x0, y, x1, y1, 9.0f, 2.0f, k_hl_edge, k_hl_top, k_hl_bot);
    else
        ui_box(x0, y, x1, y1, 9.0f, 2.0f, k_tan_edge, k_tan_top, k_tan_bot);
    if (sel && style == BTN_OFF) text = k_ink_soft;
    ui_text_c(s, cx, y + 0.5f, text, 1.0f);
}

/* two buttons side by side under the middle; sel: 0 left, 1 right, else none */
static void ui_button_pair(const char* l, int lstyle, const char* r, int rstyle, int sel, f32 y) {
    const f32 gap = 14.0f;
    f32 lw = ui_text_w(l, 1.0f) + 24.0f, rw = ui_text_w(r, 1.0f) + 24.0f, x;
    if (lw < 66.0f) lw = 66.0f;
    if (rw < 66.0f) rw = 66.0f;
    x = SHEET_CX - (lw + gap + rw) * 0.5f;
    ui_button(l, x + lw * 0.5f, y, lw, sel == 0, lstyle);
    ui_button(r, x + lw + gap + rw * 0.5f, y, rw, sel == 1, rstyle);
}

/* the selected row's band */
static void ui_row_band(f32 y, f32 h) {
    ui_box(26.0f, y, SHEET_X1 - 8.0f, y + h, h * 0.5f, 1.5f, k_hl_edge, k_hl_top, k_hl_bot);
}

/* ======================================================================
 * Pages
 * ====================================================================== */

#define TAG_X     30.0f
#define TAG_Y     15.0f
#define SHEET_Y0  26.0f
#define SHEET_Y1  224.0f
#define TABS_Y    40.0f
#define ROWS_Y    64.0f
#define ROW_H     17.0f
#define ROW_SLOTS 5 /* rows on the sheet: the longest tab must fit */
#define FITS(a) (sizeof(a) / sizeof((a)[0]) <= ROW_SLOTS)
_Static_assert(FITS(tab_video_items) && FITS(tab_audio_items) && FITS(tab_input_items) &&
               FITS(tab_game_items) && FITS(tab_perf_items), "a tab has more rows than the sheet");
#undef FITS
#define LABEL_X  38.0f
#define VALUE_CX 236.0f
#define BTN_Y    156.0f
#define FOOT_Y   179.0f /* rule; help line under it, status line under that */

/* what the selected row does, for the help line */
static const char* item_help(int id) {
    switch (id) {
        case ITEM_FPS_TARGET:      return "Dynamic keeps the game at full speed";
        case ITEM_RENDER_SCALE:    return "Lower draws fewer pixels: faster, softer";
        case ITEM_SCALE_MODE:      return "How a lower render scale fills the screen";
        case ITEM_FPS_COUNTER:     return "Frames per second, in a screen corner";
        case ITEM_NES_ASPECT:      return "NES games: 4:3 with bars, or stretched";
        case ITEM_MASTER_VOLUME:   return "Everything at once";
        case ITEM_MUSIC_VOLUME:    return "Background music";
        case ITEM_SFX_VOLUME:      return "Footsteps, tools and the rest";
        case ITEM_VOICE_VOLUME:    return "Animalese";
        case ITEM_SWAP_AB:         return "Swaps A with B, and X with Y";
        case ITEM_DPAD_STICK:      return "Walk with the D-pad. Auto: no stick found";
        case ITEM_LEFT_DEADZONE:   return "Raise it if you walk without touching it";
        case ITEM_RIGHT_DEADZONE:  return "The right stick's dead zone";
        case ITEM_ZOOM:            return "Hold L1 and press Up or Down to zoom";
        case ITEM_ENDLESS_ACRES:   return "Walk on between acres without a stop";
        case ITEM_BELLS_TO_WALLET: return "Bell bags you pick up go to your wallet";
        case ITEM_SHOP_UPGRADE:    return "No visitor: Nookington's without one";
        case ITEM_RESETTI:         return "Mr. Resetti visits if you skip saving";
        case ITEM_SHADOWS:         return "Fewer shadows draw faster";
        case ITEM_PARTICLES:       return "Rain, snow and leaves";
        case ITEM_ACRE_DRAW:
            if (s_pending.borderless_acres && s_pending.reduce_acre_draw == 2)
                return "Endless acres draws at least Cross";
            return "How many neighbouring acres draw";
        case ITEM_CULLING:         return "Skip things that are off screen";
        case ITEM_LOGGING:         return "Verbose fills log.txt, a little slower";
    }
    return "";
}

static void draw_tab_row(void) {
    const f32 gap = 5.0f, pad = 14.0f, h = 18.0f;
    f32 widths[TAB_COUNT], total = 0.0f, x, x_start;
    int t, focused = s_sel == -1;
    for (t = 0; t < TAB_COUNT; t++) {
        widths[t] = ui_text_w(s_tabs[t].name, 1.0f) + pad;
        total += widths[t];
    }
    total += gap * (TAB_COUNT - 1);
    x = x_start = SHEET_CX - total * 0.5f;
    for (t = 0; t < TAB_COUNT; t++) {
        f32 x1 = x + widths[t];
        if (t == s_tab) {
            if (focused) ui_rrect(x - 2.5f, TABS_Y - 2.5f, x1 + 2.5f, TABS_Y + h + 2.5f, h * 0.5f + 2.5f,
                                  k_hl_edge, k_hl_edge);
            ui_box(x, TABS_Y, x1, TABS_Y + h, h * 0.5f, 2.0f, k_leaf_edge, k_leaf_top, k_leaf_bot);
        } else {
            ui_box(x, TABS_Y, x1, TABS_Y + h, h * 0.5f, 2.0f, k_tan_edge, k_tan_top, k_tan_bot);
        }
        x = x1 + gap;
    }
    if (focused) {
        f32 bob = ui_bob();
        if (s_tab > 0) ui_arrow(x_start - 10.0f - bob, TABS_Y + h * 0.5f, -1, k_wood);
        if (s_tab < TAB_COUNT - 1) ui_arrow(x_start + total + 10.0f + bob, TABS_Y + h * 0.5f, +1, k_wood);
    }
    x = x_start;
    for (t = 0; t < TAB_COUNT; t++) {
        ui_text_c(s_tabs[t].name, x + widths[t] * 0.5f, TABS_Y + 0.5f, t == s_tab ? k_cream : k_ink_soft, 1.0f);
        x += widths[t] + gap;
    }
}

static void draw_settings_page(void) {
    const Tab* tab = &s_tabs[s_tab];
    int i, sel_btn;
    const char* help = "";
    const char* status;

    ui_sheet(SHEET_X0, SHEET_Y0, SHEET_X1, SHEET_Y1);
    ui_name_tag("Options", TAG_X, TAG_Y);
    draw_tab_row();

    for (i = 0; i <= ROW_SLOTS; i++) ui_rule(ROWS_Y + i * ROW_H - 1.0f);
    ui_rule(FOOT_Y);

    for (i = 0; i < tab->count && i < ROW_SLOTS; i++) {
        const Item* it = &tab->items[i];
        int selected = s_sel == i, changed = item_changed(it->id);
        f32 y = ROWS_Y + i * ROW_H, vw;
        char value[32];
        item_format(it->id, value, sizeof value);
        vw = ui_text_w(value, 1.0f);
        if (selected) {
            f32 half = (vw < 44.0f ? 22.0f : vw * 0.5f) + 13.0f, bob = ui_bob();
            ui_row_band(y, ROW_H - 1.0f);
            ui_arrow(VALUE_CX - half - bob, y + 8.0f, -1, k_wood);
            ui_arrow(VALUE_CX + half + bob, y + 8.0f, +1, k_wood);
            help = item_help(it->id);
        }
        if (changed) ui_rrect(29.0f, y + 5.5f, 34.0f, y + 10.5f, 2.5f, k_changed, k_changed);
        ui_text(it->label, LABEL_X, y - 0.5f, selected ? k_ink : k_ink_soft, 1.0f);
        ui_text(value, VALUE_CX - vw * 0.5f, y - 0.5f, changed ? k_changed : selected ? k_ink : k_ink_soft, 1.0f);
    }

    sel_btn = s_sel == idx_apply() ? 0 : s_sel == idx_back() ? 1 : -1;
    ui_button_pair("Apply", s_dirty ? BTN_GO : BTN_OFF, "Back", BTN_PLAIN, sel_btn, BTN_Y);

    /* help line: the selected row; status line: the buttons */
    if (s_sel == -1) help = "Left and Right turn the pages";
    else if (sel_btn == 0) help = s_dirty ? "Keep the changes marked in orange" : "Nothing changed yet";
    else if (sel_btn == 1) help = s_dirty ? "Leave: you can still keep the changes" : "Close Options";

    status = s_sel == -1 ? "A: open the page     B: back" :
             sel_btn >= 0 ? "A: choose     B: back" : "Left, Right: change     B: back";
    ui_text_fit(help, SHEET_CX, FOOT_Y + 3.0f, k_ink_soft, INNER_W);
    ui_text_fit(status, SHEET_CX, FOOT_Y + 20.0f, k_ink_faint, INNER_W);
}

/* a speech bubble for the yes/no question, its tail at the bottom left */
static void draw_bubble(const char* title, f32 y0, f32 y1) {
    const f32 x0 = 36.0f, x1 = 284.0f;
    ui_rrect(x0 + 3.0f, y0 + 4.0f, x1 + 3.0f, y1 + 4.0f, 16.0f, k_shadow, k_shadow);
    ui_tri(88.0f, y1 - 6.0f, 116.0f, y1 - 6.0f, 80.0f, y1 + 17.0f, k_wood);
    ui_box(x0, y0, x1, y1, 16.0f, 3.0f, k_wood, k_paper_top, k_paper_bot);
    ui_tri(91.5f, y1 - 6.0f, 112.0f, y1 - 6.0f, 83.5f, y1 + 11.5f, k_paper_bot);
    ui_name_tag(title, x0 + 12.0f, y0 - 11.0f);
}

static void draw_back_confirm_page(void) {
    draw_bubble("Discard changes?", 76.0f, 168.0f);
    ui_text_c("You have unapplied changes.", SHEET_CX, 100.0f, k_ink, 1.0f);
    ui_button_pair("Keep editing", BTN_PLAIN, "Discard", BTN_PLAIN, s_back_sel, 134.0f);
}

void pc_settings_menu_draw(struct game_s* game, int with_dim_backdrop) {
    GRAPH* graph;
    Gfx* gfx;
    if (!s_active || !game || !game->graph) return;
    graph = game->graph;
    s_ui.game = game;
    s_ui.nv = s_ui.nt = 0;
    s_ui.anim++;
    if (with_dim_backdrop) {
        Col c = k_backdrop;
        if (s_sub == SUB_CONFIRM_BACK) c.a = 190; /* the question stands out */
        ui_backdrop(graph, c);
    }
    if (s_sub == SUB_CONFIRM_BACK) draw_back_confirm_page();
    else draw_settings_page();
    ui_flush();
    /* leave the font's mode set, as every other font draw does */
    OPEN_DISP(graph);
    gfx = NOW_FONT_DISP;
    mFont_gppSetMode(&gfx);
    SET_FONT_DISP(gfx);
    CLOSE_DISP(graph);
    if (!s_logged_room) {
        s_logged_room = 1;
        printf("[MENU] Options drawn: font list %d bytes free, poly %d\n",
               THA_GA_getFreeBytes(&graph->font_thaga), THA_GA_getFreeBytes(&graph->polygon_opaque_thaga));
    }
}
