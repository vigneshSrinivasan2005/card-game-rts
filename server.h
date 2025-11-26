#ifndef SERVER_H
#define SERVER_H

#include <cstdint> 
#include <vector>


typedef struct {
    uint32_t unit_id; //4 bytes //0 means no id yet 
    uint32_t command_type; //4 bytes 1 means move 2 means attack 3 means place
    uint32_t unit_type; //4 bytes used only for place command
    double target_x; //8 bytes
    double target_y; //8 bytes
} Command;

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