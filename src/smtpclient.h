#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#define SMTP_PORT 25
#define MAX_IDLE_COUNT 200 

#define STATE_UNDEFINED 0
#define STATE_EHLOSENT 1 
#define STATE_MAILFROMSENT 3 
#define STATE_RCPTTOSENT 7
#define STATE_CATCHALLSENT 8 
#define STATE_SUCCESSFUL 15 

#define CATCH_ALL_ADDRESS "mailb0xd0esn0texist"

struct SMTP_SESSION_SET
{
  char ptr_domain[256];
  char mail_from[256];
  char rcpt_domain[256];
  int sock_async_timeout;
  unsigned char catch_all_check; 
};

int SmtpCheckRcpt(SMTP_SESSION_SET*, char*, char*);

#endif
