#include <flow.h>
#include "config.h"

typedef enum
{
    CAPTURE_START = 1,
    CAPTURE_INTERMEDIATE = 2,
    CAPTURE_COMPLETE = 3,
} capture_step_t;

volatile uint32_t t1_ms = 0;
volatile uint32_t t2_ms = 0;
volatile uint32_t last_pulse_ms = 0;
volatile capture_step_t step = CAPTURE_START;

void flow_ISR()
{
    uint32_t now_ms = millis();

    // Debounce: reject pulses that arrive faster than DEBOUNCE_TIME_MS.
    // This filters EMI glitches and mechanical contact bounce.
    if ((now_ms - last_pulse_ms) < DEBOUNCE_TIME_MS)
        return;
    last_pulse_ms = now_ms;

    switch (step)
    {
    case CAPTURE_START:
        t1_ms = now_ms;
        step = CAPTURE_INTERMEDIATE;
        break;
    case CAPTURE_INTERMEDIATE:
        t2_ms = now_ms;
        step = CAPTURE_COMPLETE;
        break;
    case CAPTURE_COMPLETE:
        // process() has not yet consumed the previous pair.
        // Slide the window forward so we always hold the most-recent interval
        // and the state machine never gets stuck waiting for process().
        t1_ms = t2_ms;
        t2_ms = now_ms;
        // step stays CAPTURE_COMPLETE – fresh data is ready for process()
        break;
    }
}

void Flow::begin()
{
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flow_ISR, FALLING);
}

// Update the callback type to std::function in the Flow class definition
void Flow::begin(void (*callback)(float))
{
    Flow::begin();
    this->callback = callback;
}

uint16_t Flow::calculateFlow(float period)
{
    return (uint16_t)(3600.0f * (1.0f / tickPerLiter) / period);
}

void Flow::process()
{
    if (step == CAPTURE_COMPLETE)
    {
        // Atomically snapshot the ISR timestamps and clear the ready flag.
        noInterrupts();
        uint32_t ts1 = t1_ms;
        uint32_t ts2 = t2_ms;
        // Advance the ISR window start so the next interval begins at the last pulse.
        t1_ms = ts2;
        step = CAPTURE_INTERMEDIATE;
        interrupts();

        // millis() subtraction is correct even when the 32-bit counter wraps.
        uint32_t period_ms = ts2 - ts1;
        float period_s = (float)period_ms / 1000.0f;

        if (period_s > 0.0f)
        {
            uint16_t raw_flow = calculateFlow(period_s);

            // Time-varying EMA low-pass filter.
            // alpha = dt / (TC + dt) is the bilinear-equivalent of a
            // continuous RC filter with time-constant FLOW_LPF_TC_S.
            // For rapid pulses (short dt, high flow / potential noise) alpha
            // is small → the old filtered value dominates → stronger noise
            // rejection.  For slow pulses (long dt, low flow) alpha approaches
            // 1 → the filter tracks changes quickly.  This is the expected
            // RC-filter behaviour at variable sample rates.
            float alpha = period_s / (FLOW_LPF_TC_S + period_s);
            filtered_flow_l_h = alpha * (float)raw_flow + (1.0f - alpha) * filtered_flow_l_h;

            last_period_s = period_s;
            last_measured_flow_l_h = (uint16_t)filtered_flow_l_h;
            last_millis = millis();
            if (callback != NULL)
                callback(last_measured_flow_l_h);
        }
    }
    else if ((millis() - last_millis) > TIMEOUT_MS)
    {
        last_measured_flow_l_h = 0;
        filtered_flow_l_h = 0.0f;
        // Reset to CAPTURE_START so the next restart requires two pulses to
        // confirm real flow before a rate is reported (first-pulse confirmation).
        noInterrupts();
        step = CAPTURE_START;
        interrupts();
    }
}

uint16_t Flow::getFlow()
{ // in l/h
    float diff_s = (millis() - last_millis) / 1000.0f;
    if ((last_period_s < diff_s) && (diff_s < timeout_s))
    {
        return calculateFlow((float)diff_s);
    }
    else
    {
        return last_measured_flow_l_h;
    }
}

bool Flow::isFlowing()
{
    return (getFlow() > 0);
}
