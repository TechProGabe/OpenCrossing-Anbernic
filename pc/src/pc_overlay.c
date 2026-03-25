/* pc_overlay.c - FPS/speed counter overlay with embedded bitmap font */
#include "pc_platform.h"
#include "pc_overlay.h"

/* ---- Vertex type ---- */
typedef struct { float x, y, u, v, r, g, b, a; } OvVtx;
#define OV_MAX_VERTS 512

/* ---- GL objects & state ---- */
static int    ov_visible = 0;
static GLuint ov_prog = 0;
static GLuint ov_vao = 0;
static GLuint ov_vbo = 0;
static GLuint ov_tex = 0;
static GLint  ov_loc_font = -1;

/* ---- Cached metrics (updated from pc_vi.c) ---- */
static double ov_fps   = 60.0;
static double ov_speed = 100.0;

/* ---- Embedded shaders ---- */
#ifdef PC_USE_GLES
static const char* ov_vert_src =
    "#version 310 es\n"
    "precision mediump float;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "out vec2 v_uv; out vec4 v_col;\n"
    "void main(){gl_Position=vec4(a_pos,0.0,1.0);v_uv=a_uv;v_col=a_col;}\n";
static const char* ov_frag_src =
    "#version 310 es\n"
    "precision mediump float;\n"
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
        printf("[OVERLAY] shader error: %s\n", log);
    }
    return s;
}

/* ---- Vertex helpers ---- */
static void ov_push(OvVtx* v, int* n,
                    float px, float py, float u, float vt,
                    float r, float g, float b, float a) {
    if (*n >= OV_MAX_VERTS) return;
    OvVtx* p = &v[*n];
    p->x = px / (float)g_pc_window_w * 2.0f - 1.0f;
    p->y = 1.0f - py / (float)g_pc_window_h * 2.0f;
    p->u = u; p->v = vt;
    p->r = r; p->g = g; p->b = b; p->a = a;
    (*n)++;
}

static void ov_quad(OvVtx* v, int* n,
                    float x, float y, float w, float h,
                    float u0, float v0, float u1, float v1,
                    float r, float g, float b, float a) {
    ov_push(v, n, x,     y,     u0, v0, r, g, b, a);
    ov_push(v, n, x,     y + h, u0, v1, r, g, b, a);
    ov_push(v, n, x + w, y,     u1, v0, r, g, b, a);
    ov_push(v, n, x + w, y,     u1, v0, r, g, b, a);
    ov_push(v, n, x,     y + h, u0, v1, r, g, b, a);
    ov_push(v, n, x + w, y + h, u1, v1, r, g, b, a);
}

static void ov_char(OvVtx* v, int* n, int ch,
                    float x, float y, float cw, float ch_h,
                    float r, float g, float b, float a) {
    int idx = ch - 32;
    if (idx < 0 || idx > 95) idx = 0;
    float col = (float)(idx % 16);
    float row = (float)(idx / 16);
    ov_quad(v, n, x, y, cw, ch_h,
            col / 16.0f, row / 6.0f,
            (col + 1.0f) / 16.0f, (row + 1.0f) / 6.0f,
            r, g, b, a);
}

static void ov_string(OvVtx* v, int* n, const char* s,
                      float x, float y, float cw, float ch,
                      float r, float g, float b, float a) {
    for (int i = 0; s[i]; i++)
        ov_char(v, n, (unsigned char)s[i], x + i * cw, y, cw, ch, r, g, b, a);
}

/* ---- Public API ---- */

void pc_overlay_init(void) {
    /* Compile & link shader */
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
        printf("[OVERLAY] link error: %s\n", log);
    }
    ov_loc_font = glGetUniformLocation(ov_prog, "u_font");

    /* Create font texture: 128x48 R8, 16x6 grid of 8x8 chars */
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

    /* Create VAO/VBO */
    glGenVertexArrays(1, &ov_vao);
    glGenBuffers(1, &ov_vbo);
    glBindVertexArray(ov_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ov_vbo);
    glBufferData(GL_ARRAY_BUFFER, OV_MAX_VERTS * sizeof(OvVtx), NULL, GL_STREAM_DRAW);
    /* loc 0: vec2 pos */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(OvVtx), (void*)0);
    /* loc 1: vec2 uv */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(OvVtx), (void*)(2 * sizeof(float)));
    /* loc 2: vec4 color */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(OvVtx), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);
}

void pc_overlay_shutdown(void) {
    if (ov_tex)  { glDeleteTextures(1, &ov_tex);       ov_tex = 0; }
    if (ov_vbo)  { glDeleteBuffers(1, &ov_vbo);        ov_vbo = 0; }
    if (ov_vao)  { glDeleteVertexArrays(1, &ov_vao);   ov_vao = 0; }
    if (ov_prog) { glDeleteProgram(ov_prog);            ov_prog = 0; }
}

void pc_overlay_toggle(void) { ov_visible ^= 1; }
int  pc_overlay_is_visible(void) { return ov_visible; }

void pc_overlay_update(double fps, double speed) {
    ov_fps = fps;
    ov_speed = speed;
}

void pc_overlay_draw(void) {
    if (!ov_visible || !ov_prog) return;

    /* ---- Save GL state ---- */
    GLint sv_prog, sv_vao, sv_tex, sv_vp[4], sv_bsrc, sv_bdst;
    glGetIntegerv(GL_CURRENT_PROGRAM, &sv_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &sv_vao);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &sv_tex);
    glGetIntegerv(GL_VIEWPORT, sv_vp);
    glGetIntegerv(GL_BLEND_SRC_RGB, &sv_bsrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &sv_bdst);
    GLboolean sv_blend   = glIsEnabled(GL_BLEND);
    GLboolean sv_depth   = glIsEnabled(GL_DEPTH_TEST);
    GLboolean sv_scissor = glIsEnabled(GL_SCISSOR_TEST);

    /* ---- Set overlay state ---- */
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

    /* ---- Layout ---- */
    int scale = g_pc_window_h / 240;
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;
    float cw  = 8.0f * scale;
    float ch  = 8.0f * scale;
    float pad = 4.0f * scale;

    char line1[32], line2[32];
    snprintf(line1, sizeof(line1), "%.1f FPS", ov_fps);
    snprintf(line2, sizeof(line2), "%d%% Speed", (int)(ov_speed + 0.5));

    int len1 = (int)strlen(line1);
    int len2 = (int)strlen(line2);
    int max_len = len1 > len2 ? len1 : len2;

    float pw = max_len * cw + 2.0f * pad;
    float ph = 2.0f * ch + 3.0f * pad;
    float px = (float)g_pc_window_w - pw - pad;
    float py = pad;

    /* ---- Build vertices ---- */
    OvVtx verts[OV_MAX_VERTS];
    int nv = 0;

    /* Background panel (solid block char 127 -> all-white UV region) */
    float su = (15.0f + 0.5f) / 16.0f;
    float sv_uv = (5.0f + 0.5f) / 6.0f;
    ov_quad(verts, &nv, px, py, pw, ph,
            su, sv_uv, su, sv_uv,
            0.0f, 0.0f, 0.0f, 0.65f);

    /* Line 1: FPS (white) */
    ov_string(verts, &nv, line1,
              px + pad, py + pad, cw, ch,
              1.0f, 1.0f, 1.0f, 1.0f);

    /* Line 2: Speed (slightly dimmer) */
    ov_string(verts, &nv, line2,
              px + pad, py + pad + ch + pad, cw, ch,
              0.8f, 0.8f, 0.8f, 1.0f);

    /* ---- Upload & draw ---- */
    glBindBuffer(GL_ARRAY_BUFFER, ov_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(nv * sizeof(OvVtx)), verts);
    glDrawArrays(GL_TRIANGLES, 0, nv);

    /* ---- Restore GL state ---- */
    glUseProgram(sv_prog);
    glBindVertexArray(sv_vao);
    glBindTexture(GL_TEXTURE_2D, sv_tex);
    glViewport(sv_vp[0], sv_vp[1], sv_vp[2], sv_vp[3]);
    glBlendFunc(sv_bsrc, sv_bdst);
    if (!sv_blend)   glDisable(GL_BLEND);
    if (sv_depth)    glEnable(GL_DEPTH_TEST);
    if (sv_scissor)  glEnable(GL_SCISSOR_TEST);
}
