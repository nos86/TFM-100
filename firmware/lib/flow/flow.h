#ifndef FLOW_H
#define FLOW_H

/** Minimum time between two accepted pulses (ms) – rejects EMI glitches. */
#define DEBOUNCE_TIME_MS 200UL

/**
 * After this many seconds without a pulse, flow is reported as zero and the
 * state machine resets so the next pulse pair re-confirms the flow.
 * 30 s is sufficient for flows down to ~30 l/h (4 ticks/litre).
 */
#define TIMEOUT_S  30.0f
#define TIMEOUT_MS ((uint32_t)(TIMEOUT_S * 1000.0f))

/**
 * Low-pass filter time constant (s).
 * Models the hydraulic inertia of the circuit; pump max = 4 m³/h.
 * A ~5 s TC strongly attenuates single-shot EMI spikes while still
 * tracking genuine ramp-up/ramp-down in a few seconds.
 */
#define FLOW_LPF_TC_S 5.0f

#include <Arduino.h>

class Flow
{
public:
    Flow(uint8_t tickPerLiter) : last_measured_flow_l_h(0),
                                 filtered_flow_l_h(0.0f),
                                 tickPerLiter(tickPerLiter),
                                 callback(NULL),
                                 last_millis(0),
                                 last_period_s(0.0f),
                                 timeout_s(TIMEOUT_S) {};
    void begin();
    void begin(void (*callback)(float));
    uint16_t getFlow();
    bool isFlowing();
    void process();

private:
    uint16_t last_measured_flow_l_h;
    float filtered_flow_l_h; /**< EMA filter state (l/h) */
    uint8_t tickPerLiter;
    void (*callback)(float);
    uint32_t last_millis;
    float last_period_s;
    float timeout_s;
    uint16_t calculateFlow(float period); // returns l/h
};

#endif // FLOW_H