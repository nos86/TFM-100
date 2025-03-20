#include <scheduler.h>
#include <Arduino.h>

void Scheduler::addTask(void (*taskFunc)(uint32_t), uint32_t period) {
    if (numTasks < NUM_TASKS) {
        tasks[numTasks].taskFunc = taskFunc;
        tasks[numTasks].period = period;
        tasks[numTasks].lastRun = millis();
        numTasks++;
    }
}

void Scheduler::run()
{
    uint32_t currentTime = millis();
    uint32_t delta = currentTime - last_millis;
    if (delta < 0) {
        systemTime += (0xFFFFFFFF - last_millis);
    }
    systemTime += delta;
    last_millis = currentTime;
    for (uint8_t i = 0; i < numTasks; i++) {
        uint32_t delta = currentTime - tasks[i].lastRun;
        if (delta >= tasks[i].period) {
            tasks[i].taskFunc(delta);
            tasks[i].lastRun = currentTime;
        }
    }
}