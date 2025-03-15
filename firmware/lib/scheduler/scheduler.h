
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#ifndef NUM_TASKS
    #define NUM_TASKS 10
#endif

typedef struct {
    void (*taskFunc)(uint32_t);
    uint32_t period;
    uint32_t lastRun;
} Task;

class Scheduler {
    public:
        Scheduler() : numTasks(0) {}
        void addTask(void (*taskFunc)(uint32_t), uint32_t period);
        void run();
    private:
        Task tasks[NUM_TASKS];
        uint8_t numTasks;
};

#endif