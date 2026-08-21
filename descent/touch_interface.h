//
// Touch controls for Descent 1 & 2 (dxx-redux engine).
//

#ifndef DESCENT_TOUCH_INTERFACE_H
#define DESCENT_TOUCH_INTERFACE_H

#include "touch_interface_base.h"

// gameType, as passed through from the launcher
#define DESCENT_1 0
#define DESCENT_2 1

class TouchInterface : public TouchInterfaceBase
{
public:
    void createControls(std::string filesPath);

    void openGLEnd();

    void openGLStart();

    void blankButton(int state, int code);

    void newFrame();

    void newGLContext();

    // Descent's automap is a free-flying 3D view, so it gets its own stick and
    // zoom buttons rather than the 2D pan/zoom the FPS ports use.
    void automapButton(int state, int code);

    void automapStick(float joy_x, float joy_y, float mouse_x, float mouse_y);

private:
    // Weapon controls differ between the games: D1 has 5 primaries and 5
    // secondaries, D2 adds a super version of each.
    bool isD2() const { return gameType == DESCENT_2; }

    // Layout files are named after the game, not the port, so a layout follows
    // the game from one engine to another. Ports that build a library per game
    // have the name at compile time; D2X-XL plays both games from one library,
    // so it has to ask which game is running.
    std::string layoutName() const;
};

#endif /* DESCENT_TOUCH_INTERFACE_H */
