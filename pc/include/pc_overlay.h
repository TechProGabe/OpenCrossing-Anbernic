/* pc_overlay.h - FPS counter, boot splash and boot error screen */
#ifndef PC_OVERLAY_H
#define PC_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

void pc_overlay_init(void);
void pc_overlay_shutdown(void);
void pc_overlay_update(double fps, double speed);
void pc_overlay_draw(void);
void pc_overlay_boot_splash(const char* msg);
/* Loading screen at a boot step: progress 0..1 and what's happening. */
void pc_overlay_boot_progress(float frac, const char* label);
void pc_overlay_boot_error_frame(const char* const* lines, int n_lines);


#ifdef __cplusplus
}
#endif

#endif /* PC_OVERLAY_H */
