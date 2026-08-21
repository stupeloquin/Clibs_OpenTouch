//
// Touch controls for Descent 3.
//

#ifndef DESCENT3_TOUCH_INTERFACE_H
#define DESCENT3_TOUCH_INTERFACE_H

#include "touch_interface_base.h"

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
    // One game, so the layout name is fixed - unlike the Descent 1/2 ports,
    // which name the layout after whichever game is running.
    std::string layoutName() const;
};

#endif /* DESCENT3_TOUCH_INTERFACE_H */
