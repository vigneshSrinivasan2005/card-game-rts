#ifndef SERVER_H
#define SERVER_H

#include <cstdint> // For uint32_t
#include <vector>
#include <iostream>

// --- SHARED DATA STRUCTURE ---

// FIX: Add pragma pack(1) to match the client's struct definition.
// This removes padding and ensures sizeof(Command) is exactly 28 bytes.
// Without this, Server sees 32 bytes and Client sees 28 bytes, causing a deadlock.
#pragma pack(push, 1)
typedef struct {
    uint32_t unit_id;      // 4 bytes: Entity ID (0 if new, assigned by server)
    uint32_t command_type; // 4 bytes: e.g., 1=Move, 3=Place
    uint32_t unit_type;    // 4 bytes: The Card/Unit Type ID (Used only for Place command)
    double target_x;       // 8 bytes
    double target_y;       // 8 bytes
} Command; // Total Size: 28 bytes
#pragma pack(pop)


// --- GLOBAL CONSTANTS ---
const int SERVER_PORT = 8080;
const int MAX_PLAYERS = 2;

// --- FUNCTION PROTOTYPES ---
bool ListenForPlayers(int port);


// Core synchronization loop
void LockstepSyncLoop();

// Reliable I/O helpers (POSIX Sockets use int for handles)
bool SendData(int sock, const char* buffer, int size);
bool RecvData(int sock, char* buffer, int expected_size);

#endif // SERVER_H