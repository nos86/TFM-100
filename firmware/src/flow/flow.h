#ifndef FLOW_H
#define FLOW_H

#define DEBOUNCE_TIME 200.0 //mS
#define TICK_BASE_TIME 0.000128 //128uS
#define DEBOUNCE_TICK (DEBOUNCE_TIME / TICK_BASE_TIME / 1000)
#define TIMEOUT_S (float)(TICK_BASE_TIME * (UINT16_MAX + 1))

#include <Arduino.h>

class Flow {
    public:
        Flow(uint8_t tickPerLiter): last_measured_flow_l_h(0), 
                tickPerLiter(tickPerLiter), 
                callback(NULL), 
                last_sample_time(0),
                last_period_s(0.0),
                timeout_s(TIMEOUT_S)
                {};
        void begin();
        void begin(void (*callback)(float));
        uint16_t getFlow();
        void process();

    private:
        uint16_t last_measured_flow_l_h;
        uint8_t tickPerLiter;
        void (*callback)(float);
        uint32_t last_sample_time; 
        float last_period_s;
        float timeout_s;
        uint16_t calculateFlow(float period); // returns l/h
};


#endif // FLOW_H