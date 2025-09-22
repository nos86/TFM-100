
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#ifndef NUM_TASKS
#define NUM_TASKS 10
#endif

typedef struct
{
    void (*taskFunc)(uint32_t);
    uint32_t period;
    uint32_t lastRun;
} Task;

class Scheduler
{
public:
    Scheduler() : numTasks(0), systemTime(0), last_millis(0) {}
    uint8_t addTask(void (*taskFunc)(uint32_t), uint32_t period);
    bool removeTask(uint8_t taskId);
    void run();
    uint32_t getUptime() { return (uint32_t)(systemTime / 1000); }

private:
    Task tasks[NUM_TASKS];
    uint8_t numTasks;
    uint64_t systemTime;
    uint32_t last_millis;
};

#endif