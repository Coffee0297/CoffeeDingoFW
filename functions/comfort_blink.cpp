#include "comfort_blink.h"
#include <initializer_list>

void ComfortBlink::Step(Side &s, Side &other, bool bIn, bool bClock, uint32_t nTimeNow)
{
    if (bIn && !s.bLast) // Lever on
    {
        s.nSince = nTimeNow;
        s.bHeld = false;
        s.bCancel = other.bComfort; // A tap the other way during a comfort blink only cancels it
        other.bComfort = false;
        s.bComfort = false;         // A new tap on the same side starts the count again
        s.nBlinks = bClock ? 1 : 0; // An on-phase already showing counts
    }
    if (bIn && !s.bHeld && (nTimeNow - s.nSince) >= pConfig->nTapMs)
    {
        s.bHeld = true; // Held: a normal blink (also after a cancelling press)
        s.bCancel = false;
    }
    if (!bIn && s.bLast) // Lever off
    {
        // A tap: blink out the rest, a lit last blink finishes
        s.bComfort = !s.bHeld && !s.bCancel && (s.nBlinks < pConfig->nBlinks || bClock);
        s.bHeld = false;
        s.bCancel = false;
    }
    s.bLast = bIn;
}

void ComfortBlink::Update(uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled)
    {
        fLeft = fRight = 0;
        stLeft = Side{};
        stRight = Side{};
        return;
    }

    const bool bClock = *pClock != 0.0f;
    const bool bRise = bClock && !bLastClock;
    const bool bFall = !bClock && bLastClock;
    bLastClock = bClock;

    Step(stLeft, stRight, *pLeft != 0.0f, bClock, nTimeNow);
    Step(stRight, stLeft, *pRight != 0.0f, bClock, nTimeNow);

    for (Side *s : { &stLeft, &stRight })
    {
        const bool bJustOn = s->bLast && s->nSince == nTimeNow; // Counted when the lever went on
        if (bRise && s->On() && !bJustOn && s->nBlinks < 255)
            s->nBlinks++;
        // The comfort blink ends with the on-phase that completes the count
        if (bFall && s->bComfort && s->nBlinks >= pConfig->nBlinks)
            s->bComfort = false;
    }

    fLeft = stLeft.On() ? 1.0f : 0.0f;
    fRight = stRight.On() ? 1.0f : 0.0f;
}
