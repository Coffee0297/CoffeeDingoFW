#pragma once

#include <cstdint>
#ifdef DINGO_HOST_TEST
#include "enums.h"
#else
#include "config.h"
#endif

class Edge
{
public:
    static bool Check(InputEdge eEdge, bool bPrev, bool bCurr)
    {
        switch (eEdge)
        {
        case InputEdge::Rising:
            return !bPrev && bCurr;
        case InputEdge::Falling:
            return bPrev && !bCurr;
        case InputEdge::Both:
            return bPrev != bCurr;
        default:
            return false;
        }
    }
};