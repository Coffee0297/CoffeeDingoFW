#include "counter.h"
#include "edge.h"

void Counter::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    // Edges are taken against the previous cycle's inputs, so every exit records this cycle's (a reset used
    // to return early: a falling-edge reset then fired again on every cycle and held the count at 0 until the
    // reset input came back).
    const bool bInc = *pIncInput, bDec = *pDecInput, bReset = *pResetInput;
    const bool bIncEdge = Edge::Check(pConfig->eIncEdge, bLastInc, bInc);
    const bool bDecEdge = Edge::Check(pConfig->eDecEdge, bLastDec, bDec);
    const bool bResetEdge = Edge::Check(pConfig->eResetEdge, bLastReset, bReset);
    bLastInc = bInc;
    bLastDec = bDec;
    bLastReset = bReset;

    // Reset
    if (bResetEdge)
    {
        fVal = 0;
        return;
    }

    // Hold to reset
    if (pConfig->bHoldToReset)
    {
        if (bInc && (SYS_TIME - nLastIncTime >= pConfig->nResetTime))
        {
            fVal = 0;
            return;
        }

        if (bDec && (SYS_TIME - nLastDecTime >= pConfig->nResetTime))
        {
            fVal = 0;
            return;
        }
    }

    // Increment
    if (bIncEdge)
    {
        fVal++;
        if (fVal > pConfig->nMaxCount)
        {
            fVal = pConfig->bWrapAround ? 0 : pConfig->nMaxCount;
        }

        nLastIncTime = SYS_TIME;
    }

    // Decrement
    if (bDecEdge)
    {
        if(fVal == 0)
        {
            fVal = pConfig->bWrapAround ? pConfig->nMaxCount : pConfig->nMinCount;
        }
        else
        {
            fVal--;
        }

        nLastDecTime = SYS_TIME;
    }

}