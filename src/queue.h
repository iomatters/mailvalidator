#ifndef QUEUE_H
#define QUEUE_H

#include <sys/event.h>

int InitQueue(int&);
int ChangeQueueEvent(int& kq, int ident, short filter, unsigned short flags);
int ChangeQueueEvent_Safe(pthread_mutex_t *lock, int& kq, int ident, short filter, unsigned short flags);
int GetQueueEvent(int& kq, struct kevent *event, int &event_count);
int GetQueueEvent_Safe(pthread_mutex_t *lock, int& kq, struct kevent *event, int &event_count);
void CloseQueue(int&);

#endif
