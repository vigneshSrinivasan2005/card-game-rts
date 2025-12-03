#ifndef SERVER_H
#define SERVER_H

#include <cstdint> 
#include <vector>

// --- Command Structure ---
// This must match the server's 28-byte layout
#pragma pack(push, 1)
struct Command {
    uint32_t unit_id;
    uint32_t command_type;
    uint32_t unit_type;
    double   target_x;
    double   target_y;
};
#pragma pack(pop)

// --- GLOBAL CONSTANTS ---

const int SERVER_PORT = 8080;
const int MAX_PLAYERS = 2;

// --- FUNCTION PROTOTYPES (Used internally by server.cpp) ---

// Setup and connection functions
bool ListenForPlayers(int port);

// Core synchronization loop
void LockstepSyncLoop();

// Reliable I/O helpers (POSIX Sockets use int for handles)
bool SendData(int sock, const char* buffer, int size);
bool RecvData(int sock, char* buffer, int expected_size);

#endif // SERVER_H