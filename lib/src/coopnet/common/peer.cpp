#include <map>
#include <vector>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "libcoopnet.h"
#include "peer.hpp"
#include "client.hpp"
#include "mpacket.hpp"
#include "logging.hpp"
#include "utils.hpp"

#define PEER_TIMEOUT 45.0f /* 45 seconds */

static void sOnCandidate(juice_agent_t *agent, const char *sdp, void *user_ptr) { reinterpret_cast<Peer*>(user_ptr)->OnCandidate(sdp); }
static void sOnGatheringDone(juice_agent_t *agent, void *user_ptr) { reinterpret_cast<Peer*>(user_ptr)->OnGatheringDone(); }

static void sOnStateChanged(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    Peer* peer = (Peer*)user_ptr;

    PeerEventStateChanged stateChanged = {
        .state = state
    };

    PeerEvent event = {
        .peerId = peer->mId,
        .type = PEER_EVENT_STATE_CHANGED,
        .data = { .stateChanged = stateChanged },
    };

    std::lock_guard<std::mutex> guard(gClient->mEventsMutex);
    gClient->mEvents.push_back(event);
}

static void sOnRecv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    Peer* peer = (Peer*)user_ptr;

    PeerEventRecv recv = {
        .data = (const uint8_t*)malloc(size),
        .dataSize = size
    };
    memcpy((void*)recv.data, data, size);

    std::lock_guard<std::mutex> guard(gClient->mEventsMutex);
    gClient->mEvents.push_back({
        .peerId = peer->mId,
        .type = PEER_EVENT_RECV,
        .data = { .recv = recv },
    });
}

Peer::Peer(Client* aClient, uint64_t aId, uint32_t aPriority) {
    mId = aId;
    mPriority = aPriority;
    mLastState = JUICE_STATE_DISCONNECTED;
    mCurrentState = JUICE_STATE_DISCONNECTED;
    mTimeout = clock_elapsed() + PEER_TIMEOUT;
    mControlling = (gClient->mCurrentUserId < mId);

    juice_set_log_level(JUICE_LOG_LEVEL_NONE);

    // Agent 1: Create agent
    juice_config_t config;
    memset(&config, 0, sizeof(config));

    // STUN server example
    config.stun_server_host = aClient->mStunServer.host.c_str();
    config.stun_server_port = aClient->mStunServer.port;
    LOG_INFO("STUN server: %s, %u", config.stun_server_host, config.stun_server_port);

    // TURN server example (use your own server in production)
    if (mControlling) {
        mTurnServers = (juice_turn_server_t*)calloc(aClient->mTurnServers.size(), sizeof(juice_turn_server_t));
        if (!mTurnServers) {
            config.turn_servers = nullptr;
            config.turn_servers_count = 0;
            LOG_ERROR("Failed to allocate turn servers");
        } else {
            for (uint32_t i = 0; i < aClient->mTurnServers.size(); i++) {
                StunTurnServer* turn = &aClient->mTurnServers[i];
                mTurnServers[i].host = turn->host.c_str();
                mTurnServers[i].port = turn->port;
                mTurnServers[i].username = turn->username.c_str();
                mTurnServers[i].password = turn->password.c_str();
                LOG_INFO("TURN server: %s, %u", mTurnServers[i].host, mTurnServers[i].port);
            }
            config.turn_servers = mTurnServers;
            config.turn_servers_count = aClient->mTurnServers.size();
        }
    }

    config.cb_state_changed = sOnStateChanged;
    config.cb_candidate = sOnCandidate;
    config.cb_gathering_done = sOnGatheringDone;
    config.cb_recv = sOnRecv;
    config.user_ptr = this;
    config.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;

    mConnected = false;
    mAgent = juice_create(&config);

    // Send if it is an ICE controller
    if (mControlling) {
        SendSdp();
    }
}

void Peer::Update() {
    // Check for peer connection fail
    if (!mConnected && mPriority >= gClient->mCurrentPriority) {
        float now = clock_elapsed();
        if (now >= mTimeout) {
            mTimeout = now + PEER_TIMEOUT;

            LOG_INFO("Peer '%" PRIu64 "' failed", mId);

            MPacketPeerFailed(
                { .lobbyId = gClient->mCurrentLobbyId, .peerId = mId }
            ).Send(*gClient->mConnection);
        }
    }
}

void Peer::Connect(const char* aSdp) {
    juice_set_remote_description(mAgent, aSdp);
    if (mControlling) {
        juice_gather_candidates(mAgent);
    }

    // Send if it isn't an ICE controller
    if (!mControlling) {
        SendSdp();
    }
}

void Peer::SendSdp() {
    juice_get_local_description(mAgent, mSdp, JUICE_MAX_SDP_STRING_LEN);
    LOG_INFO("Local description (%" PRIu64 "):\n%s\n", mId, mSdp);

    MPacketPeerSdp(
        { .lobbyId = gClient->mCurrentLobbyId, .userId = mId },
        { mSdp }
    ).Send(*gClient->mConnection);
}

bool Peer::Send(const uint8_t* aData, size_t aDataLength) {
    if (!mConnected) {
        LOG_INFO("Refusing send because not yet connected to (%" PRIu64 ")\n", mId);
        return false;
    }
    juice_send(mAgent, (const char*)aData, aDataLength);
    return true;
}

void Peer::Disconnect() {
    if (mAgent) {
        LOG_INFO("Peer disconnect %" PRIu64 "", mId);
        juice_destroy(mAgent);
        mAgent = nullptr;
        mTurnServers = nullptr;
        mConnected = false;
    }
}

void Peer::CandidateAdd(const char* aSdp) {
    juice_add_remote_candidate(mAgent, aSdp);
}

void Peer::CandidateDone() {
    LOG_INFO("Remote gathering done %" PRIu64 "", mId);
    juice_set_remote_gathering_done(mAgent);
    if (!mControlling) {
        juice_gather_candidates(mAgent);
    }
}

void Peer::OnStateChanged(juice_state_t aState) {
    LOG_INFO("State change (%" PRIu64 "): %s", mId, juice_state_to_string(aState));

    bool wasConnected = (mLastState == JUICE_STATE_CONNECTED) || (mLastState == JUICE_STATE_COMPLETED);
    bool wasDisconnected = (mLastState == JUICE_STATE_DISCONNECTED) || (mLastState == JUICE_STATE_FAILED);

    mConnected = (aState == JUICE_STATE_CONNECTED) || (aState == JUICE_STATE_COMPLETED);
    bool isDisconnected = (aState == JUICE_STATE_DISCONNECTED) || (aState == JUICE_STATE_FAILED);

    if (!wasConnected && mConnected) {
        // Retrieve candidates
        char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
        char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
        if (juice_get_selected_candidates(mAgent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0) {
            LOG_INFO("Local candidate : %s\n", local);
            LOG_INFO("Remote candidate: %s\n", remote);

            if (mControlling && strstr(local, "relay") != NULL) {
                LOG_INFO("Using TURN relay server");
            } else if (!mControlling && strstr(remote, "relay") != NULL) {
                LOG_INFO("Using TURN relay server");
            }
        }

        if (gCoopNetCallbacks.OnPeerConnected) {
            gCoopNetCallbacks.OnPeerConnected(mId);
        }
    }

    if (!wasDisconnected && isDisconnected) {
        if (gCoopNetCallbacks.OnPeerDisconnected) {
            gCoopNetCallbacks.OnPeerDisconnected(mId);
        }
    }

    mLastState = mCurrentState;
    mCurrentState = aState;
}

void Peer::OnCandidate(const char* aSdp) {
    LOG_INFO("Candidate (%" PRIu64 "): %s", mId, aSdp);

    // Uncomment to force a TURN connection
    /*
    if (mControlling) {
        if (!strstr(aSdp, "relay")) {
            LOG_INFO("Rejecting non-relay!");
            return;
        }
    } else {
        if (!strstr(aSdp, "srflx")) {
            LOG_INFO("Rejecting non-srflx!");
            return;
        }
    }*/

    MPacketPeerCandidate(
        { .lobbyId = gClient->mCurrentLobbyId, .userId = mId },
        { aSdp }
    ).Send(*gClient->mConnection);
}

void Peer::OnGatheringDone() {
    LOG_INFO("Gathering done (%" PRIu64 ")", mId);
    MPacketPeerCandidateDone(
        { .lobbyId = gClient->mCurrentLobbyId, .userId = mId }
    ).Send(*gClient->mConnection);
}

void Peer::OnRecv(const uint8_t* aData, size_t aSize) {
    if (gCoopNetCallbacks.OnReceive) {
        gCoopNetCallbacks.OnReceive(mId, aData, (uint64_t)aSize);
    }
}
