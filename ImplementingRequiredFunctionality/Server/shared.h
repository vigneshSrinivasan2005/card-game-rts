#ifndef SHARED_H
#define SHARED_H

#include <vector>
#include <string>
#include <pthread.h>
#include <cstdint>

// --- RTS PROTOCOL STRUCTURES ---
#pragma pack(push, 1)
struct Command {
    uint32_t unit_id;      // 0 means no id yet 
    uint32_t command_type; // 1=Move, 2=Attack, 3=Place, 4=EndGame
    uint32_t unit_type;    // Used only for place command
    double target_x;
    double target_y;
};
#pragma pack(pop)

// --- LOBBY STRUCTURES ---
struct GameRoom {
    int id;
    int hostSocket;
    int joinerSocket;
    bool isFull;
    bool isActive;
};

// --- GLOBAL SHARED STATE ---
extern std::vector<GameRoom> g_Games;
extern pthread_mutex_t g_LobbyMutex;

#endif // SHARED_H