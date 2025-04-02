#pragma once
#include <string>
#include <vector>

class TimePeriod {
    private:
        TimePeriod* mParent = nullptr;
        std::string mPath = "";
        uint64_t mSeconds = 0;
        uint64_t mLimit = 0;
        uint64_t mEpoch = 0;
        bool mAltered = false;
        std::vector<int> mLobbies;
        std::vector<int> mPlayers;
    public:
        TimePeriod(TimePeriod* parent, std::string aPath, uint64_t aSeconds, uint64_t aLimit);
        uint64_t LatestEntryTime();
        bool NeedsEntry();
        int LargestLobbiesInRange(uint64_t aStart, uint64_t aEnd);
        int LargestPlayersInRange(uint64_t aStart, uint64_t aEnd);
        void Insert(int aLobbies, int aPlayers);
        void Update();
        void Save();
};

class Metrics {
    private:
        TimePeriod* mWeekly = nullptr;
        TimePeriod* mDaily = nullptr;
        TimePeriod* mHourly = nullptr;
        int mLobbies = 0;
        int mPlayers = 0;
    public:
        Metrics();
        void Update(int aLobbies, int aPlayers);
        void Save(int aLobbies, int aPlayers);
};
