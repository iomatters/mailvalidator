#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "sckt.h"
#include "printlog.h"


SOCKET CreateListenSocket(int nport)
{
    struct sockaddr_in addr = {0};
    int sock = INVALID_SOCKET;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(nport);
    addr.sin_addr.s_addr = INADDR_ANY;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printlog(llError, "CreateListenSocket", "socket(): %d-%s", errno, strerror(errno));
        return INVALID_SOCKET;
    }

    int opt = 1; // ???
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0)
    {
        printlog(llError, "CreateListenSocket", "setsockopt(): %d-%s", errno, strerror(errno));
        close(sock);
        return INVALID_SOCKET;
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printlog(llError, "CreateListenSocket", "bind(): %d-%s", errno, strerror(errno));
        close(sock);
        return INVALID_SOCKET;
    }

    if (listen(sock, 1) < 0)
    {
        printlog(llError, "CreateListenSocket", "listen(): %d-%s", errno, strerror(errno));
        close(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

SOCKET CreateClientSocket(char* addr, int nport, int& ntimeout)
{
    struct sockaddr_in sa = {0};
    int sock = INVALID_SOCKET;
    bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(nport);
    sa.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printlog(llError, "CreateClientSocket", "socket(): %d-%s", errno, strerror(errno));
        return INVALID_SOCKET;
    }

    struct timeval tv;
    tv.tv_sec = ntimeout;             /* 10 second timeout */
    tv.tv_usec = 0;

    long arg = fcntl(sock, F_GETFL, NULL); 
    arg |= O_NONBLOCK; 
    fcntl(sock, F_SETFL, arg); 

    if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
         if (errno == EINPROGRESS) {
	    fd_set fdset;
	    FD_ZERO(&fdset);
            FD_SET(sock, &fdset);

            if (select(sock + 1, NULL, &fdset, NULL, &tv) > 0) {
                int so_error = 0;
                socklen_t len = sizeof so_error;
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
 
                if (so_error != 0) { 
                    printlog(llWarning, "CreateClientSocket", "Connect(): %d-%s", so_error, strerror(so_error));
                    close(sock);
                    return INVALID_SOCKET; 
                }
            }
	    else {
                printlog(llWarning, "CreateClientSocket", "Connection timeout error: %d-%s", errno, strerror(errno));
                close(sock);
                return INVALID_SOCKET;
	    }
         }
         else {
	     printlog(llError, "CreateClientSocket", "Connecting error: %d-%s", errno, strerror(errno));
             close(sock);
             return INVALID_SOCKET;
         }
    }

    arg = fcntl(sock, F_GETFL, NULL); 
    arg &= (~O_NONBLOCK); 
    fcntl(sock, F_SETFL, arg); 

    return sock;
}

int SendBufferMove(SOCKET sock, char* buf, int& cnt)
{
    int nbytes = 0;

    if (cnt > 0)
    {
        nbytes = send(sock, buf, cnt, 0);
        if(nbytes >= 0)
        {
            memmove(buf, buf + nbytes, cnt - nbytes);
            cnt -= nbytes;
        }
    }
    return nbytes;
}

int RecvBufferMove(SOCKET sock, char* buf, int bufsiz, int& cnt)
{
    int nbytes = 0;

    struct timeval tv;
    tv.tv_sec = 20;             /* 10 second timeout */
    tv.tv_usec = 0;

    //setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO,(const char *)&tv , sizeof(tv));
    if ((nbytes =  recv(sock, buf + cnt, bufsiz - cnt, 0)) < 0)
   	printlog(llError, "RecvBufferMove", "recv timeout error: %d-%s", errno, strerror(errno));   
    else
    	 cnt += nbytes;
    
    return nbytes;
}


int RecvBufferFrom(SOCKET sock, int port, unsigned char* buf, unsigned int cnt, char* dst)
{
    int nbytes = 0;
    struct sockaddr_in dest = { 0,0,0,0 };

    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = inet_addr(dst); //dns servers

    struct timeval tv;
    tv.tv_sec = 10;             /* 10 second timeout */
    tv.tv_usec = 0;

    unsigned int sz = sizeof(dest);
    setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO,(const char *)&tv , sizeof(tv));

    // decided to leave it as it is, the 35 error is probably related to UDP/NAT problem
    if ((nbytes = recvfrom(sock, (char *)buf, cnt, 0, (struct sockaddr*)&dest,&sz)) < 0)
	printlog(llError, "RecvBufferFrom", "recvfrom timeout error: %d-%s", errno, strerror(errno));
	
     return nbytes;	
}
