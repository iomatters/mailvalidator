#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include "smtpclient.h"
#include "mailvalidator.h"
#include "queue.h"
#include "sckt.h"
#include "printlog.h"

bool bShutdownT = false;
// SMTP Proto
char szEhloCommand[256];
char szMailFromCommand[256];
char szRcptToCommand[256];
char szCatchAllToCommand[256];
char const* szQuitCommand = "QUIT\r\n";
char szRemoteAddress[256];
int kq_client = -1;
SOCKET ClientSocket = -1;
int const MAXBUFSIZE = 4096;
int SOCK_TIMEOUT = 15;
unsigned char CATCH_ALL_CHECK = 0;
char client_send_buf[MAXBUFSIZE];
int client_send_buf_cnt = 0;
char client_recv_buf[MAXBUFSIZE];
int client_recv_buf_cnt = 0;

// SMTP return code
int n_smtpreply = 0;

//extern "C"

bool Terminated() 
{
   return (bShutdown | bShutdownT);
}

void ConnectSocket(void)
{
    while (!Terminated() && ClientSocket < 0)
    {
            ClientSocket = CreateClientSocket((char*)szRemoteAddress, SMTP_PORT, SOCK_TIMEOUT);
            if (ClientSocket < 0) {
		n_smtpreply = 1001; // socket connectivity error
                printlog(llWarning, "ConnectSocket", "connecting client socket failed to %s:%d", szRemoteAddress, SMTP_PORT);
            }
	    else
                printlog(llInfo, "ConnectSocket", "client socket %d connected to %s:%d", ClientSocket, szRemoteAddress, SMTP_PORT);

            // Clear buffers 
            client_recv_buf_cnt = 0;
	    client_send_buf_cnt = 0;
  	    if (ClientSocket < 0)
	    {
	        bShutdownT = true;
                // usleep(10000);
	    }
    }
}

void DisconnectSocket(void)
{
        close(ClientSocket);
	printlog(llInfo, "DisconnectSocket", "client socket %d disconnected from %s:%d", ClientSocket, szRemoteAddress, SMTP_PORT);
        ClientSocket = -1;

        // clear recv buffer
        client_recv_buf_cnt = 0;
	client_send_buf_cnt = 0;
	bShutdownT = true;
}


int SendRequest(void)
{
   if (Terminated()) return EXIT_FAILURE;

    int res = EXIT_FAILURE;
    int nbytes = SendBufferMove(ClientSocket, client_send_buf, client_send_buf_cnt);
    res = nbytes < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    if(client_send_buf_cnt == 0)
        ChangeQueueEvent(kq_client, ClientSocket, EVFILT_WRITE, EV_DISABLE);
    return res;
}


int PutRequest(char *request_buf)
{
    if (Terminated()) return EXIT_FAILURE;
    int request_nbytes = strlen(request_buf); 

    if (request_nbytes <= 0)
        return EXIT_FAILURE;

    int res = EXIT_FAILURE;
    {
        if (request_nbytes <= (MAXBUFSIZE - client_send_buf_cnt))
        {

            memmove(client_send_buf + client_send_buf_cnt, request_buf, request_nbytes);
            client_send_buf_cnt += request_nbytes;
            client_send_buf[request_nbytes] = '\0'; 	
            ChangeQueueEvent(kq_client, ClientSocket, EVFILT_WRITE, EV_ENABLE);
            res = EXIT_SUCCESS;
        }
    }
    return res;
}


int RecvResponse(void)
{
    int nRetCode = RecvBufferMove(ClientSocket, client_recv_buf, MAXBUFSIZE, client_recv_buf_cnt);
    client_recv_buf[client_recv_buf_cnt] = '\0';  

    return nRetCode;
}

void* ThreadSMTPClient(void *arg)
{
    printlog(llInfo, "ThreadSMTPClient", "started");
    int n_progress = STATE_UNDEFINED;

    SOCKET RespondSocket = -1;
    struct kevent event = { 0,0,0,0,0,0 }; // filling 0 just for success init
    int event_count = 0;
    char buf[MAXBUFSIZE];
    int nbytes = 0;

    // queue
    if (InitQueue(kq_client) != EXIT_SUCCESS)
    {
        printlog(llError, "ThreadSMTPClient", "init queue failed");
        bShutdownT = true;
        pthread_exit(NULL);
    }

    while (!Terminated())
    {

        ConnectSocket();
        if (Terminated()) break;
        //
        ChangeQueueEvent(kq_client, ClientSocket, EVFILT_READ, EV_ADD | EV_ENABLE);
        ChangeQueueEvent(kq_client, ClientSocket, EVFILT_WRITE, EV_ADD | (client_send_buf_cnt  > 0 ? EV_ENABLE : EV_DISABLE));

	unsigned int nIdleCount = 0;
        while(!Terminated())
        {
	    // additional precaution if SMTP server hangs up in the middle of negotiation
	    if (nIdleCount > MAX_IDLE_COUNT)
	    {
		printlog(llWarning, "ThreadSMTPClient", "Idle time limit exceeded - MX not responding, status=%d, MX=%s", n_progress, szRemoteAddress);
		n_smtpreply = 5005;
	 	bShutdownT = true;
		pthread_exit(NULL);	
	    }
            if (GetQueueEvent(kq_client, &event, event_count) != EXIT_SUCCESS)
            {
                printlog(llWarning, "ThreadSMTPClient", "Error wating event, in call kevent(...)");
            }
            else if (event.ident == ClientSocket && event_count == 1)
            {
 
                if (Terminated()) break;

                if (event.flags & (EV_EOF | EV_ERROR))
                {
                    printlog(llInfo, "ThreadSMTPClient" , "Disconnected socket %d", event.ident);
           	    bShutdownT = true;
	        }
                else if (event.filter == EVFILT_READ)
                {

		    if ((nbytes = RecvResponse()) <= 0)
                    {
                        printlog(llWarning, "ThreadSMTPClient", "socket %d broken pipe to read - closed", event.ident);
                	bShutdownT = true;
	            }
		    else
		    {
		        // 
		        // Start validation
		        //
			char* tmp_buf = client_recv_buf + (client_recv_buf_cnt - nbytes);
			n_smtpreply = atoi(tmp_buf);
			switch(n_smtpreply)
    			{
        	  	   case 220:
           		      	PutRequest((char*)szEhloCommand);
           		      	n_progress = STATE_EHLOSENT;
				break;
        		   case 250 ... 251:
           		     	if (n_progress == STATE_EHLOSENT)
           			{
                			PutRequest((char*)szMailFromCommand);
                			n_progress = STATE_MAILFROMSENT; 
           			}
           			else
          			if (n_progress == STATE_MAILFROMSENT)
           			{
                			PutRequest(szRcptToCommand);
                			n_progress = STATE_RCPTTOSENT; 
           			}
           			else
           			if (n_progress == STATE_RCPTTOSENT)
				{
					if (CATCH_ALL_CHECK)
					{
				 	   PutRequest(szCatchAllToCommand);
				    	   n_progress = STATE_CATCHALLSENT;
					}
					else
					{
					   PutRequest((char*)szQuitCommand);
                			   n_progress = STATE_SUCCESSFUL;
					   bShutdownT = true;
					}
                                }
				else // means we did the catch all test and the result is positive
				if (n_progress == STATE_CATCHALLSENT)
				{ 	
				   n_smtpreply = 5007;
				   PutRequest((char*)szQuitCommand);
                                   n_progress = STATE_SUCCESSFUL;
                                   bShutdownT = true;
				}
                                break;	
  		   	 default:
				if (n_progress == STATE_CATCHALLSENT)
				{
			   	   // set 250 because we got here due the the catch all negative result	
				   n_smtpreply = 250; 
				   n_progress = STATE_SUCCESSFUL; 
				}

                                bShutdownT = true;
    			 }
		     }
		         //
		         // Validation completed 
		         //  
                }
                else if (event.filter == EVFILT_WRITE)
                {
                    if (SendRequest() != EXIT_SUCCESS)
                    {
                        printlog(llWarning, "ThreadSMTPClient", "socket %d broken pipe to write - closed", event.ident);
                    }
                }

            }
	    // incrementing idle counter
	    nIdleCount++;
            pthread_yield();
        }

        ChangeQueueEvent(kq_client, ClientSocket, EVFILT_READ, EV_DELETE);
        ChangeQueueEvent(kq_client, ClientSocket, EVFILT_WRITE, EV_DELETE);
        //
        DisconnectSocket();
    }
    CloseQueue(kq_client);
    bShutdownT = true;

    printlog(llInfo, "ThreadSMTPClient", "finished");
    pthread_exit(NULL);
}

int SmtpCheckRcpt(SMTP_SESSION_SET* smtp_session_set, char* rcptto, char* ip) 
{
    // formt SMTP commands 
    sprintf(szEhloCommand, "EHLO %s\r\n", smtp_session_set->ptr_domain);
    sprintf(szMailFromCommand, "MAIL FROM:<%s>\r\n", smtp_session_set->mail_from);
    sprintf(szRcptToCommand, "RCPT TO:<%s>\r\n", rcptto);
    sprintf(szCatchAllToCommand, "RCPT TO:<%s@%s>\r\n", CATCH_ALL_ADDRESS, smtp_session_set->rcpt_domain);
    sprintf(szRemoteAddress, "%s", ip);
    CATCH_ALL_CHECK = smtp_session_set->catch_all_check;
    SOCK_TIMEOUT = smtp_session_set->sock_async_timeout;
    // start thread
    bShutdownT = bShutdown;

    pthread_t ht;
    pthread_create(&ht, NULL, ThreadSMTPClient, NULL);

    while(!Terminated())
    {
       usleep(10000); // 100 milisec
    }
  
    return n_smtpreply; 
}
