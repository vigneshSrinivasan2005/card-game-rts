#include "lobby.h"
#include "game_instance.h"
#include "shared.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;

// Helper to send text with newline
static bool SendText(int sock, string msg) {
    msg += "\n";
    return send(sock, msg.c_str(), msg.length(), 0) > 0;
}

// Global ID counter for rooms
static int g_GameIDCounter = 1;

void* HandleClientLobby(void* arg) {
    int mySock = *(int*)arg;
    delete (int*)arg;
    
    char buffer[1024];
    bool inLobby = true;
    bool shouldCloseSocket = true; // Default to true, set to false if we pass socket to Game

    SendText(mySock, "WELCOME. Commands: REGISTER <user> <pass>, LIST, CREATE, JOIN <id>");

    while (inLobby) {
        memset(buffer, 0, 1024);
        int bytes = recv(mySock, buffer, 1024, 0);
        if (bytes <= 0) break; 

        string input(buffer);
        stringstream ss(input);
        string cmd;
        ss >> cmd;

        // --- 1. REGISTER ---
        if (cmd == "REGISTER") {
            string user, pass;
            ss >> user >> pass;
            SendText(mySock, "OK Registered " + user);
        }
        // --- 2. LIST ---
        else if (cmd == "LIST") {
            pthread_mutex_lock(&g_LobbyMutex);
            string list = "GAMES:\n";
            for (const auto& g : g_Games) {
                if (!g.isFull && g.isActive) {
                    list += "ID: " + to_string(g.id) + " | Status: WAIT\n";
                }
            }
            pthread_mutex_unlock(&g_LobbyMutex);
            SendText(mySock, list);
        }
        // --- 3. CREATE ---
        else if (cmd == "CREATE") {
            pthread_mutex_lock(&g_LobbyMutex);
            int newID = g_GameIDCounter++;
            GameRoom room = { newID, mySock, -1, false, true };
            g_Games.push_back(room);
            pthread_mutex_unlock(&g_LobbyMutex);

            SendText(mySock, "CREATED " + to_string(newID) + " WAIT...");

            // HOST WAITING LOOP
            while (true) {
                usleep(100000); // 100ms polling
                pthread_mutex_lock(&g_LobbyMutex);
                
                GameRoom* myRoom = nullptr;
                for (auto& g : g_Games) {
                    if (g.id == newID) { myRoom = &g; break; }
                }

                if (myRoom && myRoom->isFull) {
                    // Match found!
                    SendText(mySock, "MATCH_START");
                    
                    // Host thread takes over as the Game Server thread
                    MatchArgs* args = new MatchArgs{ myRoom->hostSocket, myRoom->joinerSocket };
                    
                    pthread_mutex_unlock(&g_LobbyMutex);
                    
                    // TRANSITION TO GAME
                    HandleMatch(args); 
                    
                    inLobby = false;
                    shouldCloseSocket = false; // HandleMatch closes them
                    break;
                }
                pthread_mutex_unlock(&g_LobbyMutex);
            }
        }
        // --- 4. JOIN ---
        else if (cmd == "JOIN") {
            int joinID;
            ss >> joinID;
            
            pthread_mutex_lock(&g_LobbyMutex);
            bool found = false;
            for (auto& g : g_Games) {
                if (g.id == joinID && !g.isFull) {
                    g.joinerSocket = mySock;
                    g.isFull = true;
                    found = true;
                    
                    // Notify Joiner
                    SendText(mySock, "MATCH_START");
                    
                    // The Joiner thread RETIRES here. 
                    // The Host thread will run the game for us.
                    inLobby = false;
                    shouldCloseSocket = false; // Do not close, Host thread owns it now
                    break;
                }
            }
            pthread_mutex_unlock(&g_LobbyMutex);
            
            if (!found) SendText(mySock, "ERROR Game full/missing.");
        }
        // --- 5. CHAT ---
        else if (cmd == "CHAT") {
            string msg;
            getline(ss, msg);
            SendText(mySock, "ECHO: " + msg);
        }
    }

    if (shouldCloseSocket) {
        close(mySock);
    }
    return NULL;
}