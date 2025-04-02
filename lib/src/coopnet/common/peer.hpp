#pragma once

#include <stdint.h>
#include "juice/juice.h"

class Client;

typedef enum {
    PEER_EVENT_STATE_CHANGED,
    PEER_EVENT_RECV,
} PeerEventType;

typedef struct {
    juice_state_t state;
} PeerEventStateChanged;

typedef struct {
    const uint8_t* data;
    size_t dataSize;
} PeerEventRecv;

typedef union {
    PeerEventStateChanged stateChanged;
    PeerEventRecv recv;
} PeerEventData;

typedef struct {
    uint64_t peerId;
    PeerEventType type;
    PeerEventData data;
} PeerEvent;

class Peer {
    private:
        juice_agent_t* mAgent = nullptr;
        juice_turn_server_t* mTurnServers = nullptr;
        bool mConnected = false;
        juice_state_t mCurrentState = JUICE_STATE_DISCONNECTED;
        juice_state_t mLastState = JUICE_STATE_DISCONNECTED;
        uint32_t mPriority = 0;
        float mTimeout = 0;
        bool mControlling = false;

        void SendSdp();

    public:
        uint64_t mId;
        char mSdp[JUICE_MAX_SDP_STRING_LEN];

        Peer(Client* client, uint64_t aId, uint32_t aPriority);

        void Update();

        void Connect(const char* aSdp);
        void Disconnect();
        void CandidateAdd(const char* aSdp);
        void CandidateDone();
        bool Send(const uint8_t* aData, size_t aDataLength);

        void OnStateChanged(juice_state_t aState);
        void OnCandidate(const char* aSdp);
        void OnGatheringDone();
        void OnRecv(const uint8_t* aData, size_t aSize);
};
