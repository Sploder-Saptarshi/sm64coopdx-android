// Adapted from PeachyPeach's sm64pc-omm (now sm64ex-omm)
#include "crash_handler.h"
char gLastRemoteBhv[256] = "";

struct PcDebug gPcDebug = {
    .tags = {
        0x0000000000000000,
        0x000000000000FFFF,
        0x2D1D50FB02617949,
        0x8AEB7180FAE739EB,
        0x0CDB1A233CC71057,
        0x53D5D9880C8B278E,
        0xE8E307BE5802542E,
        0x8A3ACC4FDB4FFE45,
        0x09046C2BA3C5000D,
        0xF027964ADE989C29,
        0x076CF19655C70007,
        0x440C28A5CC404F11,
        0xE9A402C28144FD8B,
        0x9A2269E87B26BE68,
        0x0E76DE227D813019,
        0x12ABA8362D430002,
        0x0BF8F9C076430007,
        0x0BFA2492BA430011
    },
    .id = DEFAULT_ID,
    .bhvOffset = /* 0x12 */ 0,
    .debugId = 0x4BE2,
    .lastModRun = NULL,
};