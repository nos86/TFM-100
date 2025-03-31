#ifndef PT100_H
#define PT100_H

#include <stdint.h>
#include <MAX31865_NonBlocking.h>

class PT100 {
private:
    MAX31865 sensor;
    bool measurementRunning;
    float alpha;
    bool filterInitialized;
    
    

public:
    PT100(int csPin);
    bool begin();
    bool begin(float alpha);
    bool begin(MAX31865::ConvMode mode, float alpha);
    bool runDiagnostics();
    bool triggerMeasurement();
    void process();
    bool errorDetected;
    float last_temperature;
    float average_temperature;
    uint8_t last_fault;
};

#endif // PT100_H
