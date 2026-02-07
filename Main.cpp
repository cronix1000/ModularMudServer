#include "Server.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "27015"

int __cdecl main(void) {
    SQLiteDatabase db;
    db.Connect("mud.db");
        // Pass the address (&db)
    GameEngine engine(&db);

        Server mudServer(&engine);
    

        if (mudServer.Start("27015") == 0) {
            mudServer.Run();
        }
        else {
            return 1;
        }


    mudServer.Stop();
    return 0;
}