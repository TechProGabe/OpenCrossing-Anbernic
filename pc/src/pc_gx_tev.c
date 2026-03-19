/* pc_gx_tev.c - TEV shader: GLSL program loading and uniform upload */
#include "pc_gx_internal.h"

/* --- file I/O --- */

static char* load_text_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (read != (size_t)len) {
        fprintf(stderr, "WARNING: Partial read of %s (got %zu of %ld bytes)\n", path, read, len);
        free(buf);
        return NULL;
    }
    buf[read] = '\0';
    return buf;
}

static char* load_shader(const char* filename) {
    char path[512];
    snprintf(path, sizeof(path), "shaders/%s", filename);
    char* src = load_text_file(path);
    if (src) {
        printf("[PC/TEV] Loaded shader: %s\n", path);
    } else {
        fprintf(stderr, "FATAL: Could not load shader: %s\n", path);
    }
    return src;
}

static GLuint default_program = 0;

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
#ifdef PC_USE_GLES
    /* Replace "#version 330 core" with "#version 320 es" + precision qualifiers */
    const char* version_line = "#version 320 es\n";
    const char* precision_line = (type == GL_FRAGMENT_SHADER)
        ? "precision mediump float;\nprecision mediump int;\n"
        : "precision highp float;\nprecision highp int;\n";
    const char* past_version = source;
    if (strncmp(source, "#version", 8) == 0) {
        past_version = strchr(source, '\n');
        if (past_version) past_version++;
        else past_version = source;
    }
    const char* sources[3] = { version_line, precision_line, past_version };
    glShaderSource(shader, 3, sources, NULL);
#else
    glShaderSource(shader, 1, &source, NULL);
#endif
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "FATAL: Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(GLuint vert, GLuint frag) {
    if (!vert || !frag) {
        fprintf(stderr, "FATAL: Cannot link program — shader compilation failed\n");
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_normal");
    glBindAttribLocation(prog, 2, "a_color0");
    glBindAttribLocation(prog, 3, "a_texcoord0");

    glLinkProgram(prog);

    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "FATAL: Program link error: %s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

void pc_gx_tev_init(void) {
    char* vs_src = load_shader("default.vert");
    char* fs_src = load_shader("default.frag");

    if (!vs_src || !fs_src) {
        fprintf(stderr, "FATAL: Shader files missing from shaders/ directory.\n"
                        "Expected: shaders/default.vert and shaders/default.frag\n"
                        "Make sure shader files are next to the executable.\n");
        free(vs_src);
        free(fs_src);
        exit(1);
    }

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    default_program = link_program(vs, fs);

    free(vs_src);
    free(fs_src);
}

void pc_gx_tev_shutdown(void) {
    if (default_program) {
        glDeleteProgram(default_program);
        default_program = 0;
    }

}

GLuint pc_gx_tev_get_shader(PCGXState* state) {
    return default_program;
}
