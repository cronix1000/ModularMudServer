#include "Server.h"
#include <thread>
#include "GameEngine.h"
#include "GameContext.h"
#include "ClientInput.h"
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "27015"

int __cdecl main(void) {
   
    GameContext ctx;
	ThreadSafeQueue<ClientInput> inputQueue;
    GameEngine engine(ctx, inputQueue);
    Server server(ctx, &engine, inputQueue);
	server.Start(DEFAULT_PORT);
    std::thread networkThread([&server]() {
        server.Run();
        });
    networkThread.detach();

    // 2. Run the Game Engine on the Main Thread
    // This is your "New Loop"
    const int TICKS_PER_SECOND = 30;
    const int SKIP_TICKS = 1000 / TICKS_PER_SECOND;

    while (engine.IsRunning()) {
        auto next_game_tick = GetTickCount64() + SKIP_TICKS;

        engine.ProcessInputs();

        // B. Update Game World
        engine.Update(0.033f);

        // C. Sleep to maintain framerate
        int sleep_time = next_game_tick - GetTickCount64();
        if (sleep_time > 0) {
            Sleep(sleep_time);
        }
    }

    return 0;
}