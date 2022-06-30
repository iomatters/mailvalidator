#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include "printlog.h"

char dir_path_log[FILENAME_MAX];
char file_path_log[FILENAME_MAX];
char main_file_name[FILENAME_MAX];

FILE* file_log = NULL;
pthread_mutex_t lock_log = NULL;
int daylog = 0;	   /* day of month (1 -	31) */
bool bLogON = false;

int mkdirlog(const char* path)
{
    int res = EXIT_SUCCESS;
    struct stat	sb;

    if (stat(path, &sb) != 0)
    {
        if (mkdir(path, 0777) != 0 && errno != EEXIST)
            res = EXIT_FAILURE;
    }
    else if (!S_ISDIR(sb.st_mode))
    {
        errno = ENOTDIR;
        res = EXIT_FAILURE;
    }
    return res;
}

void fopenlog(void)
{
    if (mkdirlog(dir_path_log) == EXIT_SUCCESS)
    {
        char timebuf[64];
        time_t now = time(NULL);
        struct tm* gmtnow = gmtime(&now);
        strftime(timebuf, sizeof(timebuf), "%F", gmtnow);

        // day to check start of new day
        daylog = gmtnow->tm_mday;

        sprintf(file_path_log, "%s/%s-%s%s", dir_path_log, timebuf, main_file_name, FILE_LOG_POSTFIX);
        file_log = fopen(file_path_log, "a+");
        if (file_log == NULL)
        {
            printf("open log file failed - log to stdout, path=\"%s\" errno=%d strerror=\"%s\"\r\n", file_path_log, errno, strerror(errno));
            file_log = stdout;
        }
    }
    else
    {
        printf("make log dir failed - log to stdout, path=\"%s\" errno=%d strerror=%s\r\n", dir_path_log, errno, strerror(errno));
        file_log = stdout;
    }

}

void fcloselog(void)
{
    if (file_log != NULL && file_log != stdout)
        fclose(file_log);

    file_log = NULL;
}

void initlog(bool *b_logon, const char* info, const char* path)
{
    bLogON = b_logon;
    if (bLogON != true)
	return; 

    // save paths
    {
        char* file_name = basename(path);
        char* dir_name = dirname(path);

        // save main file name
        sprintf(main_file_name, "%s", file_name == NULL ? "log" : file_name);

        // make & save log dir path
        sprintf(dir_path_log, "%s/%s", dir_name == NULL ? "." : dir_name, DIR_LOGS);
    }


    // init
    {
        fopenlog();
        pthread_mutex_init(&lock_log, NULL);
        printlog(llInfo, "initlog", "------- ------- ------- log started - %s (%s)", info, path);
    }
}

void printlog(LogLevel const level, char const *const context, char const *const fmt_message, ...)
{
    if (bLogON != true)
	return;

    if (pthread_mutex_lock(&lock_log) == 0)
    {
        // get GMT time
        char timebuf[64];
        struct timeval time_now;
        gettimeofday(&time_now, NULL);
        struct tm* gmtnow = gmtime(&(time_now.tv_sec));

        // check start of new day
        if (daylog != gmtnow->tm_mday)
        {
            fcloselog();
            fopenlog();
        }

        // time
        strftime(timebuf, sizeof(timebuf), "%F %T", gmtnow);
        fprintf(file_log, "%s.%06ld\t", timebuf, time_now.tv_usec);

        // thread ID
        pthread_t th_id = pthread_self();
        fprintf(file_log, "%p\t", (void*)th_id);

        // log level
        switch (level)
        {
        case llInfo:
            fprintf(file_log, "lInfo\t");
            break;
        case llWarning:
            fprintf(file_log, "lWarning\t");
            break;
        case llError:
            fprintf(file_log, "lError\t");
            break;
        case llFatal:
            fprintf(file_log, "lFatal\t");
            break;
        default:
            fprintf(file_log, "l%d\t", level);
            break;
        }

        // context
        fprintf(file_log, "%s\t", context);

        // message
        va_list ap;
        va_start(ap, fmt_message);
        vfprintf(file_log, fmt_message, ap);
        va_end (ap);

        // trailing newline
        fprintf(file_log, "\r\n");

        fflush(file_log);

        pthread_mutex_unlock(&lock_log);
    }
}

void closelog(void)
{
    printlog(llInfo, "closelog", "------- ------- ------- log closed");
    pthread_mutex_destroy(&lock_log);
    lock_log = NULL;
    fcloselog();
}
