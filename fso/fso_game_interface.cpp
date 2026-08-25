//
// The OpenTouch bridge for FreeSpace Open.
//
// Like the Descent 3 bridge, this asks almost nothing of the engine: FreeSpace
// reads its keyboard and mouse through SDL, so every touch here becomes a real
// SDL event and the engine cannot tell the difference. Rebinding a key in the
// game's own control config screen therefore follows - the touch button that
// sends that key keeps working.
//
// The one thing the engine has to export is which screen it is on
// (fso_touch_in_game, added to gamesequence.cpp), because the overlay has to
// choose between the flight sticks and the menu pointer and the glue is built
// without FSO's headers.
//
// The keys below are FreeSpace 2's own defaults, from the CONTROL_CONFIG table
// in code/controlconfig/controlsconfigcommon.cpp. Note that FSO's key names do
// not line up with SDL's: its KEY_SLASH is SDL_SCANCODE_BACKSLASH and its
// KEY_DIVIDE is SDL_SCANCODE_SLASH (see the SDLtoFS2 table in code/io/key.cpp),
// so the scancodes here are the ones that actually produce the wanted binding.
//

#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"   // SDL_SetMainReady, and SDL_main itself
// SDL_InjectMouse and the ACTION_* codes it takes.
#include "SDL_beloko_extra.h"

#include "game_interface.h"
#include "port_act_defs.h"

extern "C"
{

// True while a mission is being flown - see code/gamesequence/gamesequence.cpp.
extern int fso_touch_in_game(void);

// ---------------------------------------------------------------- lifecycle

// FreeSpace writes its own fs2_open.log, but the early failures - the ones that
// matter during bring-up - happen before that file is open. logcat gets
// everything, at the cost of a thread.
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
                __android_log_write(ANDROID_LOG_INFO, "FSOEngine", line);
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

void PortableInit(int argc, const char **argv)
{
    start_stdio_redirect();

    // SDL3 refuses to initialise unless it has been told main() was handled
    // elsewhere, and here it was: the launcher dlopens the library and calls in
    // through JNI, so SDL never gets to run its own entry point.
    SDL_SetMainReady();

    // SDL_main.h renames FreeSpace's main() to SDL_main on Android, which is
    // convenient - it means the engine itself needs no entry-point patch and
    // the symbol is already exported.
    SDL_main(argc, (char **) argv);

    // Only returns when the player quits.
    exit(0);
}

// ------------------------------------------------------------------- input

static SDL_Window *game_window(void);

static void inject_key(int state, SDL_Scancode scancode)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));

    event.type = state ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.down = state ? true : false;
    event.key.repeat = false;
    event.key.scancode = scancode;
    event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);

    /*
     * The window id matters. FreeSpace drops any key, motion or button event
     * that is not stamped with its own window - isWindowEvent() in
     * osapi.cpp, checked at the top of every input handler - and an event
     * built here starts with windowID 0. Without this the engine saw nothing
     * the on-screen keyboard sent: the pilot screen raised the keyboard,
     * took every keystroke, and never showed a character.
     */
    event.key.windowID = SDL_GetWindowID(game_window());

    SDL_PushEvent(&event);
}

// Several of FreeSpace's defaults are shift chords - the slide thrusters, time
// compression, targeting backwards. Wrapping the press keeps them held for as
// long as the button is, which the continuous ones need.
static void inject_shifted(int state, SDL_Scancode scancode)
{
    if (state)
    {
        inject_key(1, SDL_SCANCODE_LSHIFT);
        inject_key(1, scancode);
    }
    else
    {
        inject_key(0, scancode);
        inject_key(0, SDL_SCANCODE_LSHIFT);
    }
}

int PortableKeyEvent(int state, int code, int unitcode)
{
    inject_key(state, (SDL_Scancode) code);
    return 0;
}

// The window, for turning screen fractions into the pixels SDL wants. Looked up
// once the game has one - at the first touch there always is.
static SDL_Window *game_window(void)
{
    static SDL_Window *cached = nullptr;

    if (cached)
        return cached;

    int count = 0;
    SDL_Window * const *windows = SDL_GetWindows(&count);

    if (windows && count > 0)
        cached = windows[0];

    return cached;
}

/*
 * The framebuffer the engine renders into, set from the launcher's resolution
 * choice - see setFramebufferSize in android_jni_inc.cpp.
 *
 * This, and not the SDL window, is the space a click has to be expressed in.
 * The two are not the same size here: the window came back 2112 wide while
 * FreeSpace was rendering and hit-testing at 2410, so every click landed short
 * of the finger, and further short the nearer the right edge. On the pilot
 * screen a press on CREATE arrived in the gap beside it - the engine accepted
 * the click, at coordinates that were not over any button.
 */
extern int game_screen_width;
extern int game_screen_height;

static void window_size(float *w, float *h)
{
    int iw = game_screen_width;
    int ih = game_screen_height;

    // Before the framebuffer is configured, the window is the best guess going.
    if ((iw <= 0 || ih <= 0) && game_window())
        SDL_GetWindowSizeInPixels(game_window(), &iw, &ih);

    // Something plausible rather than zero, so a touch before the window exists
    // is merely wrong rather than a divide by nothing.
    *w = iw > 0 ? (float) iw : 1280.0f;
    *h = ih > 0 ? (float) ih : 720.0f;
}

/*
 * Pitch and yaw go in as relative mouse motion. FreeSpace binds JOY_HEADING_AXIS
 * and JOY_PITCH_AXIS to MOUSE_X_AXIS and MOUSE_Y_AXIS by default, and those
 * bindings are live whether or not the "use mouse to fly" option is set - that
 * option only decides whether the mouse pretends to be joystick 0
 * (scale_invert() in controlsconfig.cpp).
 *
 * The engine multiplies what it reads by its own Mouse_sensitivity and by the
 * frame time, so this only has to get the magnitude into the right region;
 * the player tunes the rest in the game's options screen.
 */
static const float kLookScale = 90.0f;

// Where the finger last was, as a fraction of the window. Kept so a click can
// be placed where the finger is rather than wherever the cursor drifted to.
static float lastTouchX = 0.5f;
static float lastTouchY = 0.5f;

void PortableLookPitch(int mode, float pitch)
{
    SDL_InjectMouse(0, ACTION_MOVE, 0, pitch * kLookScale, 1);
}

void PortableLookYaw(int mode, float yaw)
{
    SDL_InjectMouse(0, ACTION_MOVE, yaw * kLookScale, 0, 1);
}

// Dragging a finger over a menu moves the pointer.
void PortableMouse(float dx, float dy)
{
    float w, h;
    window_size(&w, &h);

    SDL_InjectMouse(0, ACTION_MOVE, dx * w, dy * h, 1);
}

// FreeSpace's menus are pointed at, and its mouse position comes straight from
// the SDL motion event (Mouse_x = x, in code/io/mouse.cpp), so placing the
// cursor is a matter of sending an absolute motion.
void PortableMouseAbs(float x, float y)
{
    float w, h;
    window_size(&w, &h);

    lastTouchX = x;
    lastTouchY = y;

    SDL_InjectMouse(0, ACTION_MOVE, x * w, y * h, 0);
}

void PortableMouseButton(int state, int button, float dx, float dy)
{
    float w, h;
    window_size(&w, &h);

    /*
     * SDL_InjectMouse rather than a hand-built event: it goes through SDL's own
     * path, which updates the position SDL holds as well as posting the event.
     * Pushing raw events instead left that position untouched and the cursor
     * stopped moving altogether.
     *
     * SDL wants the mask of buttons held *after* the change and works out which
     * one moved by itself, so the released button must not be passed on the way
     * up. The coordinates go along with it so the click lands where the finger
     * is rather than wherever the cursor drifted to.
     */
    SDL_InjectMouse(state ? button : 0, state ? ACTION_DOWN : ACTION_UP,
                    lastTouchX * w, lastTouchY * h, 0);
}

/*
 * Movement. FreeSpace is not 6DOF: the ship has a throttle rather than a thrust
 * axis, and A/Z (FORWARD_THRUST/REVERSE_THRUST) are what move it. They are
 * momentary keys, so the left stick's forward axis becomes a press held for as
 * long as it is pushed past a threshold - a stick position cannot be handed to
 * the engine as a throttle setting without teaching it a new axis, which is the
 * next piece of work here.
 *
 * The threshold is low because leftStick() has already scaled its output up: a
 * comfortable thumb throw arrives around 1.0, not 0.1.
 */
static const float kStickThreshold = 0.35f;

// What is currently held, so a stick crossing the threshold presses once and a
// stick returning to centre releases once.
static int heldFwd = 0;        // -1 back, 0 none, +1 forward
static int heldBank = 0;       // -1 left, 0 none, +1 right
static int heldSlideH = 0;
static int heldSlideV = 0;

static void hold_pair(int *held, float value, SDL_Scancode negKey, SDL_Scancode posKey, bool shifted)
{
    const int want = value > kStickThreshold ? 1 : (value < -kStickThreshold ? -1 : 0);

    if (want == *held)
        return;

    void (*send)(int, SDL_Scancode) = shifted ? inject_shifted : inject_key;

    if (*held > 0)
        send(0, posKey);
    else if (*held < 0)
        send(0, negKey);

    if (want > 0)
        send(1, posKey);
    else if (want < 0)
        send(1, negKey);

    *held = want;
}

void PortableMoveFwd(float fwd)
{
    // Push forward to accelerate, pull back to slow: A and Z.
    hold_pair(&heldFwd, fwd, SDL_SCANCODE_Z, SDL_SCANCODE_A, false);
}

void PortableMoveSide(float strafe)
{
    // Sideways on the movement stick is a bank, which is what a flight stick
    // does and what the ship's own roll axis expects. The slide thrusters are
    // on their own buttons.
    hold_pair(&heldBank, strafe, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_9, false);
}

void PortableMoveVert(float vert)
{
    // UP_SLIDE_THRUST / DOWN_SLIDE_THRUST, both shift chords.
    hold_pair(&heldSlideV, vert, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_KP_PLUS, true);
}

void PortableMove(float fwd, float strafe)
{
    PortableMoveFwd(fwd);
    PortableMoveSide(strafe);
}

void PortableRoll(float roll)
{
    hold_pair(&heldBank, roll, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_9, false);
}

void PortableGamepadAxis(int axis, float value)
{
    switch(axis)
    {
        case ANALOGUE_AXIS_FWD:   PortableMoveFwd(value); break;
        // A pad's second stick sideways slides rather than banks: bank is on
        // the movement stick, and sliding is the axis a pad otherwise cannot
        // reach without a shift chord on the keyboard.
        case ANALOGUE_AXIS_SIDE:
            hold_pair(&heldSlideH, value, SDL_SCANCODE_1, SDL_SCANCODE_3, true);
            break;
        case ANALOGUE_AXIS_VERT:  PortableMoveVert(value); break;
        case ANALOGUE_AXIS_ROLL:  PortableRoll(value); break;
        case ANALOGUE_AXIS_PITCH: PortableLookPitch(LOOK_MODE_JOYSTICK, value); break;
        case ANALOGUE_AXIS_YAW:   PortableLookYaw(LOOK_MODE_JOYSTICK, value); break;
        default: break;
    }
}

void PortableAction(int state, int action)
{
    switch(action)
    {
        // --- flight ---
        case PORT_ACT_FWD:          inject_key(state, SDL_SCANCODE_A); break;
        case PORT_ACT_BACK:         inject_key(state, SDL_SCANCODE_Z); break;
        case PORT_ACT_BANK_LEFT:
        case PORT_ACT_LEAN_LEFT:    inject_key(state, SDL_SCANCODE_KP_7); break;
        case PORT_ACT_BANK_RIGHT:
        case PORT_ACT_LEAN_RIGHT:   inject_key(state, SDL_SCANCODE_KP_9); break;
        case PORT_ACT_MOVE_LEFT:    inject_shifted(state, SDL_SCANCODE_1); break;
        case PORT_ACT_MOVE_RIGHT:   inject_shifted(state, SDL_SCANCODE_3); break;
        case PORT_ACT_VERT_UP:
        case PORT_ACT_UP:           inject_shifted(state, SDL_SCANCODE_KP_PLUS); break;
        case PORT_ACT_VERT_DOWN:
        case PORT_ACT_DOWN:         inject_shifted(state, SDL_SCANCODE_KP_ENTER); break;
        case PORT_ACT_AFTERBURNER:  inject_key(state, SDL_SCANCODE_TAB); break;
        case PORT_ACT_FS_GLIDE:
            // TOGGLE_GLIDING is Alt+G.
            if (state)
            {
                inject_key(1, SDL_SCANCODE_LALT);
                inject_key(1, SDL_SCANCODE_G);
            }
            else
            {
                inject_key(0, SDL_SCANCODE_G);
                inject_key(0, SDL_SCANCODE_LALT);
            }
            break;

        // --- throttle. Note the FSO key names: MAX_THROTTLE is its KEY_SLASH,
        // which the SDLtoFS2 table feeds from SDL_SCANCODE_BACKSLASH.
        case PORT_ACT_FS_THROTTLE_UP:       inject_key(state, SDL_SCANCODE_EQUALS); break;
        case PORT_ACT_FS_THROTTLE_DOWN:     inject_key(state, SDL_SCANCODE_MINUS); break;
        case PORT_ACT_FS_THROTTLE_ZERO:     inject_key(state, SDL_SCANCODE_BACKSPACE); break;
        case PORT_ACT_FS_THROTTLE_THIRD:    inject_key(state, SDL_SCANCODE_LEFTBRACKET); break;
        case PORT_ACT_FS_THROTTLE_TWO_THIRD:inject_key(state, SDL_SCANCODE_RIGHTBRACKET); break;
        case PORT_ACT_FS_THROTTLE_MAX:      inject_key(state, SDL_SCANCODE_BACKSLASH); break;
        case PORT_ACT_FS_MATCH_SPEED:       inject_key(state, SDL_SCANCODE_M); break;

        // --- weapons ---
        case PORT_ACT_ATTACK:       inject_key(state, SDL_SCANCODE_LCTRL); break;
        case PORT_ACT_ALT_ATTACK:   inject_key(state, SDL_SCANCODE_SPACE); break;
        case PORT_ACT_FS_COUNTERMEASURE: inject_key(state, SDL_SCANCODE_X); break;
        case PORT_ACT_CYCLE_PRIM:
        case PORT_ACT_NEXT_WEP:
        case PORT_ACT_FS_CYCLE_PRIMARY:   inject_key(state, SDL_SCANCODE_PERIOD); break;
        case PORT_ACT_CYCLE_SEC:
        case PORT_ACT_PREV_WEP:
        // CYCLE_SECONDARY is FSO's KEY_DIVIDE, fed from SDL's plain slash.
        case PORT_ACT_FS_CYCLE_SECONDARY: inject_key(state, SDL_SCANCODE_SLASH); break;

        // --- targeting ---
        case PORT_ACT_FS_TARGET_NEXT:     inject_key(state, SDL_SCANCODE_T); break;
        case PORT_ACT_FS_TARGET_PREV:     inject_shifted(state, SDL_SCANCODE_T); break;
        case PORT_ACT_FS_TARGET_HOSTILE:  inject_key(state, SDL_SCANCODE_H); break;
        case PORT_ACT_FS_TARGET_ATTACKER: inject_key(state, SDL_SCANCODE_R); break;
        case PORT_ACT_FS_TARGET_IN_RETICLE: inject_key(state, SDL_SCANCODE_Y); break;
        case PORT_ACT_FS_TARGET_SUBSYSTEM:  inject_key(state, SDL_SCANCODE_S); break;

        // --- shields and energy ---
        case PORT_ACT_FS_SHIELD_EQUALIZE: inject_key(state, SDL_SCANCODE_Q); break;
        case PORT_ACT_FS_SHIELD_FORWARD:  inject_key(state, SDL_SCANCODE_UP); break;
        case PORT_ACT_FS_ENERGY_WEAPONS:  inject_key(state, SDL_SCANCODE_INSERT); break;
        case PORT_ACT_FS_ENERGY_SHIELDS:  inject_key(state, SDL_SCANCODE_HOME); break;
        case PORT_ACT_FS_ENERGY_ENGINES:  inject_key(state, SDL_SCANCODE_PAGEUP); break;

        // --- computer ---
        case PORT_ACT_FS_COMMS_MENU:   inject_key(state, SDL_SCANCODE_C); break;
        case PORT_ACT_FS_RADAR_RANGE:  inject_key(state, SDL_SCANCODE_APOSTROPHE); break;
        case PORT_ACT_FS_TOGGLE_HUD:   inject_shifted(state, SDL_SCANCODE_O); break;
        case PORT_ACT_FS_TIME_COMPRESS:inject_shifted(state, SDL_SCANCODE_PERIOD); break;
        case PORT_ACT_FS_TIME_EXPAND:  inject_shifted(state, SDL_SCANCODE_COMMA); break;

        // --- menus, straight to the keyboard ---
        case PORT_ACT_MENU_UP:      inject_key(state, SDL_SCANCODE_UP); break;
        case PORT_ACT_MENU_DOWN:    inject_key(state, SDL_SCANCODE_DOWN); break;
        case PORT_ACT_MENU_LEFT:    inject_key(state, SDL_SCANCODE_LEFT); break;
        case PORT_ACT_MENU_RIGHT:   inject_key(state, SDL_SCANCODE_RIGHT); break;
        case PORT_ACT_MENU_SELECT:  inject_key(state, SDL_SCANCODE_RETURN); break;
        case PORT_ACT_MENU_TAB:     inject_key(state, SDL_SCANCODE_TAB); break;
        case PORT_ACT_MENU_BACK:
        case PORT_ACT_MENU_ABORT:   inject_key(state, SDL_SCANCODE_ESCAPE); break;
        case PORT_ACT_MENU_CONFIRM: inject_key(state, SDL_SCANCODE_Y); break;

        default:
            // The weapon buttons and the wheel arrive as WEAP0..WEAP9. FreeSpace
            // has no numbered weapon select, but its comms menu and its
            // squadmate orders are numbered lists, so the digits are what those
            // want - which is exactly what the wheel is for.
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
    inject_key(1, SDL_SCANCODE_ESCAPE);
    inject_key(0, SDL_SCANCODE_ESCAPE);
}

void PortableCommand(const char *cmd)
{
    // No console to drive from here.
}

void PortableAutomapControl(float zoom, float x, float y)
{
    // FreeSpace has no automap.
}

int PortableShowKeyboard(void)
{
    return 0;
}

bool PortableSetAlwaysRun(bool run)
{
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
    // Everything that is not a mission being flown - main hall, briefing,
    // loadout, debriefing, options - is mouse-driven UI, so the overlay wants
    // its menu layout there rather than the flight sticks.
    return fso_touch_in_game() ? TS_GAME : TS_MENU;
}

} // extern "C"
