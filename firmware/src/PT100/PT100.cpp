#include "PT100.h"

// Constructor for the PT100 class.
// Initializes the sensor object.
PT100::PT100(int csPin) : sensor(csPin), measurementRunning(false), errorDetected(false), last_temperature(0.0), last_fault(0) {
    // Constructor implementation
    filterInitialized = false;
    alpha = 1;
    average_temperature = 0;
}

bool PT100::begin() {
    return begin(MAX31865::CONV_MODE_SINGLE);
}

bool PT100::begin(float alpha) {
    return begin(MAX31865::CONV_MODE_CONTINUOUS, alpha);
}

bool PT100::begin(MAX31865::ConvMode mode, float alpha) {
    sensor.begin(MAX31865::RTD_2WIRE, MAX31865::FILTER_50HZ, mode); // Initialize the MAX31865 sensor in 2-wire mode
    if (alpha <=0 || alpha > 1) alpha = 1; // Check if alpha is in the range ]0, 1]
    this->alpha = alpha;
    if (runDiagnostics()){
        last_fault = sensor.getFault();
        return (last_fault & MAX31865::FAULT_OVUV_BIT) == 0;
    }
    return false;
}

bool PT100::runDiagnostics(){
    uint32_t now = millis();
    sensor.setFaultCycle(MAX31865::FAULT_AUTO_RUN);
    while(sensor.getFaultCycle() != MAX31865::FAULT_STATUS_FINISHED){
        if (millis() - now > 1000) return false;
    }
    delay(10);
    return true;
}

void PT100::process() {
    if (measurementRunning) {
        if (sensor.isConversionComplete()) {
            measurementRunning = false;
            last_fault = sensor.getFault();
            errorDetected = last_fault > 0;
            if (!errorDetected) {
                last_temperature = sensor.getTemperature(100, 400, MAX31865::CALC_POLY_SIMPLE); // Read temperature with 100 ohm RTD and 400 ohm reference resistor
                if (!filterInitialized) {
                    average_temperature = last_temperature;
                    filterInitialized = true;
                } else {
                    average_temperature = alpha * last_temperature + (1 - alpha) * average_temperature;
                }
            }
            
        }
    }
        
}

bool PT100::triggerMeasurement() {
    if (measurementRunning) return false;
    measurementRunning = true;
    return true;
}
