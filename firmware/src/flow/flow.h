#ifndef FLOW_H
#define FLOW_H

#define DEBOUNCE_TIME 200.0 //mS
#define TICK_BASE_TIME 0.000128 //128uS
#define DEBOUNCE_TICK (DEBOUNCE_TIME / TICK_BASE_TIME / 1000)

#include <Arduino.h>

class Flow {
    public:
        Flow(uint8_t tickPerLiter): last_flow(0), 
                tickPerLiter(tickPerLiter), 
                callback(NULL){};
        void begin();
        void begin(void (*callback)(float));
        float getFlow();
        void process();

    private:
        float last_flow;
        uint8_t tickPerLiter;
        void (*callback)(float);
};


#endif // FLOW_H