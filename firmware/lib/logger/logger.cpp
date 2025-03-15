#include <Arduino.h>

bool AddMessageToLog(String msg, bool status, bool inner){
    if (inner){
        Serial.print("  ");
    }
    Serial.print("[");
    if (status){
        Serial.print(" DONE ");
    } else {
        Serial.print("FAILED");
    }
    Serial.print("] ");
    Serial.println(msg);
    return status;
}

bool AddMessageToLog(String msg, bool status){
    return AddMessageToLog(msg, status, false);
}

void AddInfoToLog(String msg, bool inner){
    if (inner){
        Serial.print("  ");
    }
    Serial.print("[ INFO ] ");
    Serial.println(msg);
}

void AddInfoToLog(String msg){
    AddInfoToLog(msg, false);
}
