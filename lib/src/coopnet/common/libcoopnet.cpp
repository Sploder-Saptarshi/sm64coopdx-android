#include "client.hpp"
#include "libcoopnet.h"

CoopNetCallbacks gCoopNetCallbacks = { 0 };
CoopNetSettings gCoopNetSettings = { 0 };

bool coopnet_is_connected(void) {
    return (gClient && gClient->mConnection && gClient->mConnection->mActive);
}

CoopNetRc coopnet_begin(const char* aHost, uint32_t aPort, const char* aName, uint64_t aDestId) {
    if (gClient) { return COOPNET_OK; }

    gClient = new Client();
    bool ret = gClient->Begin(aHost, aPort, aName, aDestId);
    
    if (!ret) {
        coopnet_shutdown();
        coopnet_update();
    }

    return ret
        ? COOPNET_OK
        : COOPNET_FAILED;
}

CoopNetRc coopnet_shutdown(void) {
    if (!gClient) { return COOPNET_DISCONNECTED; }

    gClient->mShutdown = true;

    return COOPNET_OK;
}

CoopNetRc coopnet_update(void) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->Update();
    if (!gClient) { return COOPNET_DISCONNECTED; }

    if (gClient->mShutdown) {
        delete gClient;
        gClient = nullptr;
    }
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_create(const char* aGame, const char* aVersion, const char* aHostName, const char* aMode, uint16_t aMaxConnections, const char* aPassword, const char* aDescription) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyCreate(aGame, aVersion, aHostName, aMode, aMaxConnections, aPassword, aDescription);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_update(uint64_t aLobbyId, const char* aGame, const char* aVersion, const char* aHostName, const char* aMode, const char* aDescription) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyUpdate(aLobbyId, aGame, aVersion, aHostName, aMode, aDescription);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_join(uint64_t aLobbyId, const char* aPassword) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyJoin(aLobbyId, aPassword);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_leave(uint64_t aLobbyId) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyLeave(aLobbyId);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_list_get(const char* aGame, const char* aPassword) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyListGet(aGame, aPassword);
    return COOPNET_OK;
}

CoopNetRc coopnet_send(const uint8_t* aData, uint64_t aDataLength) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    return gClient->PeerSend(aData, aDataLength)
        ? COOPNET_OK
        : COOPNET_FAILED;
}

CoopNetRc coopnet_send_to(uint64_t aPeerId, const uint8_t* aData, uint64_t aDataLength) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    return gClient->PeerSendTo(aPeerId, aData, aDataLength)
        ? COOPNET_OK
        : COOPNET_FAILED;
}

CoopNetRc coopnet_unpeer(uint64_t aPeerId) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    Peer* peer = gClient->PeerGet(aPeerId);
    if (!peer) {
        return COOPNET_FAILED;
    }
    peer->Disconnect();
    return COOPNET_OK;
}
