#include "condition.h"
#include "dbc.h"

void Condition::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    // Hysteresis (relational ops only): when fArgOff differs from fArg it's the release point, so
    // the output turns on past the further threshold and only releases past the nearer one — order
    // independent (works whether fArgOff is below fArg for '>' or above it for '<'). fArgOff==fArg
    // (the default) collapses to a plain comparison, so existing configs are unchanged.
    const float in = *pInput;
    const float set = pConfig->fArg;
    const float rel = pConfig->fArgOff;
    const bool hyst = (rel != set);
    const bool wasOn = (fVal != 0);
    const float lo = (set < rel) ? set : rel;   // nearer-to-off for '>', further-on for '<'
    const float hi = (set > rel) ? set : rel;

    switch(pConfig->eOperator)
    {
        case Operator::Equal:
            fVal = (in == set) ? 1.0f : 0.0f;
            break;

        case Operator::NotEqual:
            fVal = (in != set) ? 1.0f : 0.0f;
            break;

        case Operator::GreaterThan:
            fVal = (hyst ? (wasOn ? in > lo : in > hi) : in > set) ? 1.0f : 0.0f;
            break;

        case Operator::LessThan:
            fVal = (hyst ? (wasOn ? in < hi : in < lo) : in < set) ? 1.0f : 0.0f;
            break;

        case Operator::GreaterThanOrEqual:
            fVal = (hyst ? (wasOn ? in >= lo : in >= hi) : in >= set) ? 1.0f : 0.0f;
            break;

        case Operator::LessThanOrEqual:
            fVal = (hyst ? (wasOn ? in <= hi : in <= lo) : in <= set) ? 1.0f : 0.0f;
            break;

        case Operator::BitwiseAnd:
            fVal = (uint16_t)(in) & (uint16_t)(set);
            break;

        case Operator::BitwiseNand:
            fVal = ~((uint16_t)(in) & (uint16_t)(set));
            break;

        default:
            fVal = 0;
            break;
    }
}