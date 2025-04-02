#include <cstdint>
#include <string>
#include <algorithm>

#include "lobby.hpp"
#include "logging.hpp"
#include "utils.hpp"

// callbacks
void (*gOnLobbyJoin)(Lobby* lobby, Connection* connection) = nullptr;
void (*gOnLobbyLeave)(Lobby* lobby, Connection* connection) = nullptr;
void (*gOnLobbyDestroy)(Lobby* lobby) = nullptr;

Lobby::Lobby(Connection* aOwner, uint64_t aId, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, uint16_t aMaxConnections, std::string& aPassword, std::string& aDescription) {
    mOwner = aOwner;
    mId = aId;
    mGame = aGame;
    mVersion = aVersion;
    mHostName = aHostName;
    mMode = aMode;
    mMaxConnections = aMaxConnections;
    mPassword = aPassword;
    mDescription = aDescription;
}

Lobby::~Lobby() {
    LOG_INFO("Destroying lobby %" PRIu64 "", mId);

    for (auto& it : mConnections) {
        if (!it) { continue; }
        if (it->mLobby != this) { continue; }
        if (gOnLobbyLeave) { gOnLobbyLeave(this, it); }
        it->mLobby = nullptr;
    }
    mConnections.clear();

    if (gOnLobbyDestroy) { gOnLobbyDestroy(this); }
}

enum MPacketErrorNumber Lobby::Join(Connection* aConnection, std::string& aPassword) {
    // sanity check
    if (!aConnection) { return MERR_LOBBY_JOIN_FAILED; }
    if (aConnection->mLobby == this) { return MERR_NONE; }

    // make sure people are updated
    if (mOwner->mUpdated && !aConnection->mUpdated) { return MERR_LOBBY_JOIN_FAILED; }

    // leave older lobby
    if (aConnection->mLobby != nullptr) {
        aConnection->mLobby->Leave(aConnection);
    }

    // make sure there's room
    if (mConnections.size() >= mMaxConnections) {
        return MERR_LOBBY_JOIN_FULL;
    }

    // make sure password matches
    if (mPassword != aPassword) {
        return MERR_LOBBY_PASSWORD_INCORRECT;
    }

    auto it = std::find(mConnections.begin(), mConnections.end(), aConnection);
    if (it == mConnections.end()) {
        mConnections.push_back(aConnection);
    }
    aConnection->mLobby = this;
    aConnection->mPriority = mNextPriority++;

    if (gOnLobbyJoin) { gOnLobbyJoin(this, aConnection); }
    return MERR_NONE;
}

void Lobby::Leave(Connection* aConnection) {
    if (!aConnection) { return; }
    if (aConnection->mLobby != this) { return; }

    if (gOnLobbyLeave) { gOnLobbyLeave(this, aConnection); }

    mConnections.erase(std::remove(mConnections.begin(), mConnections.end(), aConnection), mConnections.end());

    aConnection->mLobby = nullptr;

    if (mOwner == aConnection) {
        delete this;
    }
}