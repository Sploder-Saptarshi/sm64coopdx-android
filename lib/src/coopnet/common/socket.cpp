#include "socket.hpp"
#include "libcoopnet.h"
#include "logging.hpp"

#ifdef __APPLE__
#define SIOCGIFHWADDR SIOCGIFCONF
#define ifr_hwaddr ifr_addr
#endif

uint64_t SocketAddHash(uint64_t info) {
    uint16_t hash = 0;
    uint8_t* chunks = (uint8_t*)&info;
    for (int i = 0; i < 6; i++) {
        hash ^= ~(chunks[i] * 37);
    }
    info &= 0x0000FFFFFFFFFFFF;
    info |= ((uint64_t)hash) << (8 * 6);
    return info;
}

#ifdef _WIN32
#include <iphlpapi.h>

int SocketInitialize(int aAf, int aType, int aProtocol) {
    // start up winsock
    if (!gCoopNetSettings.SkipWinsockInit) {
        WSADATA wsaData;
        int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rc != NO_ERROR) {
            LOG_ERROR("WSAStartup failed with error %d", rc);
            return -1;
        }
    }

    SOCKET_RESET_ERROR();
    int sock = socket(aAf, aType, aProtocol);
    if (sock == -1) {
        LOG_ERROR("socket creation failed with error %d", SOCKET_LAST_ERROR);
        return sock;
    }

    return sock;
}

int SocketClose(int aSocket) {
    int ret = closesocket(aSocket);
    //WSACleanup();
    return ret;
}

void SocketSetOptions(int aSocket) {
    // set socket to non-blocking mode
    SOCKET_RESET_ERROR();
    u_long mode = 1;
    if (ioctlsocket(aSocket, FIONBIO, &mode) != 0) {
        LOG_ERROR("failed to set non-blocking: %d", SOCKET_LAST_ERROR);
    }

    // set socket to keep-alive mode
    SOCKET_RESET_ERROR();
    int keepAlive = 1;
    int result = setsockopt(aSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));
    if (result == SOCKET_ERROR) {
        LOG_ERROR("failed to set keep-alive: %d", SOCKET_LAST_ERROR);
    }

    // set socket to dont-linger
    SOCKET_RESET_ERROR();
    int on = 1;
    if (setsockopt(aSocket, SOL_SOCKET, SO_DONTLINGER, (char*)&on, sizeof(on)) < 0) {
        LOG_ERROR("failed to set dont-linger: %d", SOCKET_LAST_ERROR);
    }

}

void SocketLimitBuffer(int aSocket, int64_t* amount) {
    SOCKET_RESET_ERROR();
    unsigned long bufferLength = 0;
    if (ioctlsocket(aSocket, FIONREAD, &bufferLength) != 0) {
        LOG_ERROR("failed to retrieve buffer size: %d", SOCKET_LAST_ERROR);
        return;
    }
    if (*amount > bufferLength) {
        *amount = bufferLength;
    }
}

uint64_t SocketGetInfoBits(int aSocket) {
    DWORD bufferSize = 0;
    GetAdaptersInfo(NULL, &bufferSize);
    PIP_ADAPTER_INFO adapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    uint64_t info = SOCKET_DEFAULT_INFO;

    if (adapterInfo && GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO currentAdapter = adapterInfo;

        while (currentAdapter != NULL) {
            bool active = false;
            IP_ADDR_STRING* ip = &currentAdapter->IpAddressList;
            while (ip) {
                if (strcmp(ip->IpAddress.String, "0.0.0.0") && strcmp(ip->IpAddress.String, "127.0.0.1")) { active = true; }
                ip = ip->Next;
            }

            if (active && currentAdapter->AddressLength == 6) {
                uint64_t value = 0;
                for (int i = 0; i < 6; i++) {
                    value ^= ((uint64_t)currentAdapter->Address[i]) << (i * 8);
                }
                info += value;
            }

            currentAdapter = currentAdapter->Next;
        }
    }

    if (adapterInfo) { free(adapterInfo); }

    info = SocketAddHash(info);
    return info;
}
#else

#include <sys/ioctl.h>
#include <net/if.h>
#ifdef TARGET_ANDROID
// Only necessary for Android 5 and 6
// https://stackoverflow.com/questions/26476346/ifaddrs-h-header-not-found-when-compiling-sdl-for-android
#include "android/ifaddrs-android.h"
#else
#include <ifaddrs.h>
#endif

int SocketInitialize(int aAf, int aType, int aProtocol) {
    return socket(aAf, aType, aProtocol);
}

int SocketClose(int aSocket) {
    return close(aSocket);
}

void SocketSetOptions(int aSocket) {
    // set socket to non-blocking mode
    int flags = fcntl(aSocket, F_GETFL, 0);
    SOCKET_RESET_ERROR();
    int rc = fcntl(aSocket, F_SETFL, ((unsigned int)flags) | O_NONBLOCK);
    if (rc == -1) {
        LOG_ERROR("failed to set to non-blocking: %d", SOCKET_LAST_ERROR);
    }

    // set socket to keep-alive mode
    SOCKET_RESET_ERROR();
    int optval = 1;
    if(setsockopt(aSocket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
        LOG_ERROR("failed to set to keep-alive mode: %d", SOCKET_LAST_ERROR);
    }

    // set socket to dont-linger
    SOCKET_RESET_ERROR();
    struct linger lingerStruct = { 1, 0 };
    if (setsockopt(aSocket, SOL_SOCKET, SO_LINGER, &lingerStruct, sizeof(lingerStruct)) < 0) {
        LOG_ERROR("failed to set to dont-linger mode: %d", SOCKET_LAST_ERROR);
    }
}

void SocketLimitBuffer(int aSocket, int64_t* amount) {
    SOCKET_RESET_ERROR();
    int bufferLength = 0;
    if (ioctl(aSocket, FIONREAD, &bufferLength) == -1) {
        LOG_ERROR("failed to retrieve buffer size: %d", SOCKET_LAST_ERROR);
        return;
    }
    if (*amount > bufferLength) {
        *amount = bufferLength;
    }
}

uint64_t SocketGetInfoBits(int aSocket) {
    struct ifaddrs* ifaddr = NULL;
    struct ifaddrs* ifa = NULL;
    uint64_t info = SOCKET_DEFAULT_INFO;

    if (getifaddrs(&ifaddr) == 0 && ifaddr) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) { continue; }
            if (ifa->ifa_addr->sa_family != AF_INET) { continue; }

            struct ifreq ifr;
            strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);

            if (ioctl(aSocket, SIOCGIFHWADDR, &ifr) < 0) {
                continue;
            }

            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
            if (!strcmp(ip, "127.0.0.1")) { continue; }
            if (!strcmp(ip, "0.0.0.0")) { continue; }

            unsigned char* hw = (unsigned char*)ifr.ifr_hwaddr.sa_data;
            uint64_t value = 0;
            for (int i = 0; i < 6; i++) {
                value ^= ((uint64_t)hw[i]) << (8 * i);
            }
            info += value;
        }

        freeifaddrs(ifaddr);
    }

    info = SocketAddHash(info);
    return info;
}

#endif
