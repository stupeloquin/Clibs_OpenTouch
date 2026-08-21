//
// OpenTouch -> dxx-redux glue for Descent 1 & 2.
//
// Everything engine-specific lives behind dxx-redux/android/touch_input.h, so
// this file never includes Descent's own headers.
//

#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>

#include "SDL.h"
#include "SDL_keyboard.h"

#include "game_interface.h"
#include "touch_input.h"

extern "C"
{

// The engine's real entry point, renamed under Android (see main/inferno.c).
extern int dxx_main(int argc, char *argv[]);

// ---------------------------------------------------------------- lifecycle

// The engine talks through con_printf, which writes to stdout. SDL only
// redirects stdout to logcat when it runs SDL_main itself, and this port enters
// through NativeLib.init instead - so pump it across ourselves. Without this
// the engine's own diagnostics are invisible.
static void *stdio_to_logcat(void *arg)
{
    int fd = (int) (intptr_t) arg;
    char line[512];
    size_t used = 0;

    for (;;)
    {
        char c;
        const ssize_t n = read(fd, &c, 1);

        if (n <= 0)
            break;

        if (c == '\n' || used == sizeof(line) - 1)
        {
            line[used] = 0;
            if (used)
                __android_log_write(ANDROID_LOG_INFO, "DxxEngine", line);
            used = 0;
        }
        else
            line[used++] = c;
    }

    return nullptr;
}

static void start_stdio_redirect(void)
{
    int fds[2];

    if (pipe(fds) != 0)
        return;

    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);

    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    pthread_t t;
    if (pthread_create(&t, nullptr, stdio_to_logcat, (void *) (intptr_t) fds[0]) == 0)
        pthread_detach(t);
}

// DXX-Rebirth picks its home directory from D1X_REBIRTH_HOME / D2X_REBIRTH_HOME,
// falling back to ~/.d{1,2}x-rebirth/. On Android that fallback lands inside the
// game folder, which may be read-only or SAF-backed, so point it at the same
// per-engine user_files directory the library is built to use. dxx-redux
// ignores these variables.
static void set_engine_home_directory(void)
{
    const char *user_files = getenv("USER_FILES");

    if (!user_files)
        return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", user_files, DXX_ANDROID_USER_DIR);

    setenv("D1X_REBIRTH_HOME", path, 1);
    setenv("D2X_REBIRTH_HOME", path, 1);
}

void PortableInit(int argc, const char **argv)
{
    start_stdio_redirect();
    set_engine_home_directory();

    dxx_main(argc, (char **) argv);

    // dxx_main only returns when the player quits.
    exit(0);
}

// ------------------------------------------------------------------- input

// TouchJoy reports deflection as (anchor - finger) in *normalised screen
// fractions*, clamped to +/-1 - so reaching 1.0 would mean dragging a whole
// screen width. A comfortable thumb throw is nearer 0.1. leftStick()/
// rightStick() in touch_interface_base already multiply that by the user's
// sensitivity slider and a fixed factor (15 forward, 10 strafe/yaw, 2 pitch),
// which lands roughly in the 0..1 range we want - so take their output as the
// rate directly and only add the gain needed to even the axes up.
//
// Vertical fractions are ~2.2x larger than horizontal ones for the same finger
// distance on a 21:9 screen, which together with the base's 10-vs-2 factors
// leaves pitch about 2.3x weaker than yaw; hence the larger pitch gain. Full
// rate lands at roughly 160px of thumb travel on either axis.
#define GAIN_FWD            1.0f
#define GAIN_SIDE           1.5f
#define GAIN_LOOK_YAW       1.5f
#define GAIN_LOOK_PITCH     3.5f

void PortableMoveFwd(float fwd)
{
#ifdef DXX_TOUCH_DEBUG
    LOGI("axis fwd  = %f", fwd);
#endif
    dxx_touch_axis_forward(fwd * GAIN_FWD);
}

void PortableMoveSide(float strafe)
{
#ifdef DXX_TOUCH_DEBUG
    LOGI("axis side = %f", strafe);
#endif
    dxx_touch_axis_sideways(strafe * GAIN_SIDE);
}

void PortableMove(float fwd, float strafe)
{
    PortableMoveFwd(fwd);
    PortableMoveSide(strafe);
}

void PortableMoveVert(float vert)
{
#ifdef DXX_TOUCH_DEBUG
    LOGI("axis vert = %f", vert);
#endif
    dxx_touch_axis_vertical(vert);
}

void PortableRoll(float roll)
{
#ifdef DXX_TOUCH_DEBUG
    LOGI("axis roll = %f", roll);
#endif
    dxx_touch_axis_bank(roll, 0);
}

void PortableLookPitch(int mode, float pitch)
{
#ifdef DXX_TOUCH_DEBUG
    LOGI("PortableLookPitch mode=%d v=%f", mode, pitch);
#endif
    if(mode == LOOK_MODE_JOYSTICK)
        dxx_touch_axis_pitch(pitch * GAIN_LOOK_PITCH, 0);
    else
        dxx_touch_axis_pitch(pitch, 1);
}

void PortableLookYaw(int mode, float yaw)
{
#ifdef DXX_TOUCH_DEBUG
    LOGI("PortableLookYaw mode=%d v=%f", mode, yaw);
#endif
    // Descent's heading is positive to the left, the opposite of the look
    // convention the touch layer uses.
    if(mode == LOOK_MODE_JOYSTICK)
        dxx_touch_axis_heading(-yaw * GAIN_LOOK_YAW, 0);
    else
        dxx_touch_axis_heading(-yaw, 1);
}

// Push a real SDL key event: the engine matches on keysym.sym, and going
// through the event queue means the menus, console and text entry all behave
// exactly as they do with a hardware keyboard.
static void inject_key(int state, int scancode)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    event.type = state ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.state = state ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.scancode = (SDL_Scancode) scancode;
    event.key.keysym.sym = SDL_GetKeyFromScancode((SDL_Scancode) scancode);

    SDL_PushEvent(&event);
}

int PortableKeyEvent(int state, int code, int unitcode)
{
    inject_key(state, code);
    return 0;
}

void PortableAction(int state, int action)
{
    switch(action)
    {
        // --- flight ---
        case PORT_ACT_FWD:          dxx_touch_action(state, DXX_TA_ACCELERATE); break;
        case PORT_ACT_BACK:         dxx_touch_action(state, DXX_TA_REVERSE); break;
        case PORT_ACT_MOVE_LEFT:    dxx_touch_action(state, DXX_TA_SLIDE_LEFT); break;
        case PORT_ACT_MOVE_RIGHT:   dxx_touch_action(state, DXX_TA_SLIDE_RIGHT); break;
        case PORT_ACT_VERT_UP:
        case PORT_ACT_UP:           dxx_touch_action(state, DXX_TA_SLIDE_UP); break;
        case PORT_ACT_VERT_DOWN:
        case PORT_ACT_DOWN:         dxx_touch_action(state, DXX_TA_SLIDE_DOWN); break;
        case PORT_ACT_BANK_LEFT:
        case PORT_ACT_LEAN_LEFT:    dxx_touch_action(state, DXX_TA_BANK_LEFT); break;
        case PORT_ACT_BANK_RIGHT:
        case PORT_ACT_LEAN_RIGHT:   dxx_touch_action(state, DXX_TA_BANK_RIGHT); break;

        // --- weapons ---
        case PORT_ACT_ATTACK:       dxx_touch_action(state, DXX_TA_FIRE_PRIMARY); break;
        case PORT_ACT_ALT_ATTACK:   dxx_touch_action(state, DXX_TA_FIRE_SECONDARY); break;
        case PORT_ACT_FIRE_FLARE:   dxx_touch_action(state, DXX_TA_FIRE_FLARE); break;
        case PORT_ACT_FIRE_BOMB:    dxx_touch_action(state, DXX_TA_DROP_BOMB); break;
        case PORT_ACT_CYCLE_PRIM:
        case PORT_ACT_NEXT_WEP:     dxx_touch_action(state, DXX_TA_CYCLE_PRIMARY); break;
        case PORT_ACT_CYCLE_SEC:
        case PORT_ACT_PREV_WEP:     dxx_touch_action(state, DXX_TA_CYCLE_SECONDARY); break;

        // --- ship systems (D2) ---
        case PORT_ACT_AFTERBURNER:   dxx_touch_action(state, DXX_TA_AFTERBURNER); break;
        case PORT_ACT_HEADLIGHT:     dxx_touch_action(state, DXX_TA_HEADLIGHT); break;
        case PORT_ACT_ENERGY_SHIELD: dxx_touch_action(state, DXX_TA_ENERGY_SHIELD); break;

        // --- view ---
        case PORT_ACT_MAP:          dxx_touch_action(state, DXX_TA_AUTOMAP); break;
        case PORT_ACT_REAR_VIEW:    dxx_touch_action(state, DXX_TA_REAR_VIEW); break;
        case PORT_ACT_COCKPIT_VIEW: inject_key(state, SDL_SCANCODE_F3); break;

        // --- menus / misc, straight to the keyboard ---
        case PORT_ACT_MENU_UP:      inject_key(state, SDL_SCANCODE_UP); break;
        case PORT_ACT_MENU_DOWN:    inject_key(state, SDL_SCANCODE_DOWN); break;
        case PORT_ACT_MENU_LEFT:    inject_key(state, SDL_SCANCODE_LEFT); break;
        case PORT_ACT_MENU_RIGHT:   inject_key(state, SDL_SCANCODE_RIGHT); break;
        case PORT_ACT_MENU_SELECT:  inject_key(state, SDL_SCANCODE_RETURN); break;
        case PORT_ACT_MENU_ABORT:   inject_key(state, SDL_SCANCODE_ESCAPE); break;
        case PORT_ACT_MENU_CONFIRM: inject_key(state, SDL_SCANCODE_Y); break;
        case PORT_ACT_QUICKSAVE:    inject_key(state, SDL_SCANCODE_F2); break;
        case PORT_ACT_QUICKLOAD:    inject_key(state, SDL_SCANCODE_F3); break;

        default:
            // The weapon-number buttons and the wheel arrive as WEAP0..WEAP9;
            // Descent selects with the number keys, 0 meaning weapon 10.
            if(action >= PORT_ACT_WEAP0 && action <= PORT_ACT_WEAP9)
            {
                if(state)
                    dxx_touch_select_weapon(action - PORT_ACT_WEAP0);
            }
            break;
    }
}

void PortableBackButton(void)
{
    // Descent has no single "menu" key in game: Esc opens the in-game menu and
    // backs out of menus, which is what the back button should do.
    inject_key(1, SDL_SCANCODE_ESCAPE);
    inject_key(0, SDL_SCANCODE_ESCAPE);
}

void PortableMouse(float dx, float dy)
{
    MouseMove(dx, dy);
}

void PortableMouseAbs(float x, float y)
{
    MouseMoveAbsolute(x, y);
}

void PortableMouseButton(int state, int button, float dx, float dy)
{
    MouseButton(state, button);
}

void PortableCommand(const char *cmd)
{
    // dxx-redux has no console command interface to drive from here.
}

int PortableShowKeyboard(void)
{
    return 0;
}

bool PortableSetAlwaysRun(bool run)
{
    // Descent has no run/walk toggle - the ship always flies at full thrust.
    return false;
}

void PortableSetMouseTapMode(int enable)
{
}

int PortableGetMouseTapMode(void)
{
    return 1;
}

touchscreemode_t PortableGetScreenMode()
{
    switch(dxx_touch_screen_mode())
    {
        case DXX_TS_GAME:
            return TS_GAME;
        case DXX_TS_MAP:
            return TS_MAP;
        case DXX_TS_MENU:
            return TS_MENU;
        default:
            return TS_BLANK;
    }
}

} // extern "C"
