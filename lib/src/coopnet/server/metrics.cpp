#include <fstream>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include "metrics.hpp"
#include "json.hpp"
#include "logging.hpp"

using json = nlohmann::json;

TimePeriod::TimePeriod(TimePeriod* aParent, std::string aPath, uint64_t aSeconds, uint64_t aLimit) {
    mParent = aParent;
    mPath = aPath;
    mSeconds = aSeconds;
    mLimit = aLimit;

    std::ifstream f(mPath);
    json j = json::parse(f);
    mEpoch = j["epoch"].get<uint64_t>();
    mLobbies = j["lobbies"].get<std::vector<int>>();
    mPlayers = j["players"].get<std::vector<int>>();

    while (mLobbies.size() < mPlayers.size()) { mLobbies.push_back(0); }
    while (mPlayers.size() < mLobbies.size()) { mPlayers.push_back(0); }
}

uint64_t TimePeriod::LatestEntryTime() {
    return mEpoch + (mLobbies.size() - 1) * mSeconds;
}

bool TimePeriod::NeedsEntry() {
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    uint64_t now = std::chrono::system_clock::to_time_t(nowTp);
    return (LatestEntryTime() + mSeconds < now);
}

int TimePeriod::LargestLobbiesInRange(uint64_t aStart, uint64_t aEnd) {
    int largest = 0;
    uint64_t time = mEpoch;
    for (auto it : mLobbies) {
        if (time >= aStart && time <= aEnd) {
            if (largest < it) { largest = it; }
        }
        time += mSeconds;
    }
    return largest;
}

int TimePeriod::LargestPlayersInRange(uint64_t aStart, uint64_t aEnd) {
    int largest = 0;
    uint64_t time = mEpoch;
    for (auto it : mPlayers) {
        if (time >= aStart && time <= aEnd) {
            if (largest < it) { largest = it; }
        }
        time += mSeconds;
    }
    return largest;
}

void TimePeriod::Insert(int aLobbies, int aPlayers) {
    mLobbies.push_back(aLobbies);
    mPlayers.push_back(aPlayers);
    mAltered = true;
}

void TimePeriod::Update() {
    while (mParent != nullptr && mParent->NeedsEntry()) {
        uint64_t start = mParent->LatestEntryTime();
        uint64_t end   = start + mParent->mSeconds;
        int lobbies = LargestLobbiesInRange(start, end);
        int players = LargestPlayersInRange(start, end);
        mParent->Insert(lobbies, players);
    }

    if (mParent != nullptr) {
        mParent->Update();
    }

    while (mLobbies.size() > mLimit) {
        mLobbies.erase(mLobbies.begin());
        mPlayers.erase(mPlayers.begin());
        mEpoch += mSeconds;
        mAltered = true;
    }

    if (mAltered) {
        mAltered = false;
        Save();
    }
}

void TimePeriod::Save() {
    // Create a JSON object and assign the vectors to it
    nlohmann::json j;
    j["epoch"] = mEpoch;
    j["lobbies"] = mLobbies;
    j["players"] = mPlayers;

    // Serialize the JSON object to a string
    std::string json_string = j.dump(4);

    // Write the JSON string to a file
    try {
        std::ofstream outfile(mPath);
        outfile << json_string;
        outfile.close();
    } catch (...) {
        LOG_ERROR("Failed to save %s", mPath.c_str());
    }
}

Metrics::Metrics() {
    std::ifstream file("website/hourly.json");
    if (!file.good()) {
        LOG_ERROR("Could not start metrics.");
        return;
    }

    mWeekly = new TimePeriod(nullptr, "website/weekly.json", 60 * 60 * 24 * 7, 48);
    mDaily  = new TimePeriod(mWeekly, "website/daily.json", 60 * 60 * 24, 48);
    mHourly = new TimePeriod(mDaily,  "website/hourly.json", 60 * 60, 48);

    while (mHourly->NeedsEntry()) {
        mHourly->Insert(0, 0);
    }
    mHourly->Update();
    LOG_INFO("Started metrics.");
}

void Metrics::Update(int aLobbies, int aPlayers) {
    if (mHourly == nullptr) { return; }
    if (mLobbies < aLobbies) { mLobbies = aLobbies; }
    if (mPlayers < aPlayers) { mPlayers = aPlayers; }

    if (mHourly->NeedsEntry()) {
        mHourly->Insert(mLobbies, mPlayers);
        mHourly->Update();
        mLobbies = aLobbies;
        mPlayers = aPlayers;
    }

    Save(aLobbies, aPlayers);
}

void Metrics::Save(int aLobbies, int aPlayers) {
    // Create a JSON object and assign the vectors to it
    nlohmann::json j;
    j["lobbies"] = aLobbies;
    j["players"] = aPlayers;

    // Serialize the JSON object to a string
    std::string json_string = j.dump(4);

    // Write the JSON string to a file
    try {
        std::ofstream outfile("website/current.json");
        outfile << json_string;
        outfile.close();
    } catch (...) {
        LOG_ERROR("Failed to save %s", "website/current.json");
    }
}
