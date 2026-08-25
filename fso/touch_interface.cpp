//
// Touch controls for FreeSpace Open.
//
// Layout grid is 26 x 16 units, origin top left, the same as the other ports.
//
// FreeSpace is not Descent. The ship has a throttle rather than six degrees of
// freedom, and the interesting half of the game is the computer - targeting,
// shields, energy, orders - which is 141 bindable actions deep. So this layout
// is deliberately layered rather than flat:
//
//   - the sticks and a handful of buttons carry flying and shooting,
//   - a throttle column down the left edge sets speed by position rather than
//     by holding a key, because FreeSpace's throttle is a position,
//   - the weapon wheel carries the targeting and energy commands, which are the
//     long tail a player reaches for a few times a minute rather than a few
//     times a second,
//   - and everything else stays on the on-screen keyboard, which has had shift
//     chords since 2026-08-24.
//
// The keys these send are FreeSpace 2's defaults - see fso_game_interface.cpp,
// which is where the mapping lives.
//

#include "touch_interface.h"
#include "Framebuffer.h"
#ifdef USE_SDL3
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_scancode.h"
#else
#include "SDL_keycode.h"
#include "SDL_scancode.h"
#endif

void TouchInterface::openGLStart()
{
    touchcontrols::gl_startRender();
}

void TouchInterface::openGLEnd()
{
    touchcontrols::gl_endRender();
}

std::string TouchInterface::layoutName() const
{
    return "freespace";
}

void TouchInterface::createControls(std::string filesPath)
{
    tcMenuMain = new touchcontrols::TouchControls("menu", false, true, 10, false);
    tcYesNo = new touchcontrols::TouchControls("yes_no", false, false);
    tcGameMain = new touchcontrols::TouchControls("game", false, true, 1, true);
    tcGameWeapons = new touchcontrols::TouchControls("weapons", false, true, 1, false);
    tcWeaponWheel = new touchcontrols::TouchControls("weapon_wheel", false, true, 1, false);
    tcWeaponWheel->hideEditButton = true;
    tcBlank = new touchcontrols::TouchControls("blank", true, false);
    tcCustomButtons = new touchcontrols::TouchControls("custom_buttons", false, true, 1, true);
    tcKeyboard = new touchcontrols::TouchControls("keyboard", false, false);
    tcGamepadUtility = new touchcontrols::TouchControls("gamepad_utility", false, false);
    tcMouse = new touchcontrols::TouchControls("mouse", false, false);

    tcCustomButtons->setEditBackgroundControl(tcGameMain);

    // Menu ---------------------------------------------------------------
    // FreeSpace's screens - the main hall, the briefing, the loadout - are
    // pointed at with a cursor, so the invisible full-screen mouse is the
    // primary control here and the keys are the fallback.
    tcMenuMain->setFixAspect(true);
    tcMenuMain->addControl(new touchcontrols::Button("back", touchcontrols::RectF(0, 0, 2, 2), "back_button", KEY_BACK_BUTTON));

    tcMenuMain->addControl(new touchcontrols::Button("down_arrow", touchcontrols::RectF(20, 13, 23, 16), "arrow_down", PORT_ACT_MENU_DOWN));
    tcMenuMain->addControl(new touchcontrols::Button("up_arrow", touchcontrols::RectF(20, 10, 23, 13), "arrow_up", PORT_ACT_MENU_UP));
    tcMenuMain->addControl(new touchcontrols::Button("left_arrow", touchcontrols::RectF(17, 13, 20, 16), "arrow_left", PORT_ACT_MENU_LEFT));
    tcMenuMain->addControl(new touchcontrols::Button("right_arrow", touchcontrols::RectF(23, 13, 26, 16), "arrow_right", PORT_ACT_MENU_RIGHT));
    tcMenuMain->addControl(new touchcontrols::Button("enter", touchcontrols::RectF(0, 10, 6, 16), "enter", PORT_ACT_MENU_SELECT));

    // Along the top, clear of the row of buttons FreeSpace puts across the
    // bottom of its dialogs.
    tcMenuMain->addControl(new touchcontrols::Button("tab", touchcontrols::RectF(5, 0, 8, 2), "key_tab", SDL_SCANCODE_TAB));
    tcMenuMain->addControl(new touchcontrols::Button("space", touchcontrols::RectF(8, 0, 11, 2), "toggle", SDL_SCANCODE_SPACE));

    tcMenuMain->addControl(new touchcontrols::Button("keyboard", touchcontrols::RectF(2, 0, 4, 2), "keyboard", KEY_SHOW_KBRD));
    tcMenuMain->addControl(new touchcontrols::Button("gamepad", touchcontrols::RectF(22, 0, 24, 2), "gamepad", KEY_SHOW_GAMEPAD));
    tcMenuMain->addControl(new touchcontrols::Button("gyro", touchcontrols::RectF(24, 0, 26, 2), "gyro", KEY_SHOW_GYRO));
    tcMenuMain->addControl(new touchcontrols::Button("load_save_touch", touchcontrols::RectF(20, 0, 22, 2), "touchscreen_save", KEY_LOAD_SAVE_CONTROLS));

    touchcontrols::Mouse *mouseMainMenu = new touchcontrols::Mouse("mouse", touchcontrols::RectF(0, 0, 26, 16), "");
    mouseMainMenu->setHideGraphics(true);
    mouseMainMenu->setEditable(false);
    mouseMainMenu->signal_action.connect(sigc::mem_fun(this, &TouchInterface::mouseMove));
    tcMenuMain->addControl(mouseMainMenu);

    tcMenuMain->signal_button.connect(sigc::mem_fun(this, &TouchInterface::menuButton));
    tcMenuMain->setAlpha(0.8);

    // Game ---------------------------------------------------------------
    tcGameMain->setAlpha(touchSettings.alpha);

    tcGameMain->addControl(new touchcontrols::Button("back", touchcontrols::RectF(0, 0, 2, 2), "back_button", KEY_BACK_BUTTON, false, false,
                                                     "Show menu"));

    // Primary fire sits inside the look stick's area, so the button doubles as
    // the aim stick: press to fire, drag from it to keep aiming. Secondary sits
    // above it so a steering drag never launches a missile.
    tcGameMain->addControl(new touchcontrols::Button("fire_primary", touchcontrols::RectF(21, 5, 24, 8), "shoot", KEY_SHOOT, false, false,
                                                     "Fire primary"));
    tcGameMain->addControl(new touchcontrols::Button("fire_secondary", touchcontrols::RectF(21, 2, 24, 4.8), "shoot_alt", PORT_ACT_ALT_ATTACK, false, false,
                                                     "Fire secondary"));
    tcGameMain->addControl(new touchcontrols::Button("countermeasure", touchcontrols::RectF(24, 3, 26, 5), "star", PORT_ACT_FS_COUNTERMEASURE, false, false,
                                                     "Launch countermeasure"));
    tcGameMain->addControl(new touchcontrols::Button("afterburner", touchcontrols::RectF(24, 5, 26, 7), "sprint", PORT_ACT_AFTERBURNER, false, false,
                                                     "Afterburner"));

    tcGameMain->addControl(new touchcontrols::Button("cycle_primary", touchcontrols::RectF(0, 3, 3, 5), "next_weap", PORT_ACT_FS_CYCLE_PRIMARY, false, false,
                                                     "Cycle primary"));
    tcGameMain->addControl(new touchcontrols::Button("cycle_secondary", touchcontrols::RectF(0, 5, 3, 7), "prev_weap", PORT_ACT_FS_CYCLE_SECONDARY, false, false,
                                                     "Cycle secondary"));

    /*
     * Throttle, down the right edge. FreeSpace's throttle is a position, not a
     * rate: a spring-back stick pushed forward and let go would leave the ship
     * back at whatever speed it started from, which is not how the game is
     * flown. So it gets fixed settings - the same ones the keyboard has - and
     * a pair of nudges either side of them.
     */
    touchcontrols::ButtonGrid *throttle = new touchcontrols::ButtonGrid("throttle", touchcontrols::RectF(24, 8, 26, 14), "", 1, 3, false,
                                                                       "Throttle: full / two-thirds / zero");
    throttle->addCell(0, 0, "direction_up", PORT_ACT_FS_THROTTLE_MAX);
    throttle->addCell(0, 1, "key_arrow_right", PORT_ACT_FS_THROTTLE_TWO_THIRD);
    throttle->addCell(0, 2, "direction_down", PORT_ACT_FS_THROTTLE_ZERO);
    tcGameMain->addControl(throttle);

    tcGameMain->addControl(new touchcontrols::Button("match_speed", touchcontrols::RectF(24, 14, 26, 16), "sprint_slow", PORT_ACT_FS_MATCH_SPEED, false, false,
                                                     "Match target speed"));

    // Targeting, the two a fight actually needs in the moment. The rest are on
    // the wheel.
    tcGameMain->addControl(new touchcontrols::Button("target_next", touchcontrols::RectF(16, 0, 18, 2), "binocular", PORT_ACT_FS_TARGET_NEXT, false, false,
                                                     "Target next ship"));
    tcGameMain->addControl(new touchcontrols::Button("target_hostile", touchcontrols::RectF(14, 0, 16, 2), "red_strike", PORT_ACT_FS_TARGET_HOSTILE, false, false,
                                                     "Target nearest hostile"));
    tcGameMain->addControl(new touchcontrols::Button("target_attacker", touchcontrols::RectF(12, 0, 14, 2), "red_cross", PORT_ACT_FS_TARGET_ATTACKER, false, false,
                                                     "Target closest attacker"));

    // Shields and comms, the two that keep a pilot alive and a wing useful.
    tcGameMain->addControl(new touchcontrols::Button("shield_equalize", touchcontrols::RectF(4, 0, 6, 2), "red_cross_color", PORT_ACT_FS_SHIELD_EQUALIZE, false, false,
                                                     "Equalize shields"));
    tcGameMain->addControl(new touchcontrols::Button("comms", touchcontrols::RectF(6, 0, 8, 2), "chat", PORT_ACT_FS_COMMS_MENU, false, false,
                                                     "Communications menu"));

    // The on-screen keyboard is the only way to reach the other hundred-odd
    // bindings, so it stays visible.
    tcGameMain->addControl(new touchcontrols::Button("keyboard", touchcontrols::RectF(10, 0, 12, 2), "keyboard", KEY_SHOW_KBRD, false, false,
                                                     "Show keyboard"));
    tcGameMain->addControl(new touchcontrols::Button("show_weapons", touchcontrols::RectF(12, 14, 14, 16), "show_weapons", KEY_SHOW_WEAPONS, false, false,
                                                     "Show number keys"));
    tcGameMain->addControl(new touchcontrols::Button("show_custom", touchcontrols::RectF(0, 2, 2, 4), "custom_show", KEY_SHOW_CUSTOM, false, true,
                                                     "Show custom"));

    // Sticks. Right = aim (pitch/yaw, as relative mouse), left = bank plus
    // thrust. Both on the lower half so the HUD stays readable.
    touchJoyRight = new touchcontrols::TouchJoy("touch", touchcontrols::RectF(17, 4.9, 26, 16), "look_arrow", "fixed_stick_circle");
    tcGameMain->addControl(touchJoyRight);
    touchJoyRight->signal_move.connect(sigc::mem_fun(this, &TouchInterface::rightStick));
    touchJoyRight->signal_double_tap.connect(sigc::mem_fun(this, &TouchInterface::rightDoubleTap));

    touchJoyLeft = new touchcontrols::TouchJoy("stick", touchcontrols::RectF(0, 8, 8, 16), "strafe_arrow", "fixed_stick_circle");
    tcGameMain->addControl(touchJoyLeft);
    touchJoyLeft->signal_move.connect(sigc::mem_fun(this, &TouchInterface::leftStick));
    touchJoyLeft->signal_double_tap.connect(sigc::mem_fun(this, &TouchInterface::leftDoubleTap));

    touchJoyLeft->registerTouchJoySWAPFIX(touchJoyRight);
    touchJoyRight->registerTouchJoySWAPFIX(touchJoyLeft);

    tcGameMain->signal_button.connect(sigc::mem_fun(this, &TouchInterface::gameButton));
    tcGameMain->signal_settingsButton.connect(sigc::mem_fun(this, &TouchInterface::gameSettingsButton));
    tcMenuMain->signal_settingsButton.connect(sigc::mem_fun(this, &TouchInterface::gameSettingsButton));

    // Number keys ---------------------------------------------------------
    // FreeSpace has no numbered weapon select, but its comms menu and its
    // squadmate orders are numbered lists, so the row is worth having under
    // the same button the Descent ports use for weapons.
    tcGameWeapons->addControl(new touchcontrols::Button("weapon1", touchcontrols::RectF(1, 14, 3, 16), "key_1", 1));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon2", touchcontrols::RectF(3, 14, 5, 16), "key_2", 2));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon3", touchcontrols::RectF(5, 14, 7, 16), "key_3", 3));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon4", touchcontrols::RectF(7, 14, 9, 16), "key_4", 4));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon5", touchcontrols::RectF(9, 14, 11, 16), "key_5", 5));

    tcGameWeapons->addControl(new touchcontrols::Button("weapon6", touchcontrols::RectF(15, 14, 17, 16), "key_6", 6));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon7", touchcontrols::RectF(17, 14, 19, 16), "key_7", 7));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon8", touchcontrols::RectF(19, 14, 21, 16), "key_8", 8));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon9", touchcontrols::RectF(21, 14, 23, 16), "key_9", 9));
    tcGameWeapons->addControl(new touchcontrols::Button("weapon0", touchcontrols::RectF(23, 14, 25, 16), "key_0", 0));

    tcGameWeapons->signal_button.connect(sigc::mem_fun(this, &TouchInterface::selectWeaponButton));
    tcGameWeapons->setAlpha(0.8);

    // Weapon wheel -------------------------------------------------------
    // Carries the long tail: the targeting and energy commands a pilot reaches
    // for between fights rather than during them.
    wheelSelect = new touchcontrols::WheelSelect("weapon_wheel", touchcontrols::RectF(7, 2, 19, 14), "weapon_wheel_%d", wheelNbr);
    wheelSelect->signal_selected.connect(sigc::mem_fun(this, &TouchInterface::weaponWheel));
    wheelSelect->signal_enabled.connect(sigc::mem_fun(this, &TouchInterface::weaponWheelSelected));
    tcWeaponWheel->addControl(wheelSelect);

    if(touchSettings.weaponWheelOpaque)
        tcWeaponWheel->setAlpha(0.8);
    else
        tcWeaponWheel->setAlpha(touchSettings.alpha);

    // Blank (any tap advances a briefing or a cutscene) -------------------
    tcBlank->addControl(new touchcontrols::Button("enter", touchcontrols::RectF(0, 0, 26, 16), "", 0x123));
    tcBlank->signal_button.connect(sigc::mem_fun(this, &TouchInterface::blankButton));

    // Keyboard -----------------------------------------------------------
    // The upper half, not the lower one the other ports use. FreeSpace raises
    // this itself whenever a screen wants typing - the pilot screen does, on
    // the way in - and it lays its own buttons along the bottom of the picture,
    // so a keyboard down there covers the CREATE and SELECT row the player has
    // to reach to get past it. The top of these screens is empty.
    uiKeyboard = new touchcontrols::UI_Keyboard("keyboard", touchcontrols::RectF(0, 0, 26, 8), "font_dual", 0, 0, 0);
    uiKeyboard->signal.connect(sigc::mem_fun(this, &TouchInterface::keyboardKeyPressed));
    tcKeyboard->addControl(uiKeyboard);
    tcKeyboard->setPassThroughTouch(touchcontrols::TouchControls::PassThrough::NO_CONTROL);

    // Yes/No -------------------------------------------------------------
    tcYesNo->addControl(new touchcontrols::Button("yes", touchcontrols::RectF(8, 12, 11, 15), "key_y", PORT_ACT_MENU_CONFIRM));
    tcYesNo->addControl(new touchcontrols::Button("no", touchcontrols::RectF(15, 12, 18, 15), "key_n", PORT_ACT_MENU_ABORT));
    tcYesNo->signal_button.connect(sigc::mem_fun(this, &TouchInterface::menuButton));
    tcYesNo->setAlpha(0.8);

    // Mouse screen -------------------------------------------------------
    touchcontrols::Mouse *mouseMouseMenu = new touchcontrols::Mouse("mouse", touchcontrols::RectF(0, 2, 26, 16), "");
    mouseMouseMenu->setHideGraphics(true);
    mouseMouseMenu->setEditable(false);
    mouseMouseMenu->signal_action.connect(sigc::mem_fun(this, &TouchInterface::mouseMove));
    tcMouse->addControl(mouseMouseMenu);
    tcMouse->addControl(new touchcontrols::Button("back", touchcontrols::RectF(0, 0, 2, 2), "back_button", KEY_BACK_BUTTON, false, false, "Back"));
    tcMouse->addControl(new touchcontrols::Button("hide_mouse", touchcontrols::RectF(4, 0, 6, 2), "mouse2", KEY_USE_MOUSE, false, false, "Hide mouse"));
    tcMouse->setAlpha(0.9);
    tcMouse->signal_button.connect(sigc::mem_fun(this, &TouchInterface::mouseButton));

    //---------------------------------------------------------------------
    controlsContainer.addControlGroup(tcKeyboard);
    controlsContainer.addControlGroup(tcGamepadUtility); // before gamemain so touches don't go through
    controlsContainer.addControlGroup(tcCustomButtons);
    controlsContainer.addControlGroup(tcGameMain);
    controlsContainer.addControlGroup(tcYesNo);
    controlsContainer.addControlGroup(tcGameWeapons);
    controlsContainer.addControlGroup(tcMenuMain);
    controlsContainer.addControlGroup(tcWeaponWheel);
    controlsContainer.addControlGroup(tcBlank);
    controlsContainer.addControlGroup(tcMouse);

    std::string newSettings = (std::string) filesPath + "/touch_settings_" + layoutName() + ".xml";
    UI_tc = touchcontrols::createDefaultSettingsUI(&controlsContainer, newSettings);
    UI_tc->setAlpha(1);

    tcMenuMain->setXMLFile((std::string) filesPath + "/menu.xml");
    tcGameMain->setXMLFile((std::string) filesPath + "/game_" + layoutName() + ".xml");
    tcGameWeapons->setXMLFile((std::string) filesPath + "/weapons_" + layoutName() + ".xml");
    tcWeaponWheel->setXMLFile((std::string) filesPath + "/weapon_wheel_" + layoutName() + ".xml");
    tcCustomButtons->setXMLFile((std::string) filesPath + "/custom_" + layoutName() + ".xml");
}

void TouchInterface::blankButton(int state, int code)
{
    // Briefings and cutscenes advance on a keypress.
    PortableKeyEvent(state, SDL_SCANCODE_RETURN, 0);
}

void TouchInterface::newFrame()
{
    touchscreemode_t screenMode = PortableGetScreenMode();

    // Show the custom buttons while binding them in the menu.
    if(screenMode == TS_MENU && showCustomMenu == true)
        screenMode = TS_CUSTOM;

    if((screenMode == TS_MENU) && (useMouse || gotMouseMove))
    {
        controlsContainer.showMouse(true);
    }
    else
    {
        useMouse = false;
        gotMouseMove = false;
        controlsContainer.showMouse(false);
    }

    updateTouchScreenModeOut(screenMode);
    updateTouchScreenModeIn(screenMode);

    currentScreenMode = screenMode;
}

void TouchInterface::newGLContext()
{
    // FreeSpace allocates its own texture names with glGenTextures, so the
    // overlay's textures need no offset.
}
