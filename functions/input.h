#pragma once

#include "port.h"
#include "enums.h"

class Input
{
public:
    Input()
    {
    }

    bool Check(InputMode eMode, bool bInvert, bool bVal);

    // bLast = true: an input already on at boot is not a fresh press, so a latch is not toggled
    void Restore(bool bVal) { bOut = bVal; bLast = true; }

private:
    bool bOut;
    bool bLast;
};