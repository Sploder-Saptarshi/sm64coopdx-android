#include <cstdint>
#include <string>
#include <set>
#include <functional>
#include <unistd.h>
#include <fcntl.h>

#include "connection.hpp"
#include "libcoopnet.h"
#include "logging.hpp"
#include "mpacket.hpp"

#define CONNECTION_KEEP_ALIVE_SECS (60 * 3)

Connection::Connection(uint64_t id) {
    mId = id;
}

Connection::~Connection() {
}

void Connection::Begin(uint64_t (*aDestIdFunction)(uint64_t aInput)) {
    // store info
    mActive = true;

    // get destination id
    struct sockaddr_in addr = { 0 };
    socklen_t len = sizeof(addr);
    getpeername(mSocket, (struct sockaddr*)&addr, &len);
    uint64_t addr64 = (uint64_t)addr.sin_addr.s_addr;
    mDestinationId = (aDestIdFunction)
        ? aDestIdFunction(addr64)
        : 0;

    // set socket to non-blocking mode
    SocketSetOptions(mSocket);

    // convert address
    char asciiAddress[256] = { 0 };
    inet_ntop(AF_INET, &(mAddress.sin_addr), asciiAddress, sizeof(asciiAddress));
    mAddressStr = asciiAddress;

    // don't send a keep-alive packet immediately
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    uint64_t now = std::chrono::system_clock::to_time_t(nowTp);
    mLastSendTime = now;

    LOG_INFO("[%" PRIu64 "] Connection accepted: %s :: %" PRIu64 "", mId, mAddressStr.c_str(), mDestinationId);
}

void Connection::Disconnect(bool aIntentional) {
    if (!mActive) { return; }

    if (mLobby) {
        mLobby->Leave(this);
    }

    mActive = false;
    SocketClose(mSocket);

    if (gCoopNetCallbacks.OnDisconnected) {
        gCoopNetCallbacks.OnDisconnected(aIntentional);
    }
}

void Connection::Update() {
    // send a packet with no important informations every 3 minutes,
    // just to keep the connection alive
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    uint64_t now = std::chrono::system_clock::to_time_t(nowTp);
    if ((mLastSendTime + CONNECTION_KEEP_ALIVE_SECS) < now) {
        MPacketKeepAlive({ 0 }).Send(*this);
    }
}

void Connection::Receive() {
    // check buffer size
    int64_t remaining = (int64_t)MPACKET_MAX_SIZE - (int64_t)mDataSize;
    if (remaining <= 0) {
        LOG_ERROR("[%" PRIu64 "] Receive buffer full %" PRId64 "", mId, remaining);
        Disconnect(false);
        return;
    }

    // limit the buffer to the available amount
    SocketLimitBuffer(mSocket, &remaining);

#if defined(OSX_BUILD) || defined(__unix__)
#ifndef __linux__
    // OSX seems to return errno 0, size 0 on recv() when there is nothing to receive.
    // This causes the socket to think the connection is closed...
    // So instead, we'll just not call it if there is no data available.
    // The side effect of this is that we will not detect connection drops very quickly.
    if (remaining <= 0) { return; }
#endif
#endif

    // receive from socket
    SOCKET_RESET_ERROR();
    int ret = recv(mSocket, (char*)&mData[mDataSize], (size_t)remaining, MSG_DONTWAIT);
    int rc = SOCKET_LAST_ERROR;
    /*if ((ret != -1) || (rc != SOCKET_EAGAIN && rc != SOCKET_EWOULDBLOCK)) {
        LOG_INFO("RECV: %d, %d, %" PRId64 ", %" PRId64, ret, rc, remaining, mDataSize);
    }*/

    // make sure connection is still active
    if (!mActive) { return; }

    // check for error
    if ((ret == -1) && (rc == SOCKET_EAGAIN || rc == SOCKET_EWOULDBLOCK)) {
        //LOG_INFO("[%" PRIu64 "] continue", mId);
        return;
    } else if (ret == 0 || (rc == SOCKET_ECONNRESET)) {
        LOG_INFO("[%" PRIu64 "] Connection closed (%d, %d).", mId, ret, rc);
        Disconnect(false);
        return;
    } else if (ret < 0) {
        LOG_ERROR("[%" PRIu64 "] Error receiving data (%d)!", mId, rc);
        Disconnect(false);
        return;
    }

    /*LOG_INFO("[%" PRIu64 "] Received data:", mId);
    for (size_t i = 0; i < (size_t)ret; i++) {
        printf("  %02X", data[i]);
    }
    printf("\n");*/

    mDataSize += ret;
    MPacket::Read(this, mData, &mDataSize, MPACKET_MAX_SIZE);
}
