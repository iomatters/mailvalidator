#ifndef SCKT_H
#define SCKT_H

typedef int SOCKET;

#define INVALID_SOCKET -1

SOCKET CreateListenSocket(int);
SOCKET CreateClientSocket(char*, int, int&);
int SendBufferMove(SOCKET sock, char* buf, int& cnt);
int RecvBufferMove(SOCKET sock, char* buf, int bufsiz, int& cnt);
int RecvBufferFrom(SOCKET, int, unsigned char*, unsigned int, char*);

#endif
