#ifndef SHARED_H
#define SHARED_H

#include <vector>
#include <string>
#include <pthread.h>
#include <cstdint>
#include <unordered_map>
#include <cstdio>
#include <sstream>
#include <cstring>
#include <iostream>
#include <sys/socket.h>

using namespace std;

//  RTS PROTOCOL STRUCTURES 
#pragma pack(push, 1)
struct Command {
    uint32_t unit_id;      // 0 means no id yet 
    uint32_t command_type; // 1=Move, 2=Attack, 3=Place, 4=EndGame
    uint32_t unit_type;    // Used only for place command
    double target_x;
    double target_y;
};
#pragma pack(pop)

//  LOBBY STRUCTURES 
struct GameRoom {
    int id;
    int hostSocket;
    int joinerSocket;
    bool isFull;
    bool isActive;
};

struct User {
    string username;
    int numWins;
};


//  GLOBAL SHARED STATE 
//USER MANAGEMENT
extern unordered_map<string, User> g_AllUsers; //maps username to user
extern unordered_map<int, User> connected_Users; // maps socket to user
// use both g_games and connected_Users to figure out who is in lobby


//GAME MANAGEMENT
extern vector<GameRoom> g_Games;

//MULTITHREADING MANAGEMENT
extern pthread_mutex_t g_LobbyMutex;

// Method to load and save userState
void getAllUsers();
void saveAllUsers();

//method to send message to all users in lobby
void sendToAllInLobby(const string& message);


//helpers for all
bool SendText(int sock, string msg);


#endif // SHARED_H