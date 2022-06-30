#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <pthread.h>
#include "queue.h"
#include "printlog.h"

struct timespec timeout = {0,250000000}; // Sec, NanoSec

int InitQueue(int& kq)
{
    kq = -1;

    /* create a new kernel event queue */
    if ((kq = kqueue()) == -1)
    {
        printlog(llError, "InitQueue", "kqueue() failed, errno=%d strerror=\"%s\"", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    printlog(llInfo, "InitQueue", "kqueue inited, queue=%d", kq);
    return EXIT_SUCCESS;
}

int ChangeQueueEvent(int& kq, int ident, short filter, unsigned short flags)
{
    if (kq <= 0)
    {
        return EXIT_FAILURE;
    }

    struct kevent change = {0};    /* event we want to monitor */

    EV_SET(&change, ident, filter, flags, 0, 0, 0);
    if (kevent(kq, &change, 1, NULL, 0, NULL))
    {
        printlog(llError, "ChangeQueueEvent", "kevent() failed, attempt to change event failed, errno=%d strerror=\"%s\" queue=%d ident=%d filter=%d(%s) flags=%d(%s,%s)", errno, strerror(errno), kq, ident, filter, filter == EVFILT_READ  ? "EVFILT_READ" : filter == EVFILT_WRITE ? "EVFILT_WRITE" : "UNKNOWN", flags, flags & EV_ADD ? "EV_ADD": flags & EV_DELETE ? "EV_DELETE": "*", flags & EV_DISABLE ? "EV_DISABLE": flags & EV_ENABLE ? "EV_ENABLE": "*");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int ChangeQueueEvent_Safe(pthread_mutex_t *lock, int& kq, int ident, short filter, unsigned short flags)
{
    int res = EXIT_FAILURE;
    pthread_mutex_lock(lock);
    {
        res = ChangeQueueEvent(kq, ident, filter, flags);
    }
    pthread_mutex_unlock(lock);
    return res;
}


int GetQueueEvent(int& kq, struct kevent *event, int &event_count)
{
    if (kq <= 0)
        return EXIT_FAILURE;

    event_count = kevent(kq, NULL, 0, event, 1, &timeout);
    if (event_count < 0)
    {
        printlog(llError, "GetQueueEvents", "kevent() failed, queue=%d errno=%d strerror=\"%s\"", kq, errno, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int GetQueueEvent_Safe(pthread_mutex_t *lock, int& kq, struct kevent *event, int &event_count)
{
    int res = EXIT_FAILURE;
    if (pthread_mutex_trylock(lock) == 0)
    {
        res = GetQueueEvent(kq, event, event_count);
        if (res == EXIT_SUCCESS && event_count == 1)
            res = ChangeQueueEvent(kq, event->ident, event->filter, EV_DISABLE);

        pthread_mutex_unlock(lock);
    }
    else
    {
        event_count = 0;
        res = EXIT_SUCCESS;
    }
    return res;
}

void CloseQueue(int& kq)
{
    int lkq = kq;
    kq = -1;
    close(lkq);
    printlog(llInfo, "CloseQueue", "kqueue closed, queue=%d", lkq);
}
