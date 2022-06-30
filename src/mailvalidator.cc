#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex.h>
#include "mailvalidator.h"
#include "printlog.h"
#include "mxlookup.h"
#include "sckt.h"
#include "smtpclient.h"
#include "textsearch.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

typedef enum _ErrCode
{
  errTerminated = 13,
  errSuccess = 250,
  errInvalidFormat = 5000,
  errCantResolve = 5001,
  errMXLookup = 5002,
  errMXLimitExceeded = 5003,
  errDomainBlacklisted = 5004,
  errIdleTimeExceeded = 5005,
  errGetHostByName = 5006,
  errCatchAllDetected = 5007
} ErrorCode;

bool bShutdown = false;
bool bFileMode = false;
bool bFileLog = false;
bool bAquery = false;
bool bNameServer = false;

// Global settings for SMTP negotiation  
SMTP_SESSION_SET SmtpSessionSet = {"mail.educause.edu", "admin@educause.edu", "", 15, 0};

char szNameServer[16] = "8.8.8.8";
char szRcptTo[256] = "";
char szFileName[1024] = "";
char szXFileName[1024] = ""; 

int nSleep = 100;
int nMXLimit = 1;
int nFileSize = 500;

// For A and MX records separately
DNS_RECORDS mxBuf, aBuf;

#ifdef DEBUG
char* BytesToStr(char* dst, int size, void* bytes, int count)
{
    unsigned char* src = (unsigned char*)bytes;
    char* buf = dst;

    for(int i = 0, j = 0; i < count; i += 16)
    {
        if ((buf + 80) >= (dst + size))
        {
            buf += sprintf(buf, "(data is too large, skip...) ");
            break;
        }
        buf += sprintf(buf, "%08X: ", i);
        for (j = 0; j < 16 ; j++)
        {
            if ((i + j) < count)
                buf += sprintf(buf, "%02X ", src[i + j]);
            else
                buf += sprintf(buf, "   ");
            if (j == 7) buf += sprintf(buf, "| ");
        }
        buf += sprintf(buf, " ");
        for (j = 0; j < 16 && (i + j) < count; j++)
            buf += sprintf(buf, "%c", src[i + j] >= 32 ? src[i + j] : ' ');
       if ((i + 16) < count)
            buf += sprintf(buf, "\r\n");
    }
    *(buf) = '\0';
    return dst;
}
#endif

//
// Get the DNS servers from /etc/resolv.conf file on Linux
//
void GetNameserver()
{
    if (!bNameServer) {
    	FILE *fp;
    	char line[200] , *p;
  
    	if ((fp = fopen("/etc/resolv.conf" , "r")) == NULL) 
        	printlog(llWarning, "GetNameserver", "Failed opening /etc/resolv.conf file");
    	else {
      		while(fgets(line , 200 , fp)) {
          		if(line[0] == '#') 
              			continue;

          		if(strncmp(line , "nameserver" , 10) == 0) {
              			p = strtok(line , " ");
	       			p = strtok(NULL , " ");
              			strcpy(szNameServer, p);
	      		 	break;	
        		 }
      		}
    	}

    }
    //szNameServer[strlen(szNameServer)] = '\0';
    printlog(llInfo, "GetNameserver", "Nameserver to be used=%s", szNameServer);
}


int GetHostByName(const char *fqdn, char* ip) {
#ifndef STDDNSQUERY 
	ngethostbyname(aBuf, (unsigned char*)fqdn, szNameServer, T_A);
	if (aBuf.len == 0) {
             return EXIT_FAILURE;  		
	}
	ip[strlen(aBuf.record[0])] = '\0';	
	strncpy (ip, aBuf.record[0], strlen(aBuf.record[0]));

#else
        struct hostent *hp;
        struct in_addr **addr_list;

        hp = gethostbyname(fqdn);
        if (hp == NULL)
                return EXIT_FAILURE;
         else {
                addr_list = (struct in_addr **)hp->h_addr_list;
		strncpy (ip, inet_ntoa(*addr_list[0]), strlen(inet_ntoa(*addr_list[0])));
  	        ip[strlen(inet_ntoa(*addr_list[0]))] = '\0'; 
        }
#endif
        return EXIT_SUCCESS;
}

int ParseArguments(int argc, char *argv[])
{
    // check count
    if (argc < 2)
    {
        printf("Usage: mailvalidator [@server] [-a] [-h] [-l] [-flist filename] [-fsize filesize] [-xlist filename] [-d sleep msec.]\n\t[-t socket timeout sec.] [-mx mail servers limit] [-catchall] [-p ptr record] [-s mailfrom] [rcptto]\r\n");
        printlog(llFatal, "ParseArguments", "1 argument expected, mailvalidator [rcpt to]");
        return -1;
    }

    // parse arguments
    for (int i = 1; i < argc; i++)
    {
        if (memcmp(argv[i], "-flist", 7) == 0)
        {
            strcpy(szFileName, argv[i] + 7); 
	    bFileMode = true;  i++; 
	}
        else if (memcmp(argv[i], "-l", 3) == 0)
        {
            bFileLog = true; 
        }
        else if (memcmp(argv[i], "-catchall", 10) == 0)
        {
            SmtpSessionSet.catch_all_check = 1;
        }
        else if (memcmp(argv[i], "-d", 3) == 0)
        {
            nSleep = atoi(argv[i] + 3); i++; 
        }
        else if (memcmp(argv[i], "-t", 3) == 0)
        {
            SmtpSessionSet.sock_async_timeout = atoi(argv[i] + 3); i++;
        }
        else if (memcmp(argv[i], "-mx", 4) == 0)
        {
            nMXLimit = atoi(argv[i] + 4); i++; 
        }
	else if (memcmp(argv[i], "-s", 3) == 0)
        {
	    strcpy(SmtpSessionSet.mail_from, argv[i] + 3); i++;
	}
        else if (memcmp(argv[i], "-p", 3) == 0)
        {
            strcpy(SmtpSessionSet.ptr_domain, argv[i] + 3); i++;
        }    
        else if (memcmp(argv[i], "-a", 3) == 0)
        {
            bAquery = true;
        }
        else if (memcmp(argv[i], "-fsize", 7) == 0)
        {
            nFileSize = atoi(argv[i] + 7); i++;
        }
        else if (memcmp(argv[i], "@", 1) == 0)
        {
            strcpy(szNameServer, argv[i] + 1); 
	    bNameServer = true;
        }
        if (memcmp(argv[i], "-xlist", 7) == 0)
        {
            strcpy(szXFileName, argv[i] + 7); i++;
        }
	else if (memcmp(argv[i], "-h", 3) == 0)
        {
            printf("\r\nName:\r\n");
            printf("\t%s\r\n", _PROGNFO_);
	    printf("\r\nDescription:\r\n");
	    printf("\tThe mailvalidator utility perfoms email address validation tests such as MX servers existence and 250 SMTP reply code.\r\n");	
	    printf("\tEmail(es) for validation can be provided in the form of a single argument or a plain-text file containing list of emails.\r\n");
            printf("\tNameservers from etc/resolv.conf are used or 8.8.8.8 by default.\r\n");
	    printf("\r\nUsage:\r\n");
	    printf("\tmailvalidator [@server] [-a] [-h] [-l] [-f filename] [-fsize filesize] [-xlist filename] [-d delay msec.]\n\t[-t socket timeout sec.] [-mx mail servers number] [-catchall] [-p ptr record] [-s mailfrom] [rcptto]\r\n");
	    printf("\r\nOptions:\r\n");
	    printf("\t-h\r\n");
	    printf("\t\tOutput this help screen.\r\n");
            printf("\t@server\r\n");
            printf("\t\tIP address of the name server to query, if not stated and resolve.conf is unusable, then 8.8.8.8 to be used by default.\r\n");	   
            printf("\t-a\r\n");
            printf("\t\tRun FQDN A record test (please note, FQDN without A record can still have MX records), default OFF.\r\n");
            printf("\t-catchall\r\n");
            printf("\t\tRun the catch-all test, default OFF.\r\n");
	    printf("\t-d NUM\r\n");
            printf("\t\tDelay in msec. to be set while processing a list of emails, default 100.\r\n");    
            printf("\t-flist filename\r\n");
            printf("\t\tFile name containing a list of emails.\r\n");
	    printf("\t-fsize NUM\r\n");
            printf("\t\tLines (i.e. emails) number limit to be used in file mode, default 500.\r\n"); 
	    printf("\t-l\r\n");
	    printf("\t\tEnable logging, default OFF.\r\n");       
            printf("\t-mx NUM\r\n");
            printf("\t\tMX servers number to be attempted, default 1.\r\n");	    
            printf("\t-p ptr record\r\n");
            printf("\t\tPTR record to be used for EHLO command, default educause.edu.\r\n");
	    printf("\t-s mailfrom\r\n");
            printf("\t\tMAIL FROM to be used while checking 250 SMTP reply code, default admin@educause.edu.\r\n");
	    printf("\t-t NUM\r\n");
            printf("\t\tSocket timeout in sec. to be set while establishing connectivity, default 15.\r\n");
            printf("\t-xlist filename\r\n");
            printf("\t\tFilename containig blacklisted domains, to be used excluded from validation.\r\n");
	    printf("\r\nArguments:\r\n");
            printf("\trcptto\r\n");
	    printf("\t\tEmail address to be verified.\r\n");
	    return EXIT_FAILURE;
        }
	else  
        {
	    strcpy(szRcptTo, argv[i]);
        }
    }

    return EXIT_SUCCESS;
}

int ParseEmail(char* email)
{
   regex_t preg;
   regmatch_t svm[ 2 ];
   int rc = 0;
 
   if ( (rc = regcomp( &preg, EMAIL_REGEXP, REG_EXTENDED)) != 0 )
   {
        printlog(llWarning, "ParseEmail", "Regexp init failed, email=%s", email);
        return errInvalidFormat;
   }
 
   if ((rc =  regexec( &preg, email, 2, svm, 0 )) != 0 )
   {
       if (rc == REG_NOMATCH)
       {
          printlog(llWarning, "ParseEmail", "Invalid rcpt format, email=%s", email);
          return errInvalidFormat;
       }
       else
       { 
	  printlog(llWarning, "ParseEmail", "Regexp exec failed, email=%s", email);
          return errInvalidFormat;
       }
   }
   regfree(&preg);

   int nPos = strcspn(email, "@"); 
   nPos++;
   strcpy(SmtpSessionSet.rcpt_domain, email+nPos);
   if (strlen(szXFileName) > 0)
   {
	char pattern[256];
        sprintf(pattern, "\n%s\n", SmtpSessionSet.rcpt_domain); // exact match
	if (SearchInFile(szXFileName, pattern))
	{	
	    printlog(llWarning, "ParseEmail", "FQDN <%s> is blacklisted, email=%s", SmtpSessionSet.rcpt_domain,  email);
            return errDomainBlacklisted;
	}
   }

   // if check A record option given
   char mxIP[16];
   if (bAquery)
   {
        if (GetHostByName(SmtpSessionSet.rcpt_domain, &mxIP[0]) != 0)
        {	
             printlog(llWarning, "ParseEmail", "Failed on resolving FQDN, FQDN=%s", SmtpSessionSet.rcpt_domain);
             return errCantResolve;		
        }
        printlog(llInfo, "ParseEmail", "FQDN <%s> has IP=%s", SmtpSessionSet.rcpt_domain, mxIP);
    }

   int retCode = 0;
   // TODO
   // char* tmp = (char *)malloc(strlen(SmtpSessionSet.rcpt_domain));
   // strcpy(tmp, SmtpSessionSet.rcpt_domain); 
   ngethostbyname(mxBuf, (unsigned char*)SmtpSessionSet.rcpt_domain, szNameServer, T_MX);
   sprintf(&SmtpSessionSet.rcpt_domain[strlen(SmtpSessionSet.rcpt_domain) - 1], "%s", "\0");

   if (mxBuf.len == 0)
   {
	printlog(llWarning, "ParseEmail", "Failed on retrieving MX record(s), email=%s, FQDN=%s", email, SmtpSessionSet.rcpt_domain);
	retCode = errMXLookup;
   }
   printlog(llInfo, "ParseEmail", "FQDN <%s> has %d MX record(s), email=%s", SmtpSessionSet.rcpt_domain, mxBuf.len, email);

   int i = 0;
   for (i=0;i<mxBuf.len;i++)
   {
	if (i == (nMXLimit))
	{
	    retCode = errMXLimitExceeded; 
	    printlog(llWarning, "ParseEmail", "MX servers limit exceeded, FQDN=%s, email=%s", mxBuf.record[i], email);
	    break;
	}
        if (GetHostByName(mxBuf.record[i], &mxIP[0]) != 0)
        {
	    printlog(llWarning, "ParseEmail", "Failed on resolving FQDN, FQDN=%s, email=%s", mxBuf.record[i], email);
	    retCode = errGetHostByName;
            continue;
        } 
	printlog(llInfo, "ParseEmail", "email=%s, FQDN=%s MX%d=%s, IP=%s", email, SmtpSessionSet.rcpt_domain, i, mxBuf.record[i], mxIP);

	retCode=SmtpCheckRcpt(&SmtpSessionSet, email, mxIP);

 	bool bCompleted = ((retCode == 250)  || (retCode == 251) || (retCode == 450) || (retCode == 451) ||  (retCode == 550) || (retCode == 553) || (retCode == 5005) || (retCode == 5007)) ? true : false;	
 	if (bCompleted)
		break;
   }

  return retCode; 
}    

void on_terminate(int sig)
{
    printlog(llInfo, "on_terminate", "SIGNAL! signal %d caught; exit...", sig);
    printf("\r\nsignal %d caught\r\nexit...\r\n\r\n", sig);
    bShutdown = true;
}

void PrintErrCode(char* rcptto, int errcode)
{
   char str[256]; 
   switch(errcode)
   {
	case errInvalidFormat:
           strcpy(str, "_ERR: Invalid RCPT format");	
	   break;
	case errCantResolve:
           strcpy(str, "_ERR: FQDN cant be resolved");	
	   break;
        case errMXLookup:
           strcpy(str, "_ERR: No MX records found");
           break;
	case errSuccess:
 	   strcpy(str, "_OK: Valid"); 
	   break;
	case errMXLimitExceeded:
	   strcpy(str, "_ERR: MX servers limit exceeded");
           break;
	case errTerminated:
           strcpy(str, "_ERR: Program has been terminated");
           break;
	case errDomainBlacklisted: 
           strcpy(str, "_ERR: Domain is blacklisted");
           break;
        case errIdleTimeExceeded:
           strcpy(str, "_ERR: Idle time limit exceeded - MX not responding");
           break;
	case errGetHostByName:
	   strcpy(str, "_ERR: GetHostByName - no A records returned");
           break;
        case errCatchAllDetected:
           strcpy(str, "_ERR: Catch-all server detected");
           break;
        case 252:
           strcpy(str, "_ERR: Cannot VRFY user, but will accept message and attempt delivery");
           break;
	case 421:
           strcpy(str, "_ERR: Service not available, closing transmission channel");
           break;
	case 450:
           strcpy(str, "_ERR: Requested mail action not taken: mailbox unavailable");
           break;
	case 451:
           strcpy(str, "_ERR: Requested action aborted: local error in processing");
           break;
	case 452:
           strcpy(str, "_ERR: Requested action not taken: insufficient system storage");
           break;
	case 500:
           strcpy(str, "_ERR: Syntax error, command unrecognised");
           break;
	case 501:
           strcpy(str, "_ERR: Syntax error in parameters or arguments");
           break;
	case 502:
           strcpy(str, "_ERR: Command not implemented");
           break;
	case 503:
           strcpy(str, "_ERR: Bad sequence of commands");
           break;
	case 504:
           strcpy(str, "_ERR: Command parameter not implemented");
           break;
	case 521:
           strcpy(str, "_ERR: <domain> does not accept mail (see rfc1846)");
           break;
	case 530:
           strcpy(str, "_ERR: Access denied");
           break;
	case 550:
           strcpy(str, "_ERR: Requested action not taken: mailbox unavailable");
           break;
	case 551:
           strcpy(str, "_ERR: User not local; please try <forward-path>");
           break;
	case 552:
           strcpy(str, "_ERR: Requested mail action aborted: exceeded storage allocation");
           break;
	case 553:
           strcpy(str, "_ERR: Requested action not taken: mailbox name not allowed");
           break;
	case 554:
           strcpy(str, "_ERR: Transaction failed");
           break;
	default:
	   strcpy(str, "_ERR: Unrecognized error code");
	   break;
    }
    printlog(llInfo, "PrintErrorCode", "SMTP reply code=%d for email address=%s", errcode, rcptto);
    printf("%s\t%d\t%s\r\n", rcptto, errcode, str);
}

int main(int argc, char *argv[])
{
	int nRetCode = 0;


	// parse arguments
	if(ParseArguments(argc, argv) != 0)
	   return -1;

        // printf(_PROGNFO_", logging %s...\n", (bFileLog == true) ? "enabled" :  "disabled");
        initlog((bool*)bFileLog, "main() "_PROGNFO_, argv[0]);
	printlog(llInfo, "main()", "Initialization ...");
        // log input values
        printlog(llInfo, "ParseArguments", "program arguments: FileName=%s; MAIL_FROM=%s; RCPT_TO=%s", szFileName, SmtpSessionSet.mail_from, szRcptTo);
        // show input values
        // printf("FILENAME: %s\r\n", szFileName);
	// printf("PTR RECORD: %s\r\n", SmtpSessionSet.ptr_domain);
        // printf("MAIL FROM: %s\r\n", SmtpSessionSet.mail_from);
        // printf("RCPT TO: %s\r\n", szRcptTo);

        // set signals handler
        signal(SIGTERM, on_terminate);
        signal(SIGINT, on_terminate);

	// reading resolve.conf
	GetNameserver();

        if (bFileMode == false)
        {
	    PrintErrCode(szRcptTo, ParseEmail(szRcptTo)); 
	    nRetCode = 0;
	} 
 	else
	{	
	    char line[BUFSIZ];
    	    char* comment = NULL;
	    char param[BUFSIZ];
    	    int line_cnt = 0;
	    int param_cnt = 0;

	    FILE* f_list = NULL;

      	    if (access(szFileName, R_OK) != 0)
    	    {
        	printf("_ERR: File not found or inaccessible, file-path=%s\r\n", szFileName);
        	printlog(llWarning, "LoadFileList", "File not found or inaccessible, file-path=%s", szFileName);
        	return EXIT_FAILURE;
    	    }

    	    f_list = fopen(szFileName, "r");
    	    if (f_list == NULL)
    	    {
        	printf("_ERR: Cant open file, file-path=%s\r\n", szFileName);
        	printlog(llError, "LoadFileList", "Cant open file, file-path=%s", szFileName);
        	return EXIT_FAILURE;
    	    }


	    while (fgets(line, sizeof(line), f_list) != NULL)
    	    {

 	        if (bShutdown)
        	{
		   printf("_ERR: Program has been terminated\n");	
            	   printlog(llError, "main()", "Program has been terminated");
		   fclose(f_list);
            	   EXIT_FAILURE;
        	}      

	 	if (line_cnt == (nFileSize))
        	{
            	    printf("_ERR: File size limit exceeded, file-path=%s\r\n", szFileName);   
	  	    printlog(llWarning, "main()", "File size limit exceeded, file-path=%s\r\n", szFileName);
		    fclose(f_list);
		    return EXIT_FAILURE; 
        	}
        
	        if (line[0] != '#') // skip comment
        	{
		   line_cnt++;
            	   // scan
            	   param_cnt = sscanf(line, "%s", param);
		   if (param_cnt != 0)
	   	   {
			PrintErrCode(param, ParseEmail(param)); 
		   }

		}

		usleep(nSleep*1000); // convert to milisec	 
	     }

	    if (line_cnt == 0)
	    {
                printf("_ERR: File appears to be empty, file-path=%s\r\n", szFileName);
                printlog(llError, "File appears to be empty, file-path=%s", szFileName);
  	    }	

 	    fclose(f_list);	
	}
   
        closelog();
        return nRetCode;
}


