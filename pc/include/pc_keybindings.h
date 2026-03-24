#ifndef PC_KEYBINDINGS_H
#define PC_KEYBINDINGS_H

#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_mouse.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PCInputCode: values 0..SDL_NUM_SCANCODES-1 are keyboard scancodes.
   Values with PC_INPUT_MOUSE_BIT set are mouse buttons (low bits = SDL button index). */
typedef int PCInputCode;

#define PC_INPUT_MOUSE_BIT   0x10000
#define PC_INPUT_MOUSE1      (PC_INPUT_MOUSE_BIT | SDL_BUTTON_LEFT)    /* left click */
#define PC_INPUT_MOUSE2      (PC_INPUT_MOUSE_BIT | SDL_BUTTON_RIGHT)   /* right click */
#define PC_INPUT_MOUSE3      (PC_INPUT_MOUSE_BIT | SDL_BUTTON_MIDDLE)  /* middle click */

typedef struct {
    /* buttons */
    PCInputCode a;
    PCInputCode b;
    PCInputCode x;
    PCInputCode y;
    PCInputCode start;
    PCInputCode z;
    PCInputCode l;
    PCInputCode r;

    /* main stick */
    PCInputCode stick_up;
    PCInputCode stick_down;
    PCInputCode stick_left;
    PCInputCode stick_right;

    /* C-stick */
    PCInputCode cstick_up;
    PCInputCode cstick_down;
    PCInputCode cstick_left;
    PCInputCode cstick_right;

    /* D-pad */
    PCInputCode dpad_up;
    PCInputCode dpad_down;
    PCInputCode dpad_left;
    PCInputCode dpad_right;
} PCKeybindings;

extern PCKeybindings g_pc_keybindings;

void pc_keybindings_load(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_KEYBINDINGS_H */
