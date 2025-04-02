#pragma once

#include <vector>
#include <mutex>
#include <map>
#include <stdint.h>
#include "connection.hpp"
#include "peer.hpp"
#include "utils.hpp"

class Client {
    private:
        std::map<uint64_t, Peer*> mPeers;
    public:
        uint64_t mCurrentUserId = 0;
        uint64_t mCurrentLobbyId = 0;
        uint32_t mCurrentPriority = 0;
        bool mUpdating = false;
        Connection* mConnection = nullptr;
        std::vector<PeerEvent> mEvents;
        std::mutex mEventsMutex;

        StunTurnServer mStunServer;
        std::vector<StunTurnServer> mTurnServers;
        bool mShutdown = false;

        ~Client();

        bool Begin(std::string aHost, uint32_t aPort, std::string aName, uint64_t aDestId);
        void Update();
        void Disconnect();

        void PeerBegin(uint64_t aUserId, uint32_t aPriority);
        void PeerEnd(uint64_t aUserId);
        void PeerEndAll();
        Peer* PeerGet(uint64_t aUserId);
        bool PeerSend(const uint8_t* aData, size_t aDataLength);
        bool PeerSendTo(uint64_t aPeerId, const uint8_t* aData, size_t aDataLength);

        void LobbyCreate(std::string aGame, std::string aVersion, std::string aHostName, std::string aMode, uint16_t aMaxConnections, std::string aPassword, std::string aDescription);
        void LobbyUpdate(uint64_t aLobbyId, std::string aGame, std::string aVersion, std::string aHostName, std::string aMode, std::string aDescription);
        void LobbyJoin(uint64_t aLobbyId, std::string aPassword);
        void LobbyLeave(uint64_t aLobbyId);
        void LobbyListGet(std::string aGame, std::string aPassword);
};

extern Client* gClient;
