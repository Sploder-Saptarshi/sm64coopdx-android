#include "client.hpp"
#include "mpacket.hpp"
#include "utils.hpp"
#include "logging.hpp"

#ifndef _WIN32
#include <sys/select.h>
#endif

Client* gClient = NULL;

Client::~Client() {
    Disconnect();
    if (mConnection) {
        mConnection->Disconnect(true);
        delete mConnection;
        mConnection = nullptr;
    }
}

bool Client::Begin(std::string aHost, uint32_t aPort, std::string aName, uint64_t aDestId)
{
    mConnection = new Connection(0);

    // setup default stun server
    mStunServer.host = "stun.l.google.com";
    mStunServer.port = 19302;

    // setup a socket
    mConnection->mSocket = SocketInitialize(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(mConnection->mSocket <= 0)
    {
        LOG_ERROR("Socket failed");
        return false;
    }

    // type of socket created
    mConnection->mAddress.sin_family = AF_INET;
    mConnection->mAddress.sin_addr.s_addr = GetAddrFromDomain(aHost);
    mConnection->mAddress.sin_port = htons(aPort);

    SocketSetOptions(mConnection->mSocket);
    errno = 0;

    int rc = connect(mConnection->mSocket, (struct sockaddr*) &mConnection->mAddress, sizeof(struct sockaddr_in));
    if (rc < 0) {
        rc = errno;
        if (rc == EINPROGRESS || rc == 0) {
            // Setup the timeout duration
            struct timeval timeout;
            timeout.tv_sec = 3;
            timeout.tv_usec = 0;

            // Setup the file descriptors to watch for write readiness
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(mConnection->mSocket, &writeSet);

            // Use select to wait for the socket to become writable or timeout
            int selectResult = select(mConnection->mSocket + 1, nullptr, &writeSet, nullptr, &timeout);
            if (selectResult == 0) {
                LOG_ERROR("Connection timed out");
                SocketClose(mConnection->mSocket);
                return false;
            } else if (selectResult < 0) {
                LOG_ERROR("Error while waiting for connection");
                SocketClose(mConnection->mSocket);
                return false;
            }
        } else {
            // Other error occurred during connect
            LOG_ERROR("Connect failed: %u", rc);
            SocketClose(mConnection->mSocket);
            return false;
        }
    }

    mConnection->Begin(nullptr);

    MPacketInfo({
        .destId = aDestId,
        .infoBits = SocketGetInfoBits(mConnection->mSocket),
        .hash = hashFile(),
    }, { aName }).Send(*mConnection);

    return true;
}

void Client::Update() {
    if (!mConnection) { return; }
    if (mUpdating) { return; }
    mUpdating = true;

    mConnection->Receive();
    mConnection->Update();

    // update peer
    for (auto& it : mPeers) {
        if (it.second) {
            it.second->Update();
        }
    }

    // copy events for processing
    std::vector<PeerEvent> mEventsCopy;
    {
        std::lock_guard<std::mutex> guard(mEventsMutex);
        for (auto& it : mEvents) {
            mEventsCopy.push_back(it);
        }
        mEvents.clear();
    }

    // process queued data on main thread
    {
        for (auto& it : mEventsCopy) {
            Peer* peer = PeerGet(it.peerId);

            switch (it.type) {
                case PEER_EVENT_STATE_CHANGED:
                    if (peer) {
                        peer->OnStateChanged(it.data.stateChanged.state);
                    }
                    break;
                case PEER_EVENT_RECV:
                    if (it.data.recv.data) {
                        if (peer) {
                            peer->OnRecv(it.data.recv.data, it.data.recv.dataSize);
                        }
                        free((void*)it.data.recv.data);
                        it.data.recv.data = nullptr;
                    }
                    break;
            }
        }
    }
    mUpdating = false;
}

void Client::Disconnect() {
    mShutdown = true;
    PeerEndAll();
    if (mConnection) {
        mConnection->Disconnect(true);
        mConnection = nullptr;
    }
}

void Client::PeerBegin(uint64_t aUserId, uint32_t aPriority) {
    mPeers[aUserId] = new Peer(this, aUserId, aPriority);
    LOG_INFO("Peer begin, count: %" PRIu64 "", (uint64_t)mPeers.size());
}

void Client::PeerEnd(uint64_t aUserId) {
    Peer* peer = mPeers[aUserId];
    if (peer) {
        peer->Disconnect();
        mPeers.erase(peer->mId);
        delete peer;
    }
    LOG_INFO("Peer end, count: %" PRIu64 "", (uint64_t)mPeers.size());
}

void Client::PeerEndAll() {
    for (auto& it : mPeers) {
        Peer* peer = it.second;
        if (peer) {
            peer->Disconnect();
            delete peer;
        }
    }
    mPeers.clear();
    LOG_INFO("Peer end all, count: %" PRIu64 "", (uint64_t)mPeers.size());
}

Peer* Client::PeerGet(uint64_t aUserId) {
    return mPeers[aUserId];
}

bool Client::PeerSend(const uint8_t* aData, size_t aDataLength) {
    bool ret = true;
    for (auto& it : mPeers) {
        Peer* peer = it.second;
        if (!peer) { continue; }
        if (!peer->Send(aData, aDataLength)) {
            ret = false;
        }
    }
    return ret && (mPeers.size() > 0);
}

bool Client::PeerSendTo(uint64_t aPeerId, const uint8_t* aData, size_t aDataLength) {
    Peer* peer = mPeers[aPeerId];
    if (!peer) { return false; }
    return peer->Send(aData, aDataLength);
}

void Client::LobbyCreate(std::string aGame, std::string aVersion, std::string aHostName, std::string aMode, uint16_t aMaxConnections, std::string aPassword, std::string aDescription) {
    MPacketLobbyCreate(
        { .maxConnections = aMaxConnections },
        { aGame.substr(0, 32), aVersion.substr(0, 32), aHostName.substr(0, 32), aMode.substr(0, 32), aPassword.substr(0, 64), aDescription.substr(0, 256) }
        ).Send(*mConnection);
}

void Client::LobbyUpdate(uint64_t aLobbyId, std::string aGame, std::string aVersion, std::string aHostName, std::string aMode, std::string aDescription) {
    MPacketLobbyUpdate(
        { .lobbyId = aLobbyId },
        { aGame.substr(0, 32), aVersion.substr(0, 32), aHostName.substr(0, 32), aMode.substr(0, 32), aDescription.substr(0, 256) }
        ).Send(*mConnection);
}

void Client::LobbyJoin(uint64_t aLobbyId, std::string aPassword) {
    MPacketLobbyJoin(
        { .lobbyId = aLobbyId },
        { aPassword.substr(0, 64) }
        ).Send(*mConnection);
}

void Client::LobbyLeave(uint64_t aLobbyId) {
    MPacketLobbyLeave(
        { .lobbyId = aLobbyId }
        ).Send(*mConnection);
}

void Client::LobbyListGet(std::string aGame, std::string aPassword) {
    MPacketLobbyListGet(
        {},
        { aGame.substr(0, 32), aPassword.substr(0, 64) }
        ).Send(*mConnection);
}
