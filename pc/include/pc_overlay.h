/* pc_overlay.h - FPS/speed counter overlay */
#ifndef PC_OVERLAY_H
#define PC_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

void pc_overlay_init(void);
void pc_overlay_shutdown(void);
void pc_overlay_toggle(void);
int  pc_overlay_is_visible(void);
void pc_overlay_update(double fps, double speed);
void pc_overlay_draw(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_OVERLAY_H */
