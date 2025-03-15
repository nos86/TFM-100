#ifndef PT100_H
#define PT100_H

#include <Arduino.h>
#include <MAX31865_NonBlocking.h>

class PT100 {
private:
    MAX31865 sensor;
    bool measurementRunning;
    
    

public:
    PT100(int csPin);
    bool begin();
    bool begin(MAX31865::ConvMode mode);
    bool runDiagnostics();
    bool triggerMeasurement();
    void process();
    bool errorDetected;
    float last_temperature;
    uint8_t last_fault;
};

#endif // PT100_H
