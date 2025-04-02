#include <iostream>
#include "server.hpp"
#include "metrics.hpp"
#include "extra/server_extra.hpp"
#include "sha2.hpp"

#define PORT 34197
#define EXIT_FAILURE 1

int main(int argc, char *argv[]) {
    Metrics metrics;

    gServer = new Server();

    server_extra_init();
    gCoopNetCallbacks.DestIdFunction = sha224_u64;

    if (!gServer->Begin(PORT)) {
        exit(EXIT_FAILURE);
    }

    while (true) {
        metrics.Update(gServer->LobbyCount(), gServer->PlayerCount());
        server_extra_update();
        std::this_thread::sleep_for(std::chrono::milliseconds(60 * 1000));
    }

    return 0;
}