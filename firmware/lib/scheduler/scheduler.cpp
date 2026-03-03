#include <scheduler.h>
#include <Arduino.h>

uint8_t Scheduler::addTask(void (*taskFunc)(uint32_t), uint32_t period)
{
    if (numTasks < NUM_TASKS)
    {
        tasks[numTasks].taskFunc = taskFunc;
        tasks[numTasks].period = period;
        tasks[numTasks].lastRun = millis();
        numTasks++;
        return numTasks - 1;
    }
    return 255;
}

void Scheduler::run()
{
    uint32_t currentTime = millis();
    uint32_t delta;
    if (currentTime >= last_millis)
    {
        delta = currentTime - last_millis;
    } else {
        // Handle millis() overflow
        delta = (UINT32_MAX - last_millis) + currentTime + 1;
    }
    systemTime += delta;
    last_millis = currentTime;
    for (uint8_t i = 0; i < numTasks; i++)
    {
        uint32_t delta = currentTime - tasks[i].lastRun;
        if (delta >= tasks[i].period)
        {
            tasks[i].taskFunc(delta);
            tasks[i].lastRun = currentTime;
        }
    }
}