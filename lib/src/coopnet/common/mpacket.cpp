#include <cstdint>
#include <map>
#include <string>
#include <errno.h>
#include "libcoopnet.h"
#include "socket.hpp"
#include "mpacket.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "client.hpp"
#include "utils.hpp"

static MPacket* sPacketByType[MPACKET_MAX] = {
    new MPacket(),
    new MPacketJoined(),
    new MPacketLobbyCreate(),
    new MPacketLobbyCreated(),
    new MPacketLobbyUpdate(),
    new MPacketLobbyJoin(),
    new MPacketLobbyJoined(),
    new MPacketLobbyLeave(),
    new MPacketLobbyLeft(),
    new MPacketLobbyListGet(),
    new MPacketLobbyListGot(),
    new MPacketLobbyListFinish(),
    new MPacketPeerSdp(),
    new MPacketPeerCandidate(),
    new MPacketPeerCandidateDone(),
    new MPacketPeerFailed(),
    new MPacketStunTurn(),
    new MPacketError(),
    new MPacketKeepAlive(),
    new MPacketInfo(),
    new MPacketLoadBalance(),
};

void MPacket::Send(Connection& connection) {
    // make sure its connected
    if (!connection.mActive) {
        return;
    }

    // figure out string size
    int64_t stringSize = 0;
    for (const auto& s : mStringData) {
        int64_t size = strlen(s.c_str());
        if (size >= UINT16_MAX) {
            LOG_ERROR("Tried to include a string that was too large: %" PRId64 "", size);
            return;
        }
        stringSize += sizeof(uint16_t) + size;
    }
    if (stringSize >= UINT16_MAX) {
        LOG_ERROR("Tried to include a total string size that was too large: %" PRId64 "", stringSize);
        return;
    }

    // sanity check void data size
    if (mVoidDataSize >= UINT16_MAX) {
        LOG_ERROR("Tried to include a total void data size that was too large: %" PRId64 "", mVoidDataSize);
        return;
    }

    // setup packet header
    MPacketImplSettings impl = GetImplSettings();
    MPacketHeader pHeader = {
        .packetType = static_cast<uint16_t>(impl.packetType),
        .dataSize = (uint16_t)mVoidDataSize,
        .stringSize = (uint16_t)stringSize
    };

    // figure out data size
    int64_t dataSize = sizeof(MPacketHeader) + pHeader.dataSize + pHeader.stringSize;
    if (dataSize > (int64_t)MPACKET_MAX_SIZE || dataSize >= (int64_t)UINT16_MAX) {
        LOG_ERROR("Packet size exceeded max size (%" PRIu64 " > %" PRIu64 ")", (uint64_t)dataSize, (uint64_t)MPACKET_MAX_SIZE);
        return;
    }

    // allocate data
    uint8_t* data = (uint8_t*) malloc(dataSize);
    if (!data) {
        LOG_ERROR("Error allocating data");
        return;
    }

    // fill header
    uint8_t* d = &data[0];
    memcpy(d, &pHeader, sizeof(MPacketHeader));
    d += sizeof(MPacketHeader);

    // fill data
    memcpy(d, mVoidData, mVoidDataSize);
    d += mVoidDataSize;

    // fill strings
    for (const auto& s : mStringData) {
        const char* c = s.c_str();

        // fill string length
        uint16_t slength = strlen(c);
        *((uint16_t*)d) = slength;
        d += sizeof(uint16_t);

        // fill string
        for (uint16_t i = 0; i < slength; i++) {
            *d = *c;
            d++;
            c++;
        }
    }

    // send data buffer
    SOCKET_RESET_ERROR();
    int sent = sendto(connection.mSocket, (char*)&data[0], dataSize, MSG_NOSIGNAL, (const sockaddr*)&connection.mAddress, sizeof(struct sockaddr_in));
    int rc = SOCKET_LAST_ERROR;
    if (rc != SOCKET_EAGAIN && rc != 0) {
        LOG_ERROR("Socket sendto error: %d", rc);
    }
    //LOG_INFO("SENT: %d, %d, %" PRId64 "", sent, rc, dataSize);

    // debug print packet
    /*LOG_INFO("Sent data:");
    for (size_t i = 0; i < dataSize; i++) {
        printf("  %02X", data[i]);
    }
    printf("\n");*/

    // free data buffer
    free(data);

    // check for send error
    if (sent < 0) {
        LOG_ERROR("Error sending data (%d)!", rc);
    }

    // check for data size error
    if (sent != (int)dataSize) {
        LOG_ERROR("Error sending data, did not send all bytes (%d != %" PRIu64 ")!", sent, (uint64_t)dataSize);
    }

    // update last send time
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    connection.mLastSendTime = std::chrono::system_clock::to_time_t(nowTp);
}

void MPacket::Send(Lobby& lobby) {
    for (auto& it : lobby.mConnections) {
        Send(*it);
    }
}

void MPacket::Process(Connection* connection, uint8_t* aData) {
    // extract variables from data
    MPacketHeader header = *(MPacketHeader*)aData;
    void* voidData = &aData[sizeof(MPacketHeader)];
    void* stringData = &aData[sizeof(MPacketHeader) + header.dataSize];
    bool parseError = false;

    /*LOG_INFO("Processing data:");
    for (size_t i = 0; i < (size_t)(sizeof(MPacketHeader) + header.dataSize + header.stringSize); i++) {
        printf("  %02X", ((uint8_t*)aData)[i]);
    }
    printf("\n");*/

    // sanity check packet type
    if (header.packetType >= MPACKET_MAX || header.packetType == MPACKET_NONE) {
        LOG_ERROR("Received an unknown packet type: %u, data size: %u, string size: %u", header.packetType, header.dataSize, header.stringSize);
        return;
    }

    // receive packet
    MPacket* packet = sPacketByType[header.packetType];

    // sanity check data size
    int64_t packetSize = packet->mVoidDataSize;
    if (header.dataSize > 0 && (int64_t)header.dataSize != packet->mVoidDataSize) {
        if (header.packetType == MPACKET_INFO && (int64_t)header.dataSize == packet->mRequiredSize) {
            packetSize = packet->mRequiredSize;
        } else {
            LOG_ERROR("Received the wrong data size: %u != %" PRId64 " (required %" PRId64 ") (packetType %u)", header.dataSize, packet->mVoidDataSize, packet->mRequiredSize, header.packetType);
            return;
        }
    }

    // receive data
    memcpy(packet->mVoidData, voidData, packetSize);

    // receive strings
    packet->mStringData.clear();
    uint8_t* c = (uint8_t*)stringData;
    uint8_t* climit = c + header.stringSize;
    while (c < climit) {
        // retrieve string length
        uint16_t length = *(uint16_t*)c;
        c += sizeof(uint16_t);
        if (c >= climit) {
            if (length == 0) {
                packet->mStringData.push_back("");
                break;
            }
            parseError = true;
            break;
        }

        // allocate string
        char* cstr = (char*)malloc(length + 1);
        if (!cstr) {
            LOG_ERROR("Failed to allocate string");
            parseError = true;
            break;
        }

        // fill string
        snprintf(cstr, length + 1, "%s", c);
        c += length;

        // remember string
        packet->mStringData.push_back(cstr);
        free(cstr);
    }
    if (c != climit) { parseError = true; }

    // check impl settings
    MPacketImplSettings impl = packet->GetImplSettings();
    if (header.packetType != impl.packetType) {
        LOG_ERROR("Received packet type mismatch: %u != %u", header.packetType, impl.packetType);
        return;
    }
    if (packet->mStringData.size() != impl.stringCount) {
        LOG_ERROR("Received packet string count mismatch: %" PRIu64 " != %u", (uint64_t)packet->mStringData.size(), impl.stringCount);
        return;
    }
    if (gServer && impl.sendType == MSEND_TYPE_SERVER) {
        LOG_ERROR("Received server packet while being a server!");
        return;
    }
    if (gClient && impl.sendType == MSEND_TYPE_CLIENT) {
        LOG_ERROR("Received client packet while being a client!");
        return;
    }

    // receive the packet
    if (parseError) {
        LOG_ERROR("Packet parse error!");
    } else {
        bool ret = packet->Receive(connection);
        if (!ret) { LOG_ERROR("Packet receive error %u!", header.packetType); }
    }
}

void MPacket::Read(Connection* connection, uint8_t* aData, int64_t* aDataSize, int64_t aMaxDataSize) {
    while (true) {
        MPacketHeader header = *(MPacketHeader*)aData;
        int64_t totalSize = sizeof(MPacketHeader) + header.dataSize + header.stringSize;

        // check the received size
        if (*aDataSize < totalSize) {
            return;
        }

        // process
        MPacket::Process(connection, aData);

        // shift the data array
        int64_t j = 0;
        for (int64_t i = totalSize; i < *aDataSize; i++) {
            aData[j++] = aData[i];
        }

        // reduce the current buffer size
        *aDataSize -= totalSize;
        if (*aDataSize < 0) {
            LOG_ERROR("Data size below zero!");
            *aDataSize = 0;
        }
    }
}

bool MPacketJoined::Receive(Connection* connection) {
    LOG_INFO("MPACKET_JOINED received: userID %" PRIu64 ", version %u", mData.userId, mData.version);
    if (mData.version != MPACKET_PROTOCOL_VERSION) {
        if (gCoopNetCallbacks.OnError) {
            gCoopNetCallbacks.OnError(MERR_COOPNET_VERSION, mData.version);
        }
        gClient->Disconnect();
        return false;
    }

    gClient->mCurrentUserId = mData.userId;

    if (gCoopNetCallbacks.OnConnected) {
        gCoopNetCallbacks.OnConnected(mData.userId);
    }

    return true;
}

bool MPacketLobbyCreate::Receive(Connection* connection) {
    std::string game        = mStringData[0].substr(0, 32);
    std::string version     = mStringData[1].substr(0, 32);
    std::string hostName    = mStringData[2].substr(0, 32);
    std::string mode        = mStringData[3].substr(0, 32);
    std::string password    = mStringData[4].substr(0, 64);
    std::string description = mStringData[5].substr(0, 256);

    LOG_INFO("MPACKET_LOBBY_CREATE received: game '%s', version '%s', hostName '%s', mode '%s', maxconnections %u, password '%s'",
        game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), mData.maxConnections, password.c_str());
    gServer->LobbyCreate(connection, game, version, hostName, mode, mData.maxConnections, password, description);

    return true;
}

bool MPacketLobbyCreated::Receive(Connection* connection) {
    std::string game     = mStringData[0].substr(0, 32);
    std::string version  = mStringData[1].substr(0, 32);
    std::string hostName = mStringData[2].substr(0, 32);
    std::string mode     = mStringData[3].substr(0, 32);

    LOG_INFO("MPACKET_LOBBY_CREATED received: lobbyId %" PRIu64 ", game '%s', version '%s', hostName '%s', mode '%s', maxConnections %" PRIu64 "",
        mData.lobbyId, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), mData.maxConnections);

    if (gCoopNetCallbacks.OnLobbyCreated) {
        gCoopNetCallbacks.OnLobbyCreated(mData.lobbyId, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), mData.maxConnections);
    }

    return true;
}

bool MPacketLobbyUpdate::Receive(Connection* connection) {
    std::string game        = mStringData[0].substr(0, 32);
    std::string version     = mStringData[1].substr(0, 32);
    std::string hostName    = mStringData[2].substr(0, 32);
    std::string mode        = mStringData[3].substr(0, 32);
    std::string description = mStringData[4].substr(0, 256);

    LOG_INFO("MPACKET_LOBBY_UPDATE received: lobbyId %" PRIu64 ", game '%s', version '%s', hostName '%s', mode '%s'",
        mData.lobbyId, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str());
    gServer->LobbyUpdate(connection, mData.lobbyId, game, version, hostName, mode, description);

    return true;
}

bool MPacketLobbyJoin::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_JOIN received: lobbyId %" PRIu64 "", mData.lobbyId);

    std::string password = mStringData[0].substr(0, 64);

    Lobby* lobby = gServer->LobbyGet(mData.lobbyId);
    if (!lobby) {
        MPacketError({ .errorNumber = MERR_LOBBY_NOT_FOUND, .tag = mData.lobbyId }).Send(*connection);
        return false;
    }

    enum MPacketErrorNumber rc = lobby->Join(connection, password);
    if (rc != MERR_NONE) {
        MPacketError({ .errorNumber = (uint16_t)rc, .tag = mData.lobbyId }).Send(*connection);
        return false;
    }

    return true;
}

bool MPacketLobbyJoined::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_JOINED received: lobbyId %" PRIu64 ", userId %" PRIu64 ", priority %u, ownerId %" PRIu64 ", destId %" PRIu64 "",
        mData.lobbyId, mData.userId, mData.priority, mData.ownerId, mData.destId);

    if (mData.userId == gClient->mCurrentUserId) {
        gClient->mCurrentLobbyId = mData.lobbyId;
        gClient->mCurrentPriority = mData.priority;
    } else if (mData.lobbyId == gClient->mCurrentLobbyId) {
        gClient->PeerBegin(mData.userId, mData.priority);
    } else {
        LOG_ERROR("Received 'joined' for the wrong lobby");
        return false;
    }

    if (gCoopNetCallbacks.OnLobbyJoined) {
        gCoopNetCallbacks.OnLobbyJoined(mData.lobbyId, mData.userId, mData.ownerId, mData.destId);
    }

    return true;
}

bool MPacketLobbyLeave::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LEAVE received: lobbyId %" PRIu64 "", mData.lobbyId);

    Lobby* lobby = gServer->LobbyGet(mData.lobbyId);
    if (!lobby) {
        MPacketError({ .errorNumber = MERR_LOBBY_NOT_FOUND, .tag = mData.lobbyId }).Send(*connection);
        return false;
    }

    lobby->Leave(connection);

    return true;
}

bool MPacketLobbyLeft::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LEFT received: lobbyId %" PRIu64 ", userId %" PRIu64 "", mData.lobbyId, mData.userId);

    if (mData.userId == gClient->mCurrentUserId) {
        gClient->mCurrentLobbyId = 0;
        gClient->mCurrentPriority = 0;
        gClient->PeerEndAll();
    } else if (mData.lobbyId == gClient->mCurrentLobbyId) {
        gClient->PeerEnd(mData.userId);
    } else {
        LOG_ERROR("Received 'left' for the wrong lobby");
    }

    if (gCoopNetCallbacks.OnLobbyLeft) {
        gCoopNetCallbacks.OnLobbyLeft(mData.lobbyId, mData.userId);
    }

    return true;
}

bool MPacketLobbyListGet::Receive(Connection* connection) {
    std::string game     = mStringData[0].substr(0, 32);
    std::string password = mStringData[1].substr(0, 64);
    LOG_INFO("MPACKET_LOBBY_LIST_GET received: game '%s'", game.c_str());
    gServer->LobbyListGet(*connection, game, password);
    return true;
}

bool MPacketLobbyListGot::Receive(Connection* connection) {
    std::string game        = mStringData[0].substr(0, 32);
    std::string version     = mStringData[1].substr(0, 32);
    std::string hostName    = mStringData[2].substr(0, 32);
    std::string mode        = mStringData[3].substr(0, 32);
    std::string description = mStringData[4].substr(0, 256);
    LOG_INFO("MPACKET_LOBBY_LIST_GOT received: lobbyId %" PRIu64 ", ownerId %" PRIu64 ", connections %u/%u, game '%s', version '%s', hostname '%s', mode '%s'",
        mData.lobbyId, mData.ownerId, mData.connections, mData.maxConnections, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str());

    if (gCoopNetCallbacks.OnLobbyListGot) {
        gCoopNetCallbacks.OnLobbyListGot(mData.lobbyId, mData.ownerId, mData.connections, mData.maxConnections, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), description.c_str());
    }

    return true;
}

bool MPacketLobbyListFinish::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LIST_FINISH received");

    if (gCoopNetCallbacks.OnLobbyListFinish) {
        gCoopNetCallbacks.OnLobbyListFinish();
    }

    return true;
}

bool MPacketPeerSdp::Receive(Connection *connection) {
    std::string& sdp = mStringData[0];
    LOG_INFO("MPACKET_PEER_SDP received: lobbyId %" PRIu64 ", userId %" PRIu64 ", sdp '%s'", mData.lobbyId, mData.userId, sdp.c_str());
    if (gServer) {
        Connection* other = gServer->ConnectionGet(mData.userId);

        if (!other) {
            LOG_ERROR("Could not find user: %" PRIu64 "", mData.userId);
            return false;
        }

        MPacketPeerSdp({
           .lobbyId = mData.lobbyId,
           .userId = connection->mId
        }, mStringData).Send(*other);

        return true;
    }

    if (gClient) {
        Peer* peer = gClient->PeerGet(mData.userId);

        if (!peer) {
            LOG_ERROR("Could not find peer: %" PRIu64 "", mData.userId);
            return false;
        }

        peer->Connect(sdp.c_str());
        return true;
    }

    LOG_ERROR("Received peer sdp without being server or client");
    return false;
}

bool MPacketPeerCandidate::Receive(Connection *connection) {
    std::string& sdp = mStringData[0];
    LOG_INFO("MPACKET_PEER_CANDIDATE received: lobbyId %" PRIu64 ", userId %" PRIu64 ", sdp '%s'", mData.lobbyId, mData.userId, sdp.c_str());
    if (gServer) {
        Connection* other = gServer->ConnectionGet(mData.userId);

        if (!other) {
            LOG_ERROR("Could not find user: %" PRIu64 "", mData.userId);
            return false;
        }

        MPacketPeerCandidate({
           .lobbyId = mData.lobbyId,
           .userId = connection->mId
        }, mStringData).Send(*other);

        return true;
    }

    if (gClient) {
        Peer* peer = gClient->PeerGet(mData.userId);

        if (!peer) {
            LOG_ERROR("Could not find peer: %" PRIu64 "", mData.userId);
            return false;
        }

        peer->CandidateAdd(sdp.c_str());
        return true;
    }

    LOG_ERROR("Received peer sdp without being server or client");
    return false;
}

bool MPacketPeerCandidateDone::Receive(Connection *connection) {
    LOG_INFO("MPACKET_PEER_CANDIDATE_DONE received: lobbyId %" PRIu64 ", userId %" PRIu64 "", mData.lobbyId, mData.userId);
    if (gServer) {
        Connection* other = gServer->ConnectionGet(mData.userId);

        if (!other) {
            LOG_ERROR("Could not find user: %" PRIu64 "", mData.userId);
            return false;
        }

        MPacketPeerCandidateDone({
           .lobbyId = mData.lobbyId,
           .userId = connection->mId
        }).Send(*other);

        return true;
    }

    if (gClient) {
        Peer* peer = gClient->PeerGet(mData.userId);

        if (!peer) {
            LOG_ERROR("Could not find peer: %" PRIu64 "", mData.userId);
            return false;
        }

        peer->CandidateDone();
        return true;
    }

    LOG_ERROR("Received peer sdp without being server or client");
    return false;
}

bool MPacketPeerFailed::Receive(Connection *connection) {
    LOG_INFO("MPACKET_PEER_FAILED received: lobbyId %" PRIu64 ", peerId %" PRIu64 "", mData.lobbyId, mData.peerId);
    // make sure client is still in this lobby
    if (!connection->mLobby || connection->mLobby->mId != mData.lobbyId) {
        LOG_ERROR("Peer failed, but the one that saw the failure is no longer in the lobby");
        return false;
    }

    // make sure peer is still in this lobby
    Connection* peer = gServer->ConnectionGet(mData.peerId);
    if (!peer || !peer->mLobby || peer->mLobby->mId != mData.lobbyId) {
        LOG_ERROR("Peer failed, but the peer of the one that saw the failure is no longer in the lobby");
        return false;
    }

    // check the priority of the two connections
    if (peer->mPriority <= connection->mPriority) {
        LOG_ERROR("Peer failed, but the priority was incorrect");
        return false;
    }

    // inform them
    MPacketError({ .errorNumber = MERR_PEER_FAILED, .tag = connection->mId }).Send(*peer);

    // kick them
    connection->mLobby->Leave(peer);

    return true;
}

bool MPacketStunTurn::Receive(Connection* connection) {
    std::string host     = mStringData[0];
    std::string username = mStringData[1];
    std::string password = mStringData[2];
    LOG_INFO("MPACKET_STUN_TURN received: isStun %u, host '%s', port %u, username '%s', password '%s'", mData.isStun, host.c_str(), mData.port, username.c_str(), password.c_str());

    if (mData.isStun) {
        gClient->mStunServer.host = host;
        gClient->mStunServer.port = mData.port;
    } else {
        gClient->mTurnServers.push_back({
            .host = host,
            .username = username,
            .password = password,
            .port = mData.port,
        });
    }

    return true;
}

bool MPacketError::Receive(Connection* connection) {
    LOG_INFO("MPACKET_ERROR received: errno %u", mData.errorNumber);
    if (gCoopNetCallbacks.OnError) {
        gCoopNetCallbacks.OnError((enum MPacketErrorNumber)mData.errorNumber, mData.tag);
    }
    return true;
}

bool MPacketKeepAlive::Receive(Connection *connection) {
    LOG_INFO("[%" PRIu64 "] MPACKET_KEEP_ALIVE received", connection->mId);
    return true;
}

bool MPacketInfo::Receive(Connection *connection) {
    std::string name = mStringData[0];
    LOG_INFO("[%" PRIu64 "] MPACKET_INFO received: name '%s', destId %" PRIu64 ", infoBits %" PRIu64 "", connection->mId, name.c_str(), mData.destId, mData.infoBits);
    if (gCoopNetCallbacks.OnReceiveInfoBits) {
        gCoopNetCallbacks.OnReceiveInfoBits(connection, mData.destId, mData.infoBits, name.c_str());
    }
    return true;
}

bool MPacketLoadBalance::Receive(Connection *connection) {
    std::string host = mStringData[0];
    LOG_INFO("MPACKET_LOAD_BALANCE received: host %s, port %u", host.c_str(), mData.port);
    if (gCoopNetCallbacks.OnLoadBalance) {
        gCoopNetCallbacks.OnLoadBalance(host.c_str(), mData.port);
    }
    return false;
}
