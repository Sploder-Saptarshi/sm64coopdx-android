#pragma once

#include <thread>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <random>
#include <cstdint>
#include <chrono>
#include "socket.hpp"
#include "connection.hpp"
#include "lobby.hpp"

class Server {
    private:
        std::thread mThreadRecv;
        std::thread mThreadUpdate;
        int mSocket;
        std::map<uint64_t, Connection*> mConnections;
        std::mutex mConnectionsMutex;
        std::map<uint64_t, Lobby*> mLobbies;
        std::mt19937_64 mPrng;
        std::uniform_int_distribution<uint64_t> mRng;
        std::vector<StunTurnServer> mTurnServers;
        std::set<uint64_t> mQueueDisconnects;
        int mLobbyCount = 0;
        int mPlayerCount = 0;
        bool mRefreshBans = false;

        void ReadTurnServers();

    public:

        bool Begin(uint32_t aPort);
        void Receive();
        void Update();

        Connection* ConnectionGet(uint64_t aUserId);

        Lobby* LobbyGet(uint64_t aLobbyId);
        void LobbyListGet(Connection& aConnection, std::string aGame, std::string aPassword);

        void OnLobbyJoin(Lobby* aLobby, Connection* aConnection);
        void OnLobbyLeave(Lobby* aLobby, Connection* aConnection);
        void OnLobbyDestroy(Lobby* aLobby);

        void LobbyCreate(Connection* aConnection, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, uint16_t aMaxConnections, std::string& aPassword, std::string& aDescription);
        void LobbyUpdate(Connection* aConnection, uint64_t aLobbyId, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, std::string& aDescription);

        int PlayerCount();
        int LobbyCount();

        void QueueDisconnect(uint64_t aUserId, bool aLockMutex);
        void RefreshBans();
};

extern Server* gServer;
