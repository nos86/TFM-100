#ifndef STATUS_H
#define STATUS_H



typedef enum{
    SETUP  = 0x00,
    RUN   = 0x01,
    SLEEP = 0x7F,
    STOP  = 0xFF,
} NodeStatus_t;



#endif // STATUS_H