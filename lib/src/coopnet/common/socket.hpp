#pragma once

#define SOCKET_DEFAULT_INFO 7919

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#define in_addr_t ULONG
#define MSG_DONTWAIT 0

#define SOCKET_RESET_ERROR() WSASetLastError(0)
#define SOCKET_LAST_ERROR WSAGetLastError()
#define SOCKET_EAGAIN EAGAIN
#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCKET_ECONNRESET WSAECONNRESET

#else

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#define SOCKET_RESET_ERROR() errno = 0
#define SOCKET_LAST_ERROR errno
#define SOCKET_EAGAIN EAGAIN
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#define SOCKET_ECONNRESET ECONNRESET

#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

uint64_t SocketAddHash(uint64_t info);
int SocketInitialize(int aAf, int aType, int aProtocol);
int SocketClose(int aSocket);
void SocketSetOptions(int aSocket);
void SocketLimitBuffer(int aSocket, int64_t* amount);
uint64_t SocketGetInfoBits(int aSocket);

