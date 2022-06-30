#ifndef PRINTLOG_H
#define PRINTLOG_H

#define DIR_LOGS "logs"
#define FILE_LOG_POSTFIX ".log"

typedef enum _LogLevel
{
    //llDebug,
    llInfo,
    llWarning,
    llError,
    llFatal
} LogLevel;

void initlog(bool *b_logon, const char* info, const char* path);
void printlog(LogLevel const level, char const *const context, char const *const fmt_message, ...);
void closelog(void);

#endif
