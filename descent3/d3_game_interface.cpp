//
// The OpenTouch bridge for Descent 3.
//
// Unlike the Descent 1/2 ports, this one asks nothing of the engine. Descent 3
// reads its input through SDL already - keyboard and mouse in ddio's SDL layer -
// so every touch and gamepad action here becomes a real SDL event and the engine
// cannot tell the difference. That keeps the engine fork to the build files and
// the entry point, and it means the in-game control config screen still works:
// rebind a key there and the touch button that sends it follows.
//
// The keys chosen below are Descent 3's own defaults, from Controller_needs in
// Descent3/Controls.cpp. If a player rebinds, these stop matching - a later
// pass should read the bindings back rather than assume them.
//

#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"   // SDL_SetMainReady
// SDL_InjectMouse and the ACTION_* codes it takes.
#include "SDL_beloko_extra.h"

#include "game_interface.h"
#include "port_act_defs.h"

extern "C"
{

// Renamed from main() under Android, see Descent3/sdlmain.cpp.
extern int dxx_main(int argc, char *argv[]);

// ---------------------------------------------------------------- lifecycle

// The engine logs through plog to stdout, and SDL only redirects stdout to
// logcat when it runs the main function itself. This port enters through
// NativeLib.init instead, so pump it across ourselves - the same trick the
// Descent 1/2 glue uses.
static void *stdio_to_logcat(void *arg)
{
    const int fd = (int) (intptr_t) arg;
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
                __android_log_write(ANDROID_LOG_INFO, "D3Engine", line);
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

// Descent 3 takes its directories from the environment: D3_LOCAL for the
// writable side and D3_DIR for the shared data. The framework already points
// HOME at the game folder, so both come from there - one folder, as the other
// ports use.
static void set_game_directories(void)
{
    const char *game_path = getenv("HOME");

    if (!game_path)
        return;

    setenv("D3_LOCAL", game_path, 1);
    setenv("D3_DIR", game_path, 1);
}

void PortableInit(int argc, const char **argv)
{
    start_stdio_redirect();
    set_game_directories();

    // SDL3 refuses to initialise unless it has been told main() was handled
    // elsewhere, and here it was: the launcher dlopens the library and calls in
    // through JNI, so SDL never gets to run its own entry point.
    SDL_SetMainReady();

    dxx_main(argc, (char **) argv);

    // dxx_main only returns when the player quits.
    exit(0);
}

// ------------------------------------------------------------------- input

// Push a real SDL key event. Going through the queue means the menus, the
// console and text entry behave exactly as they do with a hardware keyboard.
static void inject_key(int state, SDL_Scancode scancode)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    event.type = state ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.down = state ? true : false;
    event.key.repeat = false;
    event.key.scancode = scancode;
    event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);

    SDL_PushEvent(&event);
}

int PortableKeyEvent(int state, int code, int unitcode)
{
    inject_key(state, (SDL_Scancode) code);
    return 0;
}

// Descent 3 takes pitch and heading from the mouse axes by default
// (ctfPITCH_DOWNAXIS and ctfHEADING_RIGHTAXIS, both ctMouseAxis), so the look
// stick drives relative mouse motion.
void PortableLookPitch(int mode, float pitch)
{
    SDL_InjectMouse(0, ACTION_MOVE, 0, pitch, 1);
}

void PortableLookYaw(int mode, float yaw)
{
    SDL_InjectMouse(0, ACTION_MOVE, yaw, 0, 1);
}

void PortableMouse(float dx, float dy)
{
    SDL_InjectMouse(0, ACTION_MOVE, dx, dy, 1);
}

void PortableMouseAbs(float x, float y)
{
    SDL_InjectMouse(0, ACTION_HOVER_MOVE, x, y, 0);
}

void PortableMouseButton(int state, int button, float dx, float dy)
{
    SDL_InjectMouse(button, state ? ACTION_DOWN : ACTION_UP, dx, dy, 0);
}

// Thrust has no mouse or keyboard analogue in Descent 3 - the axes are joystick
// only - so the movement sticks fall back to the digital thrust keys and are
// either on or off. A virtual SDL joystick would give real analogue thrust and
// is the obvious next step; this at least flies.
static void axis_as_keys(float value, SDL_Scancode positive, SDL_Scancode negative)
{
    static const float threshold = 0.2f;

    inject_key(value > threshold, positive);
    inject_key(value < -threshold, negative);
}

void PortableMoveFwd(float fwd)
{
    axis_as_keys(fwd, SDL_SCANCODE_A, SDL_SCANCODE_Z);
}

void PortableMoveSide(float strafe)
{
    axis_as_keys(strafe, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_1);
}

void PortableMoveVert(float vert)
{
    axis_as_keys(vert, SDL_SCANCODE_KP_MINUS, SDL_SCANCODE_KP_PLUS);
}

void PortableMove(float fwd, float strafe)
{
    PortableMoveFwd(fwd);
    PortableMoveSide(strafe);
}

void PortableRoll(float roll)
{
    axis_as_keys(roll, SDL_SCANCODE_E, SDL_SCANCODE_Q);
}

void PortableGamepadAxis(int axis, float value)
{
    switch(axis)
    {
        case ANALOGUE_AXIS_FWD:   PortableMoveFwd(value); break;
        case ANALOGUE_AXIS_SIDE:  PortableMoveSide(value); break;
        case ANALOGUE_AXIS_VERT:  PortableMoveVert(value); break;
        case ANALOGUE_AXIS_ROLL:  PortableRoll(value); break;
        case ANALOGUE_AXIS_PITCH: PortableLookPitch(0, value); break;
        case ANALOGUE_AXIS_YAW:   PortableLookYaw(0, value); break;
        default: break;
    }
}

void PortableAction(int state, int action)
{
    switch(action)
    {
        // --- flight, all on Descent 3's default keys ---
        case PORT_ACT_FWD:          inject_key(state, SDL_SCANCODE_A); break;
        case PORT_ACT_BACK:         inject_key(state, SDL_SCANCODE_Z); break;
        case PORT_ACT_MOVE_LEFT:    inject_key(state, SDL_SCANCODE_KP_1); break;
        case PORT_ACT_MOVE_RIGHT:   inject_key(state, SDL_SCANCODE_KP_3); break;
        case PORT_ACT_VERT_UP:
        case PORT_ACT_UP:           inject_key(state, SDL_SCANCODE_KP_MINUS); break;
        case PORT_ACT_VERT_DOWN:
        case PORT_ACT_DOWN:         inject_key(state, SDL_SCANCODE_KP_PLUS); break;
        case PORT_ACT_BANK_LEFT:
        case PORT_ACT_LEAN_LEFT:    inject_key(state, SDL_SCANCODE_Q); break;
        case PORT_ACT_BANK_RIGHT:
        case PORT_ACT_LEAN_RIGHT:   inject_key(state, SDL_SCANCODE_E); break;

        // --- weapons ---
        case PORT_ACT_ATTACK:       inject_key(state, SDL_SCANCODE_LCTRL); break;
        case PORT_ACT_ALT_ATTACK:   inject_key(state, SDL_SCANCODE_SPACE); break;
        case PORT_ACT_FIRE_FLARE:   inject_key(state, SDL_SCANCODE_F); break;
        case PORT_ACT_CYCLE_PRIM:
        case PORT_ACT_NEXT_WEP:     inject_key(state, SDL_SCANCODE_COMMA); break;
        case PORT_ACT_CYCLE_SEC:
        case PORT_ACT_PREV_WEP:     inject_key(state, SDL_SCANCODE_PERIOD); break;

        // --- ship systems. Countermeasures are Descent 3's own, and the bomb
        // button releases one since that is the nearest thing it has.
        case PORT_ACT_AFTERBURNER:  inject_key(state, SDL_SCANCODE_S); break;
        case PORT_ACT_HEADLIGHT:    inject_key(state, SDL_SCANCODE_H); break;
        case PORT_ACT_FIRE_BOMB:    inject_key(state, SDL_SCANCODE_RETURN); break;

        // --- view ---
        case PORT_ACT_MAP:          inject_key(state, SDL_SCANCODE_TAB); break;
        case PORT_ACT_REAR_VIEW:    inject_key(state, SDL_SCANCODE_R); break;

        // --- menus, straight to the keyboard ---
        case PORT_ACT_MENU_UP:      inject_key(state, SDL_SCANCODE_UP); break;
        case PORT_ACT_MENU_DOWN:    inject_key(state, SDL_SCANCODE_DOWN); break;
        case PORT_ACT_MENU_LEFT:    inject_key(state, SDL_SCANCODE_LEFT); break;
        case PORT_ACT_MENU_RIGHT:   inject_key(state, SDL_SCANCODE_RIGHT); break;
        case PORT_ACT_MENU_SELECT:  inject_key(state, SDL_SCANCODE_RETURN); break;
        case PORT_ACT_MENU_BACK:
        case PORT_ACT_MENU_ABORT:   inject_key(state, SDL_SCANCODE_ESCAPE); break;
        case PORT_ACT_MENU_CONFIRM: inject_key(state, SDL_SCANCODE_Y); break;
        case PORT_ACT_QUICKSAVE:    inject_key(state, SDL_SCANCODE_F2); break;
        case PORT_ACT_QUICKLOAD:    inject_key(state, SDL_SCANCODE_F3); break;

        default:
            // Weapon buttons and the wheel arrive as WEAP0..WEAP9. Descent 3
            // selects with the number keys, 0 meaning the tenth.
            if(action >= PORT_ACT_WEAP0 && action <= PORT_ACT_WEAP9)
            {
                const int n = action - PORT_ACT_WEAP0;
                inject_key(state, (SDL_Scancode) (n == 0 ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + n - 1));
            }
            break;
    }
}

void PortableBackButton(void)
{
    // Esc opens the in-game menu and backs out of menus, which is what the
    // back button should do.
    inject_key(1, SDL_SCANCODE_ESCAPE);
    inject_key(0, SDL_SCANCODE_ESCAPE);
}

void PortableCommand(const char *cmd)
{
    // Descent 3 has no console command interface to drive from here.
}

int PortableShowKeyboard(void)
{
    return 0;
}

bool PortableSetAlwaysRun(bool run)
{
    // No run/walk toggle - the ship always flies at full thrust.
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
    // Until the engine reports its own state, the game layout is always up.
    // Descent 3's menus are mouse-driven, so the touchscreen works on them
    // without a menu overlay of its own.
    return TS_GAME;
}

} // extern "C"
