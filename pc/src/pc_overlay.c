/* pc_overlay.c - FPS counter + in-game settings menu with embedded bitmap font */
#include "pc_platform.h"
#include "pc_overlay.h"
#include "pc_settings.h"

/* ---- Vertex type ---- */
typedef struct { float x, y, u, v, r, g, b, a; } OvVtx;
#define OV_MAX_VERTS 3072
static OvVtx ov_verts[OV_MAX_VERTS]; /* static to avoid 96KB on stack */

/* ---- GL objects ---- */
static GLuint ov_prog = 0;
static GLuint ov_vao = 0;
static GLuint ov_vbo = 0;
static GLuint ov_tex = 0;
static GLint  ov_loc_font = -1;

/* ---- FPS state ---- */
static double ov_fps   = 60.0;
static double ov_speed = 100.0;

/* ---- Menu state ---- */
int g_pc_menu_open = 0;
static int menu_cursor = 0;

/* Input repeat: initial delay then fast repeat */
static int rep_up = 0, rep_down = 0, rep_left = 0, rep_right = 0;
#define REP_DELAY  16  /* frames before repeat starts */
#define REP_RATE    3  /* frames between repeats */

/* ---- Menu items ---- */
enum {
    MI_FPS_COUNTER,
    MI_MASTER_VOLUME,
    MI_BGM_VOLUME,
    MI_SFX_VOLUME,
    MI_VOICE_VOLUME,
    MI_FULLSCREEN,
    MI_VSYNC,
    MI_MSAA,
    MI_ZOOM_ENABLED,
    MI_FPS_TARGET,
    MI_RENDER_SCALE,
    MI_WINDOW_SIZE,
    MI_SCALE_MODE,
    MI_VERBOSE,
    MI_COUNT
};

static const char* menu_labels[MI_COUNT] = {
    "FPS Counter",
    "Master Volume",
    "Music",
    "Sound Effects",
    "Voices",
    "Fullscreen",
    "V-Sync",
    "MSAA",
    "Camera Zoom",
    "FPS Target",
    "Render Scale",
    "Render Res",
    "Scale Mode",
    "Verbose Log",
};

/* ---- Menu tabs ---- */
enum { TAB_VIDEO, TAB_AUDIO, TAB_DEBUG, TAB_COUNT };
static const char* tab_labels[TAB_COUNT] = { "VIDEO", "AUDIO", "DEBUG" };
static int s_active_tab = 0;

/* Which tab each menu item belongs to (indexed by MI_*) */
static const int menu_item_tab[MI_COUNT] = {
    /* MI_FPS_COUNTER  */ TAB_DEBUG,
    /* MI_MASTER_VOLUME*/ TAB_AUDIO,
    /* MI_BGM_VOLUME   */ TAB_AUDIO,
    /* MI_SFX_VOLUME   */ TAB_AUDIO,
    /* MI_VOICE_VOLUME */ TAB_AUDIO,
    /* MI_FULLSCREEN   */ TAB_VIDEO,
    /* MI_VSYNC        */ TAB_VIDEO,
    /* MI_MSAA         */ TAB_VIDEO,
    /* MI_ZOOM_ENABLED */ TAB_VIDEO,
    /* MI_FPS_TARGET   */ TAB_VIDEO,
    /* MI_RENDER_SCALE */ TAB_VIDEO,
    /* MI_WINDOW_SIZE  */ TAB_VIDEO,
    /* MI_SCALE_MODE   */ TAB_VIDEO,
    /* MI_VERBOSE      */ TAB_DEBUG,
};

/* Returns index of nth visible item in the active tab, or -1 if out of range.
 * Also used to count visible items: count = tab_item_count(). */
static int tab_item_at(int n) {
    int found = 0;
    for (int i = 0; i < MI_COUNT; i++) {
        if (menu_item_tab[i] == s_active_tab) {
            if (found == n) return i;
            found++;
        }
    }
    return -1;
}
static int tab_item_count(void) {
    int count = 0;
    for (int i = 0; i < MI_COUNT; i++)
        if (menu_item_tab[i] == s_active_tab) count++;
    return count;
}

static void menu_get_value(int item, char* buf, int sz) {
    switch (item) {
    case MI_FPS_COUNTER:   snprintf(buf, sz, "%s", g_pc_settings.show_fps ? "ON" : "OFF"); break;
    case MI_MASTER_VOLUME: snprintf(buf, sz, "%d%%", g_pc_settings.master_volume); break;
    case MI_BGM_VOLUME:    snprintf(buf, sz, "%d%%", g_pc_settings.bgm_volume); break;
    case MI_SFX_VOLUME:    snprintf(buf, sz, "%d%%", g_pc_settings.sfx_volume); break;
    case MI_VOICE_VOLUME:  snprintf(buf, sz, "%d%%", g_pc_settings.voice_volume); break;
    case MI_FULLSCREEN: {
        const char* m[] = {"Windowed", "Fullscreen", "Borderless"};
        snprintf(buf, sz, "%s", m[g_pc_settings.fullscreen]);
        break;
    }
    case MI_VSYNC:         snprintf(buf, sz, "%s", g_pc_settings.vsync ? "ON" : "OFF"); break;
    case MI_MSAA:
        if (g_pc_settings.msaa == 0) snprintf(buf, sz, "Off");
        else snprintf(buf, sz, "%dx", g_pc_settings.msaa);
        break;
    case MI_ZOOM_ENABLED:  snprintf(buf, sz, "%s", g_pc_settings.zoom_enabled ? "ON" : "OFF"); break;
    case MI_FPS_TARGET: {
        static const char* names[] = {"60 FPS", "50 FPS", "40 FPS", "30 FPS", "20 FPS", "Unlimited", "Auto"};
        int t = g_pc_settings.fps_target;
        if (t < 0 || t > 6) t = 0;
        snprintf(buf, sz, "%s", names[t]);
        break;
    }
    case MI_RENDER_SCALE:
        snprintf(buf, sz, "%d%%", g_pc_settings.render_scale);
        break;
    case MI_WINDOW_SIZE: {
        static const char* wsnames[] = {"320x240", "480x360", "640x480", "960x720", "1280x960", "Custom"};
        int w = g_pc_settings.window_size;
        if (w < 0 || w > 5) w = 2;
        snprintf(buf, sz, "%s", wsnames[w]);
        break;
    }
    case MI_SCALE_MODE:
        snprintf(buf, sz, "%s", g_pc_settings.scale_mode == 1 ? "Center" : "Stretch");
        break;
    case MI_VERBOSE:       snprintf(buf, sz, "%s", g_pc_settings.verbose ? "ON" : "OFF"); break;
    }
}

static void menu_adjust_vol(int* vol, int dir) {
    int v = *vol + dir * 5;
    if (v < 0) v = 0; if (v > 100) v = 100;
    *vol = v;
}

static void menu_adjust(int item, int dir) {
    switch (item) {
    case MI_FPS_COUNTER:   g_pc_settings.show_fps ^= 1; break;
    case MI_MASTER_VOLUME: menu_adjust_vol(&g_pc_settings.master_volume, dir); break;
    case MI_BGM_VOLUME:    menu_adjust_vol(&g_pc_settings.bgm_volume, dir); break;
    case MI_SFX_VOLUME:    menu_adjust_vol(&g_pc_settings.sfx_volume, dir); break;
    case MI_VOICE_VOLUME:  menu_adjust_vol(&g_pc_settings.voice_volume, dir); break;
    case MI_FULLSCREEN: {
        int v = g_pc_settings.fullscreen + dir;
        if (v < 0) v = 2; if (v > 2) v = 0;
        g_pc_settings.fullscreen = v;
        pc_settings_apply();
        break;
    }
    case MI_VSYNC:
        g_pc_settings.vsync ^= 1;
        pc_settings_apply();
        break;
    case MI_MSAA: {
        int vals[] = {0, 2, 4, 8};
        int cur = 0;
        for (int i = 0; i < 4; i++) if (vals[i] == g_pc_settings.msaa) cur = i;
        cur += dir;
        if (cur < 0) cur = 3; if (cur > 3) cur = 0;
        g_pc_settings.msaa = vals[cur];
        break; /* MSAA needs restart */
    }
    case MI_ZOOM_ENABLED:  g_pc_settings.zoom_enabled ^= 1; break;
    case MI_FPS_TARGET: {
        /* Right = faster (lower index), Left = slower (higher index) */
        int v = g_pc_settings.fps_target - dir;
        if (v < 0) v = 6; if (v > 6) v = 0;
        g_pc_settings.fps_target = v;
        static const int fps_hz[7] = {60, 50, 40, 30, 20, 0, 60};
        g_pc_fps_target = fps_hz[v];
        if (v == 5) g_pc_no_framelimit = 1;
        else        g_pc_no_framelimit = 0;
        break;
    }
    case MI_RENDER_SCALE: {
        static const int scales[] = {25, 50, 75, 100};
        int cur = 3; /* default to 100% */
        for (int i = 0; i < 4; i++) if (scales[i] == g_pc_settings.render_scale) { cur = i; break; }
        cur += dir;
        if (cur < 0) cur = 3; if (cur > 3) cur = 0;
        g_pc_settings.render_scale = scales[cur];
        pc_settings_apply();
        break;
    }
    case MI_WINDOW_SIZE: {
        int v = g_pc_settings.window_size + dir;
        if (v < 0) v = 4; if (v > 4) v = 0; /* skip Custom (5) from menu cycling */
        g_pc_settings.window_size = v;
        pc_settings_apply();
        break;
    }
    case MI_SCALE_MODE:
        g_pc_settings.scale_mode ^= 1;
        g_pc_scale_mode = g_pc_settings.scale_mode;
        break;
    case MI_VERBOSE:       g_pc_settings.verbose ^= 1; break;
    }
}

/* ---- Embedded shaders ---- */
#ifdef PC_USE_GLES
static const char* ov_vert_src =
    "#version 310 es\n"
    "precision mediump float;\n"
    "precision highp int;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "out vec2 v_uv; out vec4 v_col;\n"
    "void main(){gl_Position=vec4(a_pos,0.0,1.0);v_uv=a_uv;v_col=a_col;}\n";
static const char* ov_frag_src =
    "#version 310 es\n"
    "precision mediump float;\n"
    "precision highp int;\n"
    "in vec2 v_uv; in vec4 v_col;\n"
    "uniform sampler2D u_font;\n"
    "out vec4 fragColor;\n"
    "void main(){float a=texture(u_font,v_uv).r;fragColor=vec4(v_col.rgb,v_col.a*a);}\n";
#else
static const char* ov_vert_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "out vec2 v_uv; out vec4 v_col;\n"
    "void main(){gl_Position=vec4(a_pos,0.0,1.0);v_uv=a_uv;v_col=a_col;}\n";
static const char* ov_frag_src =
    "#version 330 core\n"
    "in vec2 v_uv; in vec4 v_col;\n"
    "uniform sampler2D u_font;\n"
    "out vec4 fragColor;\n"
    "void main(){float a=texture(u_font,v_uv).r;fragColor=vec4(v_col.rgb,v_col.a*a);}\n";
#endif

/* ---- 8x8 bitmap font (ASCII 32-127 = 96 glyphs, IBM PC BIOS style) ---- */
static const unsigned char font_8x8[96][8] = {
    /* 32 ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 33 '!' */ {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    /* 34 '"' */ {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00},
    /* 35 '#' */ {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00},
    /* 36 '$' */ {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00},
    /* 37 '%' */ {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00},
    /* 38 '&' */ {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00},
    /* 39 '\''*/ {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    /* 40 '(' */ {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},
    /* 41 ')' */ {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
    /* 42 '*' */ {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    /* 43 '+' */ {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    /* 44 ',' */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    /* 45 '-' */ {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    /* 46 '.' */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    /* 47 '/' */ {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00},
    /* 48 '0' */ {0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0x7C,0x00},
    /* 49 '1' */ {0x18,0x38,0x78,0x18,0x18,0x18,0x7E,0x00},
    /* 50 '2' */ {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00},
    /* 51 '3' */ {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00},
    /* 52 '4' */ {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00},
    /* 53 '5' */ {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00},
    /* 54 '6' */ {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00},
    /* 55 '7' */ {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00},
    /* 56 '8' */ {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00},
    /* 57 '9' */ {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00},
    /* 58 ':' */ {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    /* 59 ';' */ {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    /* 60 '<' */ {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    /* 61 '=' */ {0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00},
    /* 62 '>' */ {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00},
    /* 63 '?' */ {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00},
    /* 64 '@' */ {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00},
    /* 65 'A' */ {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00},
    /* 66 'B' */ {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00},
    /* 67 'C' */ {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00},
    /* 68 'D' */ {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00},
    /* 69 'E' */ {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00},
    /* 70 'F' */ {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00},
    /* 71 'G' */ {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00},
    /* 72 'H' */ {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00},
    /* 73 'I' */ {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    /* 74 'J' */ {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00},
    /* 75 'K' */ {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00},
    /* 76 'L' */ {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00},
    /* 77 'M' */ {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00},
    /* 78 'N' */ {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00},
    /* 79 'O' */ {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    /* 80 'P' */ {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    /* 81 'Q' */ {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06},
    /* 82 'R' */ {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00},
    /* 83 'S' */ {0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00},
    /* 84 'T' */ {0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00},
    /* 85 'U' */ {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    /* 86 'V' */ {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00},
    /* 87 'W' */ {0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00},
    /* 88 'X' */ {0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0x00},
    /* 89 'Y' */ {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00},
    /* 90 'Z' */ {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00},
    /* 91 '[' */ {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},
    /* 92 '\\'*/ {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00},
    /* 93 ']' */ {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00},
    /* 94 '^' */ {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    /* 95 '_' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    /* 96 '`' */ {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00},
    /* 97 'a' */ {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00},
    /* 98 'b' */ {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00},
    /* 99 'c' */ {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00},
    /*100 'd' */ {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00},
    /*101 'e' */ {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00},
    /*102 'f' */ {0x1C,0x36,0x30,0x78,0x30,0x30,0x78,0x00},
    /*103 'g' */ {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x78},
    /*104 'h' */ {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00},
    /*105 'i' */ {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00},
    /*106 'j' */ {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C},
    /*107 'k' */ {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00},
    /*108 'l' */ {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    /*109 'm' */ {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00},
    /*110 'n' */ {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00},
    /*111 'o' */ {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00},
    /*112 'p' */ {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0},
    /*113 'q' */ {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E},
    /*114 'r' */ {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00},
    /*115 's' */ {0x00,0x00,0x7C,0xC0,0x7C,0x06,0xFC,0x00},
    /*116 't' */ {0x30,0x30,0x7C,0x30,0x30,0x36,0x1C,0x00},
    /*117 'u' */ {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00},
    /*118 'v' */ {0x00,0x00,0xC6,0xC6,0x6C,0x38,0x10,0x00},
    /*119 'w' */ {0x00,0x00,0xC6,0xD6,0xD6,0xFE,0x6C,0x00},
    /*120 'x' */ {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00},
    /*121 'y' */ {0x00,0x00,0xC6,0xC6,0xCE,0x76,0x06,0x7C},
    /*122 'z' */ {0x00,0x00,0xFC,0x98,0x30,0x64,0xFC,0x00},
    /*123 '{' */ {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00},
    /*124 '|' */ {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    /*125 '}' */ {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00},
    /*126 '~' */ {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00},
    /*127 solid*/{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
};

/* ---- Shader compile helper ---- */
static GLuint ov_compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "[OVERLAY] shader error: %s\n", log);
    }
    return s;
}

/* ---- Vertex helpers ---- */
static int ov_nv; /* current vertex count */

static void ov_push(float px, float py, float u, float vt,
                    float r, float g, float b, float a) {
    if (ov_nv >= OV_MAX_VERTS) return;
    OvVtx* p = &ov_verts[ov_nv++];
    p->x = px / (float)g_pc_window_w * 2.0f - 1.0f;
    p->y = 1.0f - py / (float)g_pc_window_h * 2.0f;
    p->u = u; p->v = vt;
    p->r = r; p->g = g; p->b = b; p->a = a;
}

static void ov_quad(float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1,
                    float r, float g, float b, float a) {
    ov_push(x,     y,     u0, v0, r, g, b, a);
    ov_push(x,     y + h, u0, v1, r, g, b, a);
    ov_push(x + w, y,     u1, v0, r, g, b, a);
    ov_push(x + w, y,     u1, v0, r, g, b, a);
    ov_push(x,     y + h, u0, v1, r, g, b, a);
    ov_push(x + w, y + h, u1, v1, r, g, b, a);
}

/* Solid block UV (char 127 = all white) for backgrounds */
#define BG_U ((15.0f + 0.5f) / 16.0f)
#define BG_V ((5.0f + 0.5f) / 6.0f)

static void ov_char(int ch, float x, float y, float cw, float ch_h,
                    float r, float g, float b, float a) {
    int idx = ch - 32;
    if (idx < 0 || idx > 95) idx = 0;
    float col = (float)(idx % 16);
    float row = (float)(idx / 16);
    ov_quad(x, y, cw, ch_h,
            col / 16.0f, row / 6.0f,
            (col + 1.0f) / 16.0f, (row + 1.0f) / 6.0f,
            r, g, b, a);
}

static void ov_string(const char* s, float x, float y, float cw, float ch,
                      float r, float g, float b, float a) {
    for (int i = 0; s[i]; i++)
        ov_char((unsigned char)s[i], x + i * cw, y, cw, ch, r, g, b, a);
}

/* Right-aligned string ending at x_right */
static void ov_string_right(const char* s, float x_right, float y, float cw, float ch,
                            float r, float g, float b, float a) {
    int len = (int)strlen(s);
    ov_string(s, x_right - len * cw, y, cw, ch, r, g, b, a);
}

/* ---- Input repeat helper ---- */
static int ov_repeat(int held, int* timer) {
    if (!held) { *timer = 0; return 0; }
    if (*timer == 0) { *timer = 1; return 1; } /* first press */
    (*timer)++;
    if (*timer == REP_DELAY) { return 1; }
    if (*timer > REP_DELAY && ((*timer - REP_DELAY) % REP_RATE) == 0) { return 1; }
    return 0;
}

/* ---- Menu input (called from draw, polls SDL state) ---- */
static void menu_process_input(void) {
    if (!g_pc_menu_open) return;

    const Uint8* keys = SDL_GetKeyboardState(NULL);
    int up    = keys[SDL_SCANCODE_UP];
    int down  = keys[SDL_SCANCODE_DOWN];
    int left  = keys[SDL_SCANCODE_LEFT];
    int right = keys[SDL_SCANCODE_RIGHT];
    int tab_l = keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_PAGEUP];
    int tab_r = keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_PAGEDOWN];

    SDL_GameController* ctrl = pc_pad_get_controller();
    if (ctrl) {
        up    |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP);
        down  |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        left  |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        right |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        tab_l |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        tab_r |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    }

    /* Tab switching */
    static int rep_tabl = 0, rep_tabr = 0;
    if (ov_repeat(tab_l, &rep_tabl)) {
        s_active_tab--;
        if (s_active_tab < 0) s_active_tab = TAB_COUNT - 1;
        menu_cursor = 0;
    }
    if (ov_repeat(tab_r, &rep_tabr)) {
        s_active_tab++;
        if (s_active_tab >= TAB_COUNT) s_active_tab = 0;
        menu_cursor = 0;
    }

    int count = tab_item_count();
    if (ov_repeat(up,    &rep_up))    { menu_cursor--; if (menu_cursor < 0) menu_cursor = count - 1; }
    if (ov_repeat(down,  &rep_down))  { menu_cursor++; if (menu_cursor >= count) menu_cursor = 0; }

    int mi = tab_item_at(menu_cursor);
    if (mi >= 0) {
        if (ov_repeat(left,  &rep_left))  menu_adjust(mi, -1);
        if (ov_repeat(right, &rep_right)) menu_adjust(mi, +1);
    }
}

/* ---- Draw: FPS counter (top-right corner) ---- */
static void draw_fps(float cw, float ch, float pad) {
    char line1[32], line2[32];
    if (g_pc_fps_target > 0)
        snprintf(line1, sizeof(line1), "%.1f/%d FPS", ov_fps, g_pc_fps_target);
    else
        snprintf(line1, sizeof(line1), "%.1f FPS", ov_fps);
    snprintf(line2, sizeof(line2), "%d%% Speed", (int)(ov_speed + 0.5));

    int len1 = (int)strlen(line1);
    int len2 = (int)strlen(line2);
    int max_len = len1 > len2 ? len1 : len2;

    float pw = max_len * cw + 2.0f * pad;
    float ph = 2.0f * ch + 3.0f * pad;
    float px = (float)g_pc_window_w - pw - pad;
    float py = pad;

    ov_quad(px, py, pw, ph, BG_U, BG_V, BG_U, BG_V, 0, 0, 0, 0.65f);
    ov_string(line1, px + pad, py + pad, cw, ch, 1, 1, 1, 1);
    ov_string(line2, px + pad, py + pad + ch + pad, cw, ch, 0.8f, 0.8f, 0.8f, 1);
}

/* ---- Draw: Settings menu (centered) ---- */
static void draw_menu(float cw, float ch, float pad) {
    int tab_count = tab_item_count();
    int cols = 28;
    int val_col = 18;
    /* Title row + tab bar row + blank + items + blank + 2 footer rows */
    int lines = 1 + 1 + 1 + tab_count + 1 + 2;

    float mw = cols * cw + 2.0f * pad;
    float mh = lines * (ch + pad) + pad;
    float mx = ((float)g_pc_window_w - mw) * 0.5f;
    float my = ((float)g_pc_window_h - mh) * 0.5f;

    /* Background */
    ov_quad(mx, my, mw, mh, BG_U, BG_V, BG_U, BG_V, 0, 0, 0, 0.80f);

    float row_h = ch + pad;
    float tx = mx + pad;
    float ty = my + pad;

    /* Title */
    float title_x = mx + (mw - 8.0f * cw) * 0.5f;
    ov_string("SETTINGS", title_x, ty, cw, ch, 1, 1, 1, 1);
    ty += row_h;

    /* Tab bar — draw all tabs, highlight active one */
    {
        /* Distribute tab labels evenly across cols chars */
        int tab_col_w = cols / TAB_COUNT;
        for (int t = 0; t < TAB_COUNT; t++) {
            float tab_x = tx + t * tab_col_w * cw;
            const char* lbl = tab_labels[t];
            int lbl_len = (int)strlen(lbl);
            float lbl_x = tab_x + ((tab_col_w - lbl_len) / 2) * cw;
            if (t == s_active_tab) {
                /* Highlight active: bright white with underline block */
                ov_string(lbl, lbl_x, ty, cw, ch, 1, 1, 0.3f, 1);
                /* Underline */
                ov_quad(tab_x, ty + ch, tab_col_w * cw - pad, 2.0f,
                        BG_U, BG_V, BG_U, BG_V, 1, 1, 0.3f, 1);
            } else {
                ov_string(lbl, lbl_x, ty, cw, ch, 0.5f, 0.5f, 0.5f, 1);
            }
        }
        ty += row_h + pad; /* tab bar + blank */
    }

    /* Items for active tab */
    for (int n = 0; n < tab_count; n++) {
        int i = tab_item_at(n);
        if (i < 0) break;

        float r, g, b;
        if (n == menu_cursor) { r = 1.0f; g = 1.0f; b = 0.3f; }
        else                  { r = 0.75f; g = 0.75f; b = 0.75f; }

        if (n == menu_cursor)
            ov_string(">", tx, ty, cw, ch, r, g, b, 1);

        ov_string(menu_labels[i], tx + 2 * cw, ty, cw, ch, r, g, b, 1);

        char val[16];
        menu_get_value(i, val, sizeof(val));
        float vr = 1, vg = 1, vb = 1;
        if (n == menu_cursor) { vr = 1; vg = 1; vb = 0.5f; }
        ov_string_right(val, tx + cols * cw, ty, cw, ch, vr, vg, vb, 1);

        /* Volume bar */
        {
            int vol_val = -1;
            if (i == MI_MASTER_VOLUME) vol_val = g_pc_settings.master_volume;
            else if (i == MI_BGM_VOLUME)   vol_val = g_pc_settings.bgm_volume;
            else if (i == MI_SFX_VOLUME)   vol_val = g_pc_settings.sfx_volume;
            else if (i == MI_VOICE_VOLUME) vol_val = g_pc_settings.voice_volume;
            if (vol_val >= 0) {
                float bar_x = tx + (val_col - 1) * cw;
                float bar_w = (cols - val_col - 5) * cw;
                float bar_h = ch * 0.3f;
                float bar_y = ty + ch * 0.35f;
                float fill = (float)vol_val / 100.0f;
                ov_quad(bar_x, bar_y, bar_w, bar_h, BG_U, BG_V, BG_U, BG_V, 0.3f, 0.3f, 0.3f, 1);
                if (fill > 0)
                    ov_quad(bar_x, bar_y, bar_w * fill, bar_h, BG_U, BG_V, BG_U, BG_V,
                            n == menu_cursor ? 1.0f : 0.6f,
                            n == menu_cursor ? 1.0f : 0.6f,
                            n == menu_cursor ? 0.3f : 0.6f, 1);
            }
        }

        ty += row_h;
    }

    /* Footer */
    ty += pad;
    ov_string("L/R:Tab  U/D:Nav  L/R:Adj", tx, ty, cw, ch, 0.5f, 0.5f, 0.5f, 1);
    ty += row_h;
    ov_string("Select/Tab:Close+Save", tx, ty, cw, ch, 0.5f, 0.5f, 0.5f, 1);
}

/* ---- Public API ---- */

void pc_overlay_init(void) {
    /* Save GL state that we'll disturb — pc_gx keeps its VAO/VBO bound at all times */
    GLint prev_vao, prev_vbo, prev_tex;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);

    GLuint vs = ov_compile_shader(GL_VERTEX_SHADER, ov_vert_src);
    GLuint fs = ov_compile_shader(GL_FRAGMENT_SHADER, ov_frag_src);
    ov_prog = glCreateProgram();
    glAttachShader(ov_prog, vs);
    glAttachShader(ov_prog, fs);
    glLinkProgram(ov_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(ov_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetProgramInfoLog(ov_prog, sizeof(log), NULL, log);
        fprintf(stderr, "[OVERLAY] link error: %s\n", log);
    }
    ov_loc_font = glGetUniformLocation(ov_prog, "u_font");

    /* Font texture: 128x48 R8, 16x6 grid of 8x8 chars */
    unsigned char pixels[128 * 48];
    memset(pixels, 0, sizeof(pixels));
    for (int i = 0; i < 96; i++) {
        int col = i % 16, row = i / 16;
        for (int y = 0; y < 8; y++) {
            unsigned char bits = font_8x8[i][y];
            for (int x = 0; x < 8; x++)
                if (bits & (0x80 >> x))
                    pixels[(row * 8 + y) * 128 + col * 8 + x] = 0xFF;
        }
    }
    glGenTextures(1, &ov_tex);
    glBindTexture(GL_TEXTURE_2D, ov_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 128, 48, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* VAO/VBO */
    glGenVertexArrays(1, &ov_vao);
    glGenBuffers(1, &ov_vbo);
    glBindVertexArray(ov_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ov_vbo);
    glBufferData(GL_ARRAY_BUFFER, OV_MAX_VERTS * sizeof(OvVtx), NULL, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(OvVtx), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(OvVtx), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(OvVtx), (void*)(4 * sizeof(float)));

    /* Restore previous GL state */
    glBindVertexArray(prev_vao);
    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glBindTexture(GL_TEXTURE_2D, prev_tex);
}

void pc_overlay_shutdown(void) {
    if (ov_tex)  { glDeleteTextures(1, &ov_tex);       ov_tex = 0; }
    if (ov_vbo)  { glDeleteBuffers(1, &ov_vbo);        ov_vbo = 0; }
    if (ov_vao)  { glDeleteVertexArrays(1, &ov_vao);   ov_vao = 0; }
    if (ov_prog) { glDeleteProgram(ov_prog);            ov_prog = 0; }
}

void pc_overlay_update(double fps, double speed) {
    ov_fps = fps;
    ov_speed = speed;
}

void pc_overlay_menu_toggle(void) {
    g_pc_menu_open ^= 1;
    if (!g_pc_menu_open) {
        /* Save settings on close */
        pc_settings_save();
    }
    /* Reset repeat timers */
    rep_up = rep_down = rep_left = rep_right = 0;
}

void pc_overlay_draw(void) {
    if (!ov_prog) return;

    /* Nothing to draw? */
    int want_fps = g_pc_settings.show_fps && !g_pc_menu_open;
    if (!want_fps && !g_pc_menu_open) return;

    /* Process menu input */
    if (g_pc_menu_open) menu_process_input();

    /* ---- Save GL state ---- */
    GLint sv_prog, sv_vao, sv_vbo, sv_tex, sv_active_tex, sv_vp[4], sv_bsrc, sv_bdst;
    glGetIntegerv(GL_CURRENT_PROGRAM, &sv_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &sv_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &sv_vbo);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &sv_active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &sv_tex);
    glGetIntegerv(GL_VIEWPORT, sv_vp);
    glGetIntegerv(GL_BLEND_SRC_RGB, &sv_bsrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &sv_bdst);
    GLboolean sv_blend   = glIsEnabled(GL_BLEND);
    GLboolean sv_depth   = glIsEnabled(GL_DEPTH_TEST);
    GLboolean sv_scissor = glIsEnabled(GL_SCISSOR_TEST);

    /* ---- Set overlay GL state ---- */
    glUseProgram(ov_prog);
    glBindVertexArray(ov_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ov_tex);
    glUniform1i(ov_loc_font, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, g_pc_window_w, g_pc_window_h);

    /* ---- Build vertices ---- */
    int scale = g_pc_window_h / 240;
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;
    float cw  = 8.0f * scale;
    float ch  = 8.0f * scale;
    float pad = 4.0f * scale;

    ov_nv = 0;

    if (g_pc_menu_open) {
        draw_menu(cw, ch, pad);
    } else if (want_fps) {
        draw_fps(cw, ch, pad);
    }

    /* ---- Upload & draw ---- */
    if (ov_nv > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, ov_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(ov_nv * sizeof(OvVtx)), ov_verts);
        glDrawArrays(GL_TRIANGLES, 0, ov_nv);
    }

    /* ---- Restore GL state ---- */
    glUseProgram(sv_prog);
    glBindVertexArray(sv_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sv_vbo);
    glActiveTexture(sv_active_tex);
    glBindTexture(GL_TEXTURE_2D, sv_tex);
    glViewport(sv_vp[0], sv_vp[1], sv_vp[2], sv_vp[3]);
    glBlendFunc(sv_bsrc, sv_bdst);
    if (!sv_blend)   glDisable(GL_BLEND);
    if (sv_depth)    glEnable(GL_DEPTH_TEST);
    if (sv_scissor)  glEnable(GL_SCISSOR_TEST);
}
