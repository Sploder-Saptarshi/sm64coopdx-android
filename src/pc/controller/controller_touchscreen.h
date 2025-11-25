#ifndef CONTROLLER_TOUCHSCREEN_H
#define CONTROLLER_TOUCHSCREEN_H
#ifdef TOUCH_CONTROLS
#include "pc/configfile.h"

#define HIDE_POS -1000

#define CHAT_BUTTON 0x001C
#define PLAYERLIST_BUTTON 0x000F
#define CONSOLE_BUTTON 0x0005

#define SCANCODE_BACK 0

#define SCREEN_WIDTH_API 1280
#define SCREEN_HEIGHT_API 960

#define LEFT_EDGE ((int)floorf(SCREEN_WIDTH_API / 2 - SCREEN_HEIGHT_API / 2 * gfx_current_dimensions.aspect_ratio))
#define RIGHT_EDGE ((int)ceilf(SCREEN_WIDTH_API / 2 + SCREEN_HEIGHT_API / 2 * gfx_current_dimensions.aspect_ratio))
#define LEFT_EDGE_FUNC(v) ((int)floorf(SCREEN_WIDTH_API / 2 - SCREEN_HEIGHT_API / 2 * gfx_current_dimensions.aspect_ratio + (v)))
#define RIGHT_EDGE_FUNC(v) ((int)ceilf(SCREEN_WIDTH_API / 2 + SCREEN_HEIGHT_API / 2 * gfx_current_dimensions.aspect_ratio - (v)))
#define RECT_FROM_LEFT_EDGE(v) ((int)floorf(LEFT_EDGE_FUNC(v)))
#define RECT_FROM_RIGHT_EDGE(v) ((int)ceilf(RIGHT_EDGE_FUNC(v)))

#define CORRECT_TOUCH_X(x) ((x * (RIGHT_EDGE - LEFT_EDGE)) + LEFT_EDGE)
#define CORRECT_TOUCH_Y(y) (y * SCREEN_HEIGHT_API)

#define TRIGGER_DETECT(size) (((pos.x + size / 2 > CORRECT_TOUCH_X(event->x)) &&\
                               (pos.x - size / 2 < CORRECT_TOUCH_X(event->x))) &&\
                              ((pos.y + size / 2 > CORRECT_TOUCH_Y(event->y)) &&\
                               (pos.y - size / 2 < CORRECT_TOUCH_Y(event->y))))

enum ConfigControlElementAnchor {
    CONTROL_ELEMENT_LEFT,
    CONTROL_ELEMENT_RIGHT,
    CONTROL_ELEMENT_CENTER,
    CONTROL_ELEMENT_HIDDEN,
};

enum ConfigControlElementIndex {
    TOUCH_STICK,
    TOUCH_MOUSE,
    TOUCH_A,
    TOUCH_B,
    TOUCH_X,
    TOUCH_Y,
    TOUCH_START,
    TOUCH_L,
    TOUCH_R,
    TOUCH_Z,
    TOUCH_CUP,
    TOUCH_CDOWN,
    TOUCH_CLEFT,
    TOUCH_CRIGHT,
    TOUCH_CHAT,
    TOUCH_PLAYERLIST,
    TOUCH_DUP,
    TOUCH_DDOWN,
    TOUCH_DLEFT,
    TOUCH_DRIGHT,
    TOUCH_CONSOLE,
    TOUCH_COUNT,
};

typedef struct {
    u32 x, y, size;
    enum ConfigControlElementAnchor anchor;
    u32 r, g, b, a;
} ConfigControlElement;

extern ConfigControlElement configControlElements[];
extern ConfigControlElement configControlElementsLast[];

extern struct ControllerAPI controller_touchscreen;
extern s16 touch_x;
extern s16 touch_y;

extern bool gInTouchConfig, gGamepadActive;
extern enum ConfigControlElementIndex gSelectedTouchElement;

struct TouchEvent {
    // Note to VDavid003: In Xorg, touchID became large!
    // SurfaceFlinger SDL2 only populated this with 1-255. 
    // But X11 SDL2 populated this with 699!
    // For X11 compatibility, I matched the types.
    SDL_TouchID touchID; //Should not be 0
    f32 x, y; //Should be from 0 to 1
};

struct Position {
    s32 x, y;
};

typedef struct {
    u8 r, g, b, a;
} Colors;

enum ControlElementType {
    Joystick,
    Mouse,
    Button
};

struct ButtonState {
    u8 buttonUp;
    u8 buttonDown;
};

struct ControlElement {
    enum ControlElementType type;
    SDL_TouchID touchID; //0 = not being touched, 1+ = Finger being used
    //Joystick
    s32 joyX, joyY;
    //Button
    s32 buttonID;
    struct ButtonState buttonTexture;
    s32 slideTouch;
};

void touch_down(struct TouchEvent* event);
void touch_motion(struct TouchEvent* event);
void touch_up(struct TouchEvent* event);

void render_touch_controls(void);

#endif
#endif
