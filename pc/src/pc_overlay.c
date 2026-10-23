/* pc_overlay.c - FPS counter, boot splash and boot error screen, drawn with an
 * embedded bitmap font. (Settings live in pc_settings_menu.c.) */
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
static GLint  ov_loc_rgba = -1;

/* ---- Boot splash logo (portmaster/ac-gc/res/splash_logo.bin) ---- */
static GLuint ov_logo_tex = 0;
static int    ov_logo_w = 0, ov_logo_h = 0;

/* ---- FPS state ---- */
static double ov_fps   = 60.0;
static double ov_speed = 100.0;

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
    "uniform int u_rgba;\n"
    "void main(){if(u_rgba==1){fragColor=texture(u_font,v_uv)*v_col;return;}"
    "float a=texture(u_font,v_uv).r;fragColor=vec4(v_col.rgb,v_col.a*a);}\n";
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
    "uniform int u_rgba;\n"
    "void main(){if(u_rgba==1){fragColor=texture(u_font,v_uv)*v_col;return;}"
    "float a=texture(u_font,v_uv).r;fragColor=vec4(v_col.rgb,v_col.a*a);}\n";
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

/* ---- Draw: FPS counter (top-right corner) ---- */
static void draw_fps(float cw, float ch, float pad) {
    char line1[32], line2[32];
    if (g_pc_fps_target > 0 && g_pc_settings.fps_target != 6)
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

/* ---- Public API ---- */

/* "OCLG", u16 w, u16 h (little endian), then RGBA8 rows: written by
 * pc/tools/make_splash_logo.py. Missing or bad file: no logo, no error. */
static void ov_load_logo(void) {
    unsigned char hdr[8];
    unsigned char* px;
    size_t n;
    FILE* f = fopen("res/splash_logo.bin", "rb");
    if (!f) return;
    if (fread(hdr, 1, 8, f) != 8 || memcmp(hdr, "OCLG", 4) != 0) {
        fclose(f);
        return;
    }
    ov_logo_w = hdr[4] | (hdr[5] << 8);
    ov_logo_h = hdr[6] | (hdr[7] << 8);
    n = (size_t)ov_logo_w * (size_t)ov_logo_h * 4;
    px = (n > 0 && n <= 4096u * 4096u) ? (unsigned char*)malloc(n) : NULL;
    if (!px || fread(px, 1, n, f) != n) {
        free(px);
        fclose(f);
        ov_logo_w = ov_logo_h = 0;
        return;
    }
    fclose(f);
    glGenTextures(1, &ov_logo_tex);
    glBindTexture(GL_TEXTURE_2D, ov_logo_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ov_logo_w, ov_logo_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(px);
}

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
    ov_loc_rgba = glGetUniformLocation(ov_prog, "u_rgba");

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

    ov_load_logo();

    /* Restore previous GL state */
    glBindVertexArray(prev_vao);
    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glBindTexture(GL_TEXTURE_2D, prev_tex);
}

void pc_overlay_shutdown(void) {
    if (ov_tex)  { glDeleteTextures(1, &ov_tex);       ov_tex = 0; }
    if (ov_logo_tex) { glDeleteTextures(1, &ov_logo_tex); ov_logo_tex = 0; }
    if (ov_vbo)  { glDeleteBuffers(1, &ov_vbo);        ov_vbo = 0; }
    if (ov_vao)  { glDeleteVertexArrays(1, &ov_vao);   ov_vao = 0; }
    if (ov_prog) { glDeleteProgram(ov_prog);            ov_prog = 0; }
}

void pc_overlay_update(double fps, double speed) {
    ov_fps = fps;
    ov_speed = speed;
}

/* A quad shaded top colour to bottom colour (on the solid font cell). */
static void ov_quad_grad(float x, float y, float w, float h,
                         const float* top, const float* bot) {
    ov_push(x,     y,     BG_U, BG_V, top[0], top[1], top[2], top[3]);
    ov_push(x,     y + h, BG_U, BG_V, bot[0], bot[1], bot[2], bot[3]);
    ov_push(x + w, y,     BG_U, BG_V, top[0], top[1], top[2], top[3]);
    ov_push(x + w, y,     BG_U, BG_V, top[0], top[1], top[2], top[3]);
    ov_push(x,     y + h, BG_U, BG_V, bot[0], bot[1], bot[2], bot[3]);
    ov_push(x + w, y + h, BG_U, BG_V, bot[0], bot[1], bot[2], bot[3]);
}

static void ov_flush(GLuint tex, int rgba) {
    if (ov_nv == 0) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(ov_loc_rgba, rgba);
    glBindBuffer(GL_ARRAY_BUFFER, ov_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(ov_nv * sizeof(OvVtx)), ov_verts);
    glDrawArrays(GL_TRIANGLES, 0, ov_nv);
    ov_nv = 0;
}

static void ov_string_centered(const char* s, float y, float cw, float ch,
                               float r, float g, float b, float a) {
    float x = ((float)g_pc_window_w - (float)strlen(s) * cw) * 0.5f;
    ov_string(s, x, y, cw, ch, r, g, b, a);
}

/* The loading screen: sky to meadow, the logo, a leaf-green progress bar
 * with what's happening under it. Called at each boot step (frac 0..1);
 * each call is one frame, and the last one stays up through the game's own
 * startup until its first frame replaces it. */
void pc_overlay_boot_progress(float frac, const char* label) {
    static const float sky_top[4]   = { 0.47f, 0.76f, 0.97f, 1.0f };
    static const float sky_bot[4]   = { 0.86f, 0.94f, 0.96f, 1.0f };
    static const float grass_top[4] = { 0.62f, 0.83f, 0.40f, 1.0f };
    static const float grass_bot[4] = { 0.40f, 0.66f, 0.27f, 1.0f };
    static const float fill_top[4]  = { 0.62f, 0.86f, 0.38f, 1.0f };
    static const float fill_bot[4]  = { 0.33f, 0.62f, 0.22f, 1.0f };
    float W, H, u, horizon, bar_w, bar_h, bar_x, bar_y, cw, ch;
    int scale;

    if (!ov_prog || !g_pc_window) return;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    SDL_PumpEvents();

    W = (float)g_pc_window_w;
    H = (float)g_pc_window_h;
    u = H / 480.0f; /* layout unit: 1 at 480 lines */

    /* Most steps run after pc_gx_init, whose VAO stays bound for good and
     * whose texture-bind cache assumes nobody else rebinds: save and
     * restore what we touch, as pc_overlay_draw does. */
    GLint sv_prog, sv_vao, sv_vbo, sv_tex, sv_active_tex, sv_fbo, sv_vp[4], sv_bsrc, sv_bdst;
    glGetIntegerv(GL_CURRENT_PROGRAM, &sv_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &sv_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &sv_vbo);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &sv_active_tex);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &sv_fbo);
    glGetIntegerv(GL_VIEWPORT, sv_vp);
    glGetIntegerv(GL_BLEND_SRC_RGB, &sv_bsrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &sv_bdst);
    GLboolean sv_blend   = glIsEnabled(GL_BLEND);
    GLboolean sv_depth   = glIsEnabled(GL_DEPTH_TEST);
    GLboolean sv_scissor = glIsEnabled(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &sv_tex);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_pc_window_w, g_pc_window_h);
    glUseProgram(ov_prog);
    glBindVertexArray(ov_vao);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(ov_loc_font, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    scale = g_pc_window_h / 240;
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;
    cw = 8.0f * scale;
    ch = 8.0f * scale;

    /* sky, a meadow along the bottom */
    ov_nv = 0;
    horizon = H * 0.80f;
    ov_quad_grad(0, 0, W, horizon, sky_top, sky_bot);
    ov_quad_grad(0, horizon, W, H - horizon, grass_top, grass_bot);
    ov_flush(ov_tex, 0);

    /* the logo, as wide as fits */
    if (ov_logo_tex) {
        float lw = W * 0.82f, lh;
        if (lw > (float)ov_logo_w * 1.4f * u) lw = (float)ov_logo_w * 1.4f * u;
        lh = lw * (float)ov_logo_h / (float)ov_logo_w;
        ov_quad((W - lw) * 0.5f, H * 0.12f, lw, lh, 0, 0, 1, 1, 1, 1, 1, 1);
        ov_flush(ov_logo_tex, 1);
    }

    /* progress bar: wood edge, cream track, leaf fill */
    bar_w = W * 0.56f;
    bar_h = 18.0f * u;
    bar_x = (W - bar_w) * 0.5f;
    bar_y = H * 0.60f;
    ov_quad(bar_x - 3 * u, bar_y - 3 * u, bar_w + 6 * u, bar_h + 6 * u,
            BG_U, BG_V, BG_U, BG_V, 0.42f, 0.29f, 0.17f, 1.0f);
    ov_quad(bar_x, bar_y, bar_w, bar_h, BG_U, BG_V, BG_U, BG_V, 1.0f, 0.97f, 0.88f, 1.0f);
    if (frac > 0.0f) ov_quad_grad(bar_x, bar_y, bar_w * frac, bar_h, fill_top, fill_bot);

    /* what's happening, and a tip on the grass */
    if (label) ov_string_centered(label, bar_y + bar_h + 12 * u, cw, ch, 0.33f, 0.22f, 0.12f, 1.0f);
    ov_string_centered("Press Select in game for Options", horizon + (H - horizon - ch) * 0.5f,
                       cw, ch, 1.0f, 0.98f, 0.90f, 1.0f);
    ov_flush(ov_tex, 0);

    glUniform1i(ov_loc_rgba, 0);
    SDL_GL_SwapWindow(g_pc_window);

    glUseProgram(sv_prog);
    glBindVertexArray(sv_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sv_vbo);
    glBindTexture(GL_TEXTURE_2D, sv_tex);
    glActiveTexture(sv_active_tex);
    glBindFramebuffer(GL_FRAMEBUFFER, sv_fbo);
    glViewport(sv_vp[0], sv_vp[1], sv_vp[2], sv_vp[3]);
    glBlendFunc(sv_bsrc, sv_bdst);
    if (!sv_blend)  glDisable(GL_BLEND);
    if (sv_depth)   glEnable(GL_DEPTH_TEST);
    if (sv_scissor) glEnable(GL_SCISSOR_TEST);
}

void pc_overlay_boot_splash(const char* msg) {
    pc_overlay_boot_progress(0.0f, msg);
}

/* One frame of the boot fatal-error panel (multi-line, centered, dark red
 * background so it can't be mistaken for the normal black boot screen).
 * Caller loops and swaps events; see pc_boot_fatal() in pc_main.c. */
void pc_overlay_boot_error_frame(const char* const* lines, int n_lines) {
    int i;

    if (!ov_prog || !g_pc_window) return;

    glViewport(0, 0, g_pc_window_w, g_pc_window_h);
    glClearColor(0.28f, 0.02f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(ov_prog);
    glBindVertexArray(ov_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ov_tex);
    glUniform1i(ov_loc_font, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    {
        int scale = g_pc_window_h / 240;
        float cw, ch, line_h, y;

        if (scale < 1) scale = 1;
        if (scale > 4) scale = 4;
        cw = 8.0f * scale;
        ch = 8.0f * scale;
        line_h = ch * 1.5f;

        ov_nv = 0;
        y = ((float)g_pc_window_h - line_h * (float)n_lines) * 0.5f;
        for (i = 0; i < n_lines; i++) {
            const char* s = lines[i];
            float x = ((float)g_pc_window_w - (float)strlen(s) * cw) * 0.5f;
            if (i == 0) {
                ov_string(s, x, y, cw, ch, 1.0f, 0.85f, 0.3f, 1.0f); /* title: yellow */
            } else {
                ov_string(s, x, y, cw, ch, 1.0f, 1.0f, 1.0f, 1.0f);
            }
            y += line_h;
        }

        if (ov_nv > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, ov_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(ov_nv * sizeof(OvVtx)), ov_verts);
            glDrawArrays(GL_TRIANGLES, 0, ov_nv);
        }
    }

    SDL_GL_SwapWindow(g_pc_window);
}

void pc_overlay_draw(void) {
    if (!ov_prog) return;

    if (!g_pc_settings.show_fps) return;

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

    draw_fps(cw, ch, pad);

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
