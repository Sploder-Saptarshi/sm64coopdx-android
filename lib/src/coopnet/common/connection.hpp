#pragma once

#include "libcoopnet.h"
#include "socket.hpp"
#include "mpacket.hpp"
#include "lobby.hpp"

class Lobby;

class Connection {
    private:
        uint8_t mData[MPACKET_MAX_SIZE] = { 0 };
        int64_t mDataSize = 0;

    public:
        bool mActive = false;
        bool mIntentionalDisconnect = false;
        uint64_t mId = 0;
        uint64_t mDestinationId = 0;
        uint64_t mInfoBits = 0;
        bool mUpdated = false;
        int mSocket = 0;
        struct sockaddr_in mAddress = { 0};
        Lobby* mLobby = nullptr;
        uint32_t mPriority = 0;
        uint64_t mLastSendTime = 0;
        std::string mAddressStr;

        Connection(uint64_t id);
        ~Connection();
        void Begin(uint64_t (*aDestIdFunction)(uint64_t aInput));
        void Disconnect(bool aIntentional);
        void Update();
        void Receive();
};
