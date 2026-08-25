//
// Touch controls for FreeSpace Open.
//

#ifndef FSO_TOUCH_INTERFACE_H
#define FSO_TOUCH_INTERFACE_H

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

private:
    // One engine, one game family, so the layout name is fixed.
    std::string layoutName() const;
};

#endif /* FSO_TOUCH_INTERFACE_H */
