#include <sys/types.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#include "socket.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "connection.hpp"
#include "mpacket.hpp"
#include "utils.hpp"

#define MAX_LOBBY_SIZE 16

Server* gServer = NULL;

StunTurnServer sStunServer = {
    .host = "stun.l.google.com",
    .username = "",
    .password = "",
    .port = 19302,
};

static void sOnLobbyJoin(Lobby* lobby, Connection* connection) { gServer->OnLobbyJoin(lobby, connection); }
static void sOnLobbyLeave(Lobby* lobby, Connection* connection) { gServer->OnLobbyLeave(lobby, connection); }
static void sOnLobbyDestroy(Lobby* lobby) { gServer->OnLobbyDestroy(lobby); }

static void sReceiveStart(Server* server) { server->Receive(); }
static void sUpdateStart(Server* server)  { server->Update(); }

void Server::ReadTurnServers() {
    mTurnServers.clear();
    std::ifstream input("turn-servers.cfg");
    std::string line;

    if (input.good()) {
        while (std::getline(input, line)) {
            std::size_t pos1 = line.find(":");
            std::size_t pos2 = line.find(":", pos1 + 1);
            std::size_t pos3 = line.find(":", pos2 + 1);
            if (pos1 == std::string::npos || pos2 == std::string::npos || pos3 == std::string::npos) { continue; }

            StunTurnServer turn = {
                .host = line.substr(0, pos1),
                .username = line.substr(pos1 + 1, pos2 - pos1 - 1),
                .password = line.substr(pos2 + 1, pos3 - pos2 - 1),
                .port = (uint16_t)std::stoul(line.substr(pos3 + 1)),
            };
            mTurnServers.push_back(turn);
            LOG_ERROR("Loaded turn server: %s:%u", turn.host.c_str(), turn.port);
        }
    } else {
        LOG_ERROR("turn-servers.cfg not found");
    }

    input.close();
}

bool Server::Begin(uint32_t aPort) {
    // read TURN servers
    ReadTurnServers();

    // create a master socket
    mSocket = SocketInitialize(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mSocket <= 0) {
        LOG_ERROR("Master socket failed (%d)!", mSocket);
        return false;
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    socklen_t opt;
    if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) != 0) {
        LOG_ERROR("Master socket failed to setsockopt!");
        return false;
    }

    // type of socket created
    struct sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(aPort);

    // bind the socket to localhost port 8888
    if (bind(mSocket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        LOG_ERROR("Bind failed!");
        return false;
    }

    LOG_INFO("Listener on port %d", aPort);

    // try to specify maximum of 5 pending connections for the master socket
    if (listen(mSocket, 5) < 0) {
        LOG_ERROR("Master socket failed to listen!");
        return false;
    }

    // create threads
    mThreadRecv = std::thread(sReceiveStart, this);
    mThreadRecv.detach();
    mThreadUpdate = std::thread(sUpdateStart, this);
    mThreadUpdate.detach();

    // setup callbacks
    gOnLobbyJoin = sOnLobbyJoin;
    gOnLobbyLeave = sOnLobbyLeave;
    gOnLobbyDestroy = sOnLobbyDestroy;

    return true;
}

void Server::Receive() {
    LOG_INFO("Waiting for connections...");

    while (true) {
        // Use a PRNG to generate a random seed
        mPrng = std::mt19937_64(std::chrono::steady_clock::now().time_since_epoch().count());

        // Get random connection id
        uint64_t connectionId = mRng(mPrng);
        while (connectionId == 0 || mConnections.count(connectionId) > 0) {
            connectionId = mRng(mPrng);
        }

        // accept the incoming connection
        Connection* connection = new Connection(connectionId);
        socklen_t len = sizeof(struct sockaddr_in);
        connection->mSocket = accept(mSocket, (struct sockaddr *) &connection->mAddress, &len);

        // make sure the connection worked
        if (connection->mSocket < 0) {
            LOG_ERROR("Failed to accept socket (%d)!", connection->mSocket);
            delete connection;
            continue;
        }

        // start connection
        connection->Begin(gCoopNetCallbacks.DestIdFunction);

        // check if connection is allowed
        if (gCoopNetCallbacks.ConnectionIsAllowed && !gCoopNetCallbacks.ConnectionIsAllowed(connection, true)) {
            QueueDisconnect(connection->mId, true);
        } else {
            // send join packet
            MPacketJoined({
                .userId = connection->mId,
                .version = MPACKET_PROTOCOL_VERSION
            }).Send(*connection);

            // send stun server
            MPacketStunTurn(
                { .isStun = true, .port = sStunServer.port },
                { sStunServer.host, sStunServer.username, sStunServer.password }
            ).Send(*connection);

            // send turn servers
            std::shuffle(mTurnServers.begin(), mTurnServers.end(), mPrng);
            for (auto& it : mTurnServers) {
                MPacketStunTurn(
                    { .isStun = false, .port = it.port },
                    { it.host, it.username, it.password }
                ).Send(*connection);
            }
        }
        // remember connection
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        mConnections[connection->mId] = connection;
        LOG_INFO("[%" PRIu64 "] Connection added, count: %" PRIu64 "", connection->mId, (uint64_t)mConnections.size());
    }
}

void Server::Update() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        int players = 0;
        size_t queueDisconnectCount = mQueueDisconnects.size();
        for (auto it = mConnections.begin(); it != mConnections.end(); ) {
            Connection* connection = it->second;
            // erase the connection if it's inactive, otherwise receive packets
            if (connection != nullptr) {
                if (connection->mActive && connection->mLobby != nullptr) {
                    players++;
                }
                if (mRefreshBans && gCoopNetCallbacks.ConnectionIsAllowed && !gCoopNetCallbacks.ConnectionIsAllowed(connection, false)) {
                    connection->Disconnect(true);
                }
                if (!connection->mActive) {
                    LOG_INFO("[%" PRIu64 "] Connection removed, count: %" PRIu64 "", connection->mId, (uint64_t)mConnections.size());
                    delete connection;
                    it = mConnections.erase(it);
                    continue;
                } else if (connection != nullptr) {
                    connection->Receive();
                    connection->Update();
                }
                if (mQueueDisconnects.count(connection->mId) > 0) {
                    connection->Disconnect(true);
                }
            }
            ++it;
        }

        if (queueDisconnectCount == mQueueDisconnects.size()) {
            mQueueDisconnects.clear();
        }
        mRefreshBans = false;

        mPlayerCount = players;
        fflush(stdout);
        fflush(stderr);
    }
}

Connection *Server::ConnectionGet(uint64_t aUserId) {
    return mConnections[aUserId];
}

Lobby* Server::LobbyGet(uint64_t aLobbyId) {
    return mLobbies[aLobbyId];
}

void Server::LobbyListGet(Connection& aConnection, std::string aGame, std::string aPassword) {
    for (auto& it : mLobbies) {
        if (!it.second) { continue; }
        if (it.second->mGame != aGame) { continue; }
        if (it.second->mPassword != aPassword) { continue; }

        MPacketLobbyListGot({
            .lobbyId = it.first,
            .ownerId = it.second->mOwner->mId,
            .connections = (uint16_t)it.second->mConnections.size(),
            .maxConnections = it.second->mMaxConnections
        }, {
            it.second->mGame,
            it.second->mVersion,
            it.second->mHostName,
            it.second->mMode,
            it.second->mDescription,
        }).Send(aConnection);
    }
    MPacketLobbyListFinish({ 0 }).Send(aConnection);
}

void Server::OnLobbyJoin(Lobby* aLobby, Connection* aConnection) {
    MPacketLobbyJoined({
        .lobbyId = aLobby->mId,
        .userId = aConnection->mId,
        .ownerId = aLobby->mOwner->mId,
        .destId = aConnection->mDestinationId,
        .priority = aConnection->mPriority
    }).Send(*aLobby);

    // inform joiner of other connections
    for (auto& it : aLobby->mConnections) {
        if (it->mId == aConnection->mId) {
            continue;
        }
        MPacketLobbyJoined({
            .lobbyId = aLobby->mId,
            .userId = it->mId,
            .ownerId = aLobby->mOwner->mId,
            .destId = it->mDestinationId,
            .priority = it->mPriority
        }).Send(*aConnection);
    }
}

void Server::OnLobbyLeave(Lobby* aLobby, Connection* aConnection) {
    MPacketLobbyLeft({
        .lobbyId = aLobby->mId,
        .userId = aConnection->mId
    }).Send(*aLobby);
}

void Server::OnLobbyDestroy(Lobby* aLobby) {
    mLobbies.erase(aLobby->mId);
    mLobbyCount--;
    LOG_INFO("[%" PRIu64 "] Lobby removed, count: %" PRIu64 "", aLobby->mId, (uint64_t)mLobbies.size());
}

void Server::LobbyCreate(Connection* aConnection, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, uint16_t aMaxConnections, std::string& aPassword, std::string &aDescription) {
    // check if this connection already has a lobby
    if (aConnection->mLobby) {
        aConnection->mLobby->Leave(aConnection);
    }

    // Get random lobby id
    uint64_t lobbyId = mRng(mPrng);
    while (lobbyId == 0 || mLobbies.count(lobbyId) > 0) {
        lobbyId = mRng(mPrng);
    }

    // limit the lobby size
    if (aMaxConnections > MAX_LOBBY_SIZE) {
        aMaxConnections = MAX_LOBBY_SIZE;
    }

    // create the new lobby
    Lobby* lobby = new Lobby(aConnection,
        lobbyId,
        aGame,
        aVersion,
        aHostName,
        aMode,
        aMaxConnections,
        aPassword,
        aDescription);

    mLobbies[lobby->mId] = lobby;

    LOG_INFO("[%" PRIu64 "] Lobby added, count: %" PRIu64 "", lobby->mId, (uint64_t)mLobbies.size());

    // notify of lobby creation
    MPacketLobbyCreated({
        .lobbyId = lobby->mId,
        .maxConnections = aMaxConnections,
    }, {
        lobby->mGame,
        lobby->mVersion,
        lobby->mHostName,
        lobby->mMode
    }).Send(*aConnection);

    lobby->Join(aConnection, aPassword);
    mLobbyCount++;
}

void Server::LobbyUpdate(Connection *aConnection, uint64_t aLobbyId, std::string &aGame, std::string &aVersion, std::string &aHostName, std::string &aMode, std::string &aDescription) {
    Lobby* lobby = LobbyGet(aLobbyId);
    if (!lobby) {
        LOG_ERROR("Could not find lobby to update: %" PRIu64 "", aLobbyId);
        return;
    }
    if (lobby->mOwner != aConnection) {
        LOG_ERROR("Could not update lobby, was not the owner: %" PRIu64 "", aLobbyId);
        return;
    }

    lobby->mGame = aGame.substr(0, 32);
    lobby->mVersion = aVersion.substr(0, 32);
    lobby->mHostName = aHostName.substr(0, 32);
    lobby->mMode = aMode.substr(0, 32);
    lobby->mDescription = aDescription.substr(0, 256);
}

int Server::PlayerCount() {
    return mPlayerCount;
}

int Server::LobbyCount() {
    return mLobbyCount;
}

void Server::QueueDisconnect(uint64_t aUserId, bool aLockMutex) {
    if (aLockMutex) {
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        if (mQueueDisconnects.count(aUserId)) { return; }
        mQueueDisconnects.insert(aUserId);
    } else {
        if (mQueueDisconnects.count(aUserId)) { return; }
        mQueueDisconnects.insert(aUserId);
    }
}

void Server::RefreshBans() {
    std::lock_guard<std::mutex> guard(mConnectionsMutex);
    mRefreshBans = true;
}
