//
// Touch controls for Descent 1 & 2 (dxx-redux engine).
//
// Layout grid is 26 x 16 units, origin top left (same as the other ports).
//
// Descent is 6DOF, so the two sticks cover four of the six axes and the
// remaining two (vertical slide, bank) get button pairs down the screen edges.
// The engine reads all six from control_info - see dxx_game_interface.cpp.
//

#include "touch_interface.h"
#include "Framebuffer.h"
#include "SDL_keycode.h"
#include "SDL_scancode.h"

extern "C"
{

// Descent's automap is a 3D view; the generic 2D automap control is unused.
void PortableAutomapControl(float zoom, float x, float y)
{
}

}

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
#ifdef DXX_LAYOUT_FROM_GAME_TYPE
    return isD2() ? "descent2" : "descent1";
#else
    return ENGINE_NAME;
#endif
}

void TouchInterface::createControls(std::string filesPath)
{
    tcMenuMain = new touchcontrols::TouchControls("menu", false, true, 10, false);
    tcYesNo = new touchcontrols::TouchControls("yes_no", false, false);
    tcGameMain = new touchcontrols::TouchControls("game", false, true, 1, true);
    tcGameWeapons = new touchcontrols::TouchControls("weapons", false, true, 1, false);
    tcWeaponWheel = new touchcontrols::TouchControls("weapon_wheel", false, true, 1, false);
    tcWeaponWheel->hideEditButton = true;
    tcAutomap = new touchcontrols::TouchControls("automap", false, true, 3, false);
    tcBlank = new touchcontrols::TouchControls("blank", true, false);
    tcCustomButtons = new touchcontrols::TouchControls("custom_buttons", false, true, 1, true);
    tcKeyboard = new touchcontrols::TouchControls("keyboard", false, false);
    tcGamepadUtility = new touchcontrols::TouchControls("gamepad_utility", false, false);
    tcMouse = new touchcontrols::TouchControls("mouse", false, false);

    // Show main game controls when editing custom buttons
    tcCustomButtons->setEditBackgroundControl(tcGameMain);

    // Menu ---------------------------------------------------------------
    // Descent's menus are keyboard-driven lists, so the arrows and enter are
    // the primary way in; the invisible full-screen mouse handles taps.
    tcMenuMain->setFixAspect(true);
    tcMenuMain->addControl(new touchcontrols::Button("back", touchcontrols::RectF(0, 0, 2, 2), "back_button", KEY_BACK_BUTTON));

    tcMenuMain->addControl(new touchcontrols::Button("down_arrow", touchcontrols::RectF(20, 13, 23, 16), "arrow_down", PORT_ACT_MENU_DOWN));
    tcMenuMain->addControl(new touchcontrols::Button("up_arrow", touchcontrols::RectF(20, 10, 23, 13), "arrow_up", PORT_ACT_MENU_UP));
    tcMenuMain->addControl(new touchcontrols::Button("left_arrow", touchcontrols::RectF(17, 13, 20, 16), "arrow_left", PORT_ACT_MENU_LEFT));
    tcMenuMain->addControl(new touchcontrols::Button("right_arrow", touchcontrols::RectF(23, 13, 26, 16), "arrow_right", PORT_ACT_MENU_RIGHT));
    tcMenuMain->addControl(new touchcontrols::Button("enter", touchcontrols::RectF(0, 10, 6, 16), "enter", PORT_ACT_MENU_SELECT));

    tcMenuMain->addControl(new touchcontrols::Button("keyboard", touchcontrols::RectF(2, 0, 4, 2), "keyboard", KEY_SHOW_KBRD));
    tcMenuMain->addControl(new touchcontrols::Button("gamepad", touchcontrols::RectF(22, 0, 24, 2), "gamepad", KEY_SHOW_GAMEPAD));
    tcMenuMain->addControl(new touchcontrols::Button("gyro", touchcontrols::RectF(24, 0, 26, 2), "gyro", KEY_SHOW_GYRO));
    tcMenuMain->addControl(new touchcontrols::Button("load_save_touch", touchcontrols::RectF(20, 0, 22, 2), "touchscreen_save", KEY_LOAD_SAVE_CONTROLS));

    touchcontrols::Mouse *mouseMainMenu = new touchcontrols::Mouse("mouse", touchcontrols::RectF(0, 2, 26, 16), "");
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

    // Primary fire sits inside the look stick's area (see the stick below), so
    // the button doubles as the aim stick: press to fire, and drag from it to
    // keep aiming while you hold fire. The controls container hands each
    // pointer to every control it lands in, which is what makes that work.
    // Secondary fire stays above the stick so a steering drag never launches a
    // missile, and the move stick is left completely clear of both.
    tcGameMain->addControl(new touchcontrols::Button("fire_primary", touchcontrols::RectF(21, 5, 24, 8), "shoot", KEY_SHOOT, false, false,
                                                     "Fire primary (drag to aim)"));
    tcGameMain->addControl(new touchcontrols::Button("fire_secondary", touchcontrols::RectF(21, 2, 24, 4.8), "shoot_alt", PORT_ACT_ALT_ATTACK, false, false,
                                                     "Fire secondary"));
    tcGameMain->addControl(new touchcontrols::Button("cycle_primary", touchcontrols::RectF(0, 3, 3, 5), "next_weap", PORT_ACT_CYCLE_PRIM, false, false,
                                                     "Cycle primary"));
    tcGameMain->addControl(new touchcontrols::Button("cycle_secondary", touchcontrols::RectF(0, 5, 3, 7), "prev_weap", PORT_ACT_CYCLE_SEC, false, false,
                                                     "Cycle secondary"));
    tcGameMain->addControl(new touchcontrols::Button("fire_flare", touchcontrols::RectF(24, 3, 26, 5), "star", PORT_ACT_FIRE_FLARE, false, false,
                                                     "Flare"));
    tcGameMain->addControl(new touchcontrols::Button("drop_bomb", touchcontrols::RectF(24, 5, 26, 7), "red_strike", PORT_ACT_FIRE_BOMB, false, false,
                                                     "Drop bomb"));

    // Vertical slide and bank: the two axes the sticks do not cover.
    touchcontrols::ButtonGrid *slideVert = new touchcontrols::ButtonGrid("slide_vert", touchcontrols::RectF(24, 8, 26, 12), "", 1, 2, false,
                                                                        "Slide up/down");
    slideVert->addCell(0, 0, "direction_up", PORT_ACT_VERT_UP);
    slideVert->addCell(0, 1, "direction_down", PORT_ACT_VERT_DOWN);
    tcGameMain->addControl(slideVert);

    touchcontrols::ButtonGrid *bank = new touchcontrols::ButtonGrid("bank", touchcontrols::RectF(11, 14, 15, 16), "", 2, 1, false,
                                                                   "Bank left/right");
    bank->addCell(0, 0, "key_arrow_left", PORT_ACT_BANK_LEFT);
    bank->addCell(1, 0, "key_arrow_right", PORT_ACT_BANK_RIGHT);
    tcGameMain->addControl(bank);

    // Ship systems. The D2-only ones are created hidden in D1 so the layout
    // editor does not offer buttons that cannot do anything.
    bool hideD2 = !isD2();
    tcGameMain->addControl(new touchcontrols::Button("afterburner", touchcontrols::RectF(18, 2, 20, 4), "sprint", PORT_ACT_AFTERBURNER, false, hideD2,
                                                     "Afterburner (D2)"));
    tcGameMain->addControl(new touchcontrols::Button("headlight", touchcontrols::RectF(16, 0, 18, 2), "flashlight", PORT_ACT_HEADLIGHT, false, hideD2,
                                                     "Headlight (D2)"));
    tcGameMain->addControl(new touchcontrols::Button("energy_shield", touchcontrols::RectF(14, 0, 16, 2), "red_cross", PORT_ACT_ENERGY_SHIELD, false, true,
                                                     "Energy to shield (D2)"));

    // View / UI
    tcGameMain->addControl(new touchcontrols::Button("automap", touchcontrols::RectF(4, 0, 6, 2), "map", PORT_ACT_MAP, false, false, "Automap"));
    tcGameMain->addControl(new touchcontrols::Button("rear_view", touchcontrols::RectF(6, 0, 8, 2), "camera", PORT_ACT_REAR_VIEW, false, false,
                                                     "Rear view"));
    tcGameMain->addControl(new touchcontrols::Button("cockpit_view", touchcontrols::RectF(8, 0, 10, 2), "goggles", PORT_ACT_COCKPIT_VIEW, false, true,
                                                     "Cockpit view"));
    tcGameMain->addControl(new touchcontrols::Button("keyboard", touchcontrols::RectF(10, 0, 12, 2), "keyboard", KEY_SHOW_KBRD, false, true,
                                                     "Show keyboard"));
    tcGameMain->addControl(new touchcontrols::Button("show_weapons", touchcontrols::RectF(12, 14, 14, 16), "show_weapons", KEY_SHOW_WEAPONS, false, false,
                                                     "Show weapon numbers"));
    tcGameMain->addControl(new touchcontrols::Button("show_custom", touchcontrols::RectF(0, 2, 2, 4), "custom_show", KEY_SHOW_CUSTOM, false, true,
                                                     "Show custom"));

    // Sticks. Left = translation (fwd/back + slide), right = rotation
    // (pitch/yaw). Both are on the lower half so the HUD stays readable.
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

    // Weapon numbers -----------------------------------------------------
    // Descent selects weapons with the number keys: 1-5 primary, 6-0
    // secondary. In D2 pressing the same key again picks the super version.
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
    wheelSelect = new touchcontrols::WheelSelect("weapon_wheel", touchcontrols::RectF(7, 2, 19, 14), "weapon_wheel_%d", wheelNbr);
    wheelSelect->signal_selected.connect(sigc::mem_fun(this, &TouchInterface::weaponWheel));
    wheelSelect->signal_enabled.connect(sigc::mem_fun(this, &TouchInterface::weaponWheelSelected));
    tcWeaponWheel->addControl(wheelSelect);

    if(touchSettings.weaponWheelOpaque)
        tcWeaponWheel->setAlpha(0.8);
    else
        tcWeaponWheel->setAlpha(touchSettings.alpha);

    // Automap ------------------------------------------------------------
    // The automap flies the same way the ship does, so reuse a look stick and
    // give it its own exit button.
    tcAutomap->addControl(new touchcontrols::Button("automap_exit", touchcontrols::RectF(0, 0, 2, 2), "back_button", KEY_BACK_BUTTON, false, false,
                                                    "Leave automap"));

    touchcontrols::TouchJoy *automapJoy = new touchcontrols::TouchJoy("automap_stick", touchcontrols::RectF(17, 8, 26, 16), "look_arrow",
                                                                     "fixed_stick_circle");
    automapJoy->signal_move.connect(sigc::mem_fun(this, &TouchInterface::automapStick));
    tcAutomap->addControl(automapJoy);
    tcAutomap->signal_button.connect(sigc::mem_fun(this, &TouchInterface::automapButton));
    tcAutomap->setAlpha(touchSettings.alpha);

    // Blank (any tap dismisses a briefing / message screen) ---------------
    tcBlank->addControl(new touchcontrols::Button("enter", touchcontrols::RectF(0, 0, 26, 16), "", 0x123));
    tcBlank->signal_button.connect(sigc::mem_fun(this, &TouchInterface::blankButton));

    // Keyboard -----------------------------------------------------------
    uiKeyboard = new touchcontrols::UI_Keyboard("keyboard", touchcontrols::RectF(0, 8, 26, 16), "font_dual", 0, 0, 0);
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
    controlsContainer.addControlGroup(tcAutomap);
    controlsContainer.addControlGroup(tcMenuMain);
    controlsContainer.addControlGroup(tcWeaponWheel);
    controlsContainer.addControlGroup(tcBlank);
    controlsContainer.addControlGroup(tcMouse);

    std::string newSettings = (std::string) filesPath + "/touch_settings_" + layoutName() + ".xml";
    UI_tc = touchcontrols::createDefaultSettingsUI(&controlsContainer, newSettings);
    UI_tc->setAlpha(1);

    // No global XML suffix: every layout filename already carries ENGINE_NAME,
    // which is the game (descent1/descent2) and is shared across engine
    // families, so a layout follows the game rather than the port.

    tcMenuMain->setXMLFile((std::string) filesPath + "/menu.xml");
    tcGameMain->setXMLFile((std::string) filesPath + "/game_" + layoutName() + ".xml");
    tcGameWeapons->setXMLFile((std::string) filesPath + "/weapons_" + layoutName() + ".xml");
    tcWeaponWheel->setXMLFile((std::string) filesPath + "/weapon_wheel_" + layoutName() + ".xml");
    tcAutomap->setXMLFile((std::string) filesPath + "/automap_" + layoutName() + ".xml");
    tcCustomButtons->setXMLFile((std::string) filesPath + "/custom_" + layoutName() + ".xml");
}

void TouchInterface::blankButton(int state, int code)
{
    // Briefings and level-intro screens advance on any key.
    PortableKeyEvent(state, SDL_SCANCODE_RETURN, 0);
}

void TouchInterface::automapButton(int state, int code)
{
    if(code == KEY_BACK_BUTTON)
    {
        if(state)
            PortableKeyEvent(1, SDL_SCANCODE_ESCAPE, 0);
        else
            PortableKeyEvent(0, SDL_SCANCODE_ESCAPE, 0);

        return;
    }

    PortableAction(state, code);
}

void TouchInterface::automapStick(float joy_x, float joy_y, float mouse_x, float mouse_y)
{
    // Same handling as the in-game look stick: the automap view rotates with
    // pitch/heading in the engine.
    rightStick(joy_x, joy_y, mouse_x, mouse_y);
}

void TouchInterface::newFrame()
{
    touchscreemode_t screenMode = PortableGetScreenMode();

    // Hack to show custom buttons while in the menu to bind keys
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
    // dxx-redux allocates its own texture names with glGenTextures, so the
    // overlay's textures do not need an offset.
}
