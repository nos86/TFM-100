#include "flow/flow.h"
#include "config.h"

typedef enum {
    CAPTURE_START = 1,
    CAPTURE_INTERMEDIATE = 2,
    CAPTURE_COMPLETE = 3,
    CAPTURE_OVERRUN = 4,
} capture_step_t;

volatile uint16_t t1 = 0;
volatile uint16_t t2 = 0;
volatile uint16_t last_tick = 0;
volatile capture_step_t step = CAPTURE_START;

void flow_ISR(){
    // Handle the state transitions for the capture process based on the current step.
    uint16_t tick = TCNT1;
    uint16_t delta_tick = tick - last_tick;
    if (delta_tick < 0) delta_tick += UINT16_MAX;
    if (delta_tick< (uint16_t)DEBOUNCE_TICK) return;  // Debounce
    last_tick = tick;
    switch (step){
    case CAPTURE_START:
        t1 = tick;  // Primo fronte
        step = CAPTURE_INTERMEDIATE;
        break;
    case CAPTURE_INTERMEDIATE:
        t2 = tick;  // Secondo fronte
        step = CAPTURE_COMPLETE;
        break;    
    default:
        step = CAPTURE_OVERRUN;
        break;
    }
}

void Flow::begin(){
    TCCR1A = 0;
    TCCR1B = (1 << CS12) | (1 << CS10);  // Prescaler 1024
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flow_ISR, FALLING);
}

// Update the callback type to std::function in the Flow class definition
void Flow::begin(void (*callback)(float)){
    Flow::begin();
    this->callback = callback;
}

uint16_t Flow::calculateFlow(float period){
    return (uint16_t)(3600 * (1.0f / tickPerLiter) / period);
}

void Flow::process(){
    if (step == CAPTURE_COMPLETE) {
        uint16_t ticks;
        if (t2 >= t1) {
            ticks = t2 - t1;  // Nessun overflow
        } else {
            ticks = (65536 - t1) + t2;  // Correzione overflow
        }
        t1 = t2; // Set end-time as start-time
        step = CAPTURE_INTERMEDIATE;
        last_period_s = ((float)ticks * TICK_BASE_TIME);
        last_measured_flow_l_h = calculateFlow(last_period_s);
        last_millis = millis();
        if (callback != NULL) callback(last_measured_flow_l_h);
    } 
    else if ((millis() - last_millis) > (1000 * timeout_s)) {
        last_measured_flow_l_h = 0;
    }
}

uint16_t Flow::getFlow() { // in l/h
    float diff_s = (millis() - last_millis) / 1000.0;
    if ((last_period_s < diff_s) && (diff_s < timeout_s)) {
        return calculateFlow((float)diff_s);
    }else{
        return last_measured_flow_l_h;
    }
}
