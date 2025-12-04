#ifndef SHARED_H
#define SHARED_H

#include <vector>
#include <pthread.h>

struct GameRoom {
    int id;
    int hostSocket;
    int joinerSocket;
    bool isFull;
    bool isActive;
};

// Global Shared Data
extern std::vector<GameRoom> g_Games;
extern pthread_mutex_t g_LobbyMutex;

#endif