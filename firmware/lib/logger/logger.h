#ifndef LOGGER_H
#define LOGGER_H

bool AddMessageToLog(String msg, bool status, bool inner);
bool AddMessageToLog(String msg, bool status);
void AddInfoToLog(String msg, bool inner);
void AddInfoToLog(String msg);


#endif