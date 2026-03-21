/* pc_vi.c - video interface → SDL window swap + frame pacing */
#include "pc_platform.h"

/* GL timing and frame reset from pc_gx.c */
extern void pc_gx_frame_timing_snapshot(void);
extern void pc_gx_begin_frame(void);
extern Uint64 pc_gx_flush_time_us;
extern Uint64 pc_gx_texload_time_us;

#define VI_TVMODE_NTSC_INT    0
#define VI_TVMODE_NTSC_DS     1
#define VI_TVMODE_PAL_INT     4
#define VI_TVMODE_MPAL_INT    8
#define VI_TVMODE_EURGB60_INT 20

static u32 retrace_count = 0;
u32 pc_frame_counter = 0;
static Uint64 frame_start_time = 0;
static Uint64 perf_freq = 0;
static void (*vi_pre_callback)(u32) = NULL;
static void (*vi_post_callback)(u32) = NULL;

void VIInit(void) { }

void VIConfigure(void* rm) { (void)rm; }

void VISetNextFrameBuffer(void* fb) { (void)fb; }

void VIFlush(void) {}

void VIWaitForRetrace(void) {
    if (!perf_freq) perf_freq = SDL_GetPerformanceFrequency();

    /* --- frame time diagnostic --- */
    Uint64 vi_enter = SDL_GetPerformanceCounter();
    double frame_ms = 0.0;
    if (frame_start_time) {
        frame_ms = (double)(vi_enter - frame_start_time) * 1000.0 / (double)perf_freq;
    }

    if (!pc_platform_poll_events()) {
        g_pc_running = 0;
        return;
    }

    Uint64 t_before_swap = SDL_GetPerformanceCounter();
    pc_platform_swap_buffers();
    Uint64 t_after_swap = SDL_GetPerformanceCounter();

    Uint64 t_before_pace = SDL_GetPerformanceCounter();
    if (!g_pc_no_framelimit) {
        /* Timer-based pacing: sleep until 16ms per frame (~60 FPS).
         * Audio production runs on a dedicated thread and is no longer
         * tied to game frame timing. */
        if (frame_start_time) {
            Uint64 now = SDL_GetPerformanceCounter();
            Uint64 elapsed_us = (now - frame_start_time) * 1000000 / perf_freq;
            /* 16667us = 60.0 Hz (NTSC). Spin for sub-ms precision. */
            while (elapsed_us < 16667) {
                Uint64 remain_us = 16667 - elapsed_us;
                if (remain_us > 2000) {
                    SDL_Delay(1);
                }
                now = SDL_GetPerformanceCounter();
                elapsed_us = (now - frame_start_time) * 1000000 / perf_freq;
            }
        }
    }
    Uint64 t_after_pace = SDL_GetPerformanceCounter();

    /* Snapshot GL timing before reporting */
    pc_gx_frame_timing_snapshot();

    /* report slow frames (>20ms = missed 60fps by >4ms) */
    if (frame_ms > 20.0 && g_pc_verbose) {
        double swap_ms = (double)(t_after_swap - t_before_swap) * 1000.0 / (double)perf_freq;
        double pace_ms = (double)(t_after_pace - t_before_pace) * 1000.0 / (double)perf_freq;
        double work_ms = (double)(vi_enter - frame_start_time) * 1000.0 / (double)perf_freq;
        double flush_ms = (double)pc_gx_flush_time_us / 1000.0;
        double texld_ms = (double)pc_gx_texload_time_us / 1000.0;
        printf("[STUTTER] frame %u: total=%.1fms work=%.1fms swap=%.1fms pace=%.1fms gl=%.1fms tex=%.1fms draws=%d\n",
               pc_frame_counter, frame_ms, work_ms, swap_ms, pace_ms,
               flush_ms, texld_ms, pc_gx_draw_call_count);
    }

    {
        static Uint64 fps_start = 0;
        static int fps_count = 0;
        if (fps_start == 0) fps_start = SDL_GetPerformanceCounter();
        fps_count++;
        if (fps_count >= 60) {
            Uint64 now = SDL_GetPerformanceCounter();
            double secs = (double)(now - fps_start) / (double)perf_freq;
            double fps = (double)fps_count / secs;
            char title[96];
            snprintf(title, sizeof(title), "Animal Crossing - %.1f FPS (%d draws)", fps, pc_gx_draw_call_count);
            SDL_SetWindowTitle(g_pc_window, title);
            if (g_pc_verbose) {
                extern int pc_emu64_frame_cmds, pc_emu64_frame_crashes;
                double flush_ms = (double)pc_gx_flush_time_us / 1000.0;
                double texld_ms = (double)pc_gx_texload_time_us / 1000.0;
                printf("[PERF] %.1f FPS | draws=%d cmds=%d crashes=%d gl=%.1fms tex=%.1fms\n",
                       fps, pc_gx_draw_call_count, pc_emu64_frame_cmds, pc_emu64_frame_crashes,
                       flush_ms, texld_ms);
            }
            fps_start = now;
            fps_count = 0;
        }
    }

    frame_start_time = SDL_GetPerformanceCounter();

    /* Reset per-frame counters and GL state for next frame */
    pc_gx_begin_frame();

    retrace_count++;
    pc_frame_counter++;
}

u32 VIGetRetraceCount(void) { return retrace_count; }

void VISetBlack(BOOL black) { (void)black; }

u32 VIGetTvFormat(void) { return 0; /* VI_NTSC */ }
u32 VIGetDTVStatus(void) { return 0; }

void* VISetPreRetraceCallback(void* cb) {
    void* old = (void*)vi_pre_callback;
    vi_pre_callback = (void (*)(u32))cb;
    return old;
}

void* VISetPostRetraceCallback(void* cb) {
    void* old = (void*)vi_post_callback;
    vi_post_callback = (void (*)(u32))cb;
    return old;
}

u32 VIGetCurrentLine(void) { return 0; }

void VISetNextXFB(void* xfb) { (void)xfb; }
