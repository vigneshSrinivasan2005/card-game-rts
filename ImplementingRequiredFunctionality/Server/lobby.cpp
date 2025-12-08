#include "lobby.h"
#include "game_instance.h"
#include "shared.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <algorithm>

using namespace std;

// Global ID counter for rooms
static int g_GameIDCounter = 1;

string generateLeaderboard() {
    // 1. Copy users and sort (Requires the lock)
    pthread_mutex_lock(&g_LobbyMutex);
    vector<User> topUsers;
    for (const auto& pair : g_AllUsers) {
        topUsers.push_back(pair.second);
    }
    pthread_mutex_unlock(&g_LobbyMutex);

    // Sort by numWins descending
    sort(topUsers.begin(), topUsers.end(), [](const User& a, const User& b) {
        return a.numWins > b.numWins;
    });

    // 2. Build the output string
    string leaderboard = "LEADERBOARD:";
    int count = 0;
    for (const auto& u : topUsers) {
        if (count >= 3) break; // Take only top 3
        leaderboard += u.username + " - Wins: " + to_string(u.numWins) + "|";
        count++;
    }
    return leaderboard;
}

void* HandleClientLobby(void* arg) {
    int mySock = *(int*)arg;
    delete (int*)arg;
    
    char buffer[1024];
    bool inLobby = true;
    bool shouldCloseSocket = true; // Default to true, set to false if we pass socket to Game

    SendText(mySock, "WELCOME. Commands: REGISTER <user>, LIST, CREATE, JOIN <id>");
    
    //send Leaderboard // Probably should wait until they ack? //TODO SEEMS RISKY

    //string leaderboard = generateLeaderboard();
    //SendText(mySock, leaderboard);

    while (inLobby) {
        usleep(10000); // 10 milliseconds sleep 
        memset(buffer, 0, 1024);

        // I want this call to be nonblocking so that even if no messages are recieve, we can send chat updates and leaderboard updates.
        //So we will use select to check for data before calling recv
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(mySock, &readfds);
        timeval timeout = {0, 0}; // Non-blocking
        int ready = select(mySock + 1, &readfds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(mySock, &readfds)) {
            cout << "[LOBBY] Prrocessed from " << mySock << ": " << buffer << endl;
            int bytes = recv(mySock, buffer, 1024, 0);
            if (bytes <= 0) break; 

            cout << "[LOBBY] Received from " << mySock << ": " << buffer << endl;

            string input(buffer);
            stringstream ss(input);
            string cmd;
            ss >> cmd;

            // --- 1. REGISTER ---
            if (cmd == "REGISTER") {
                string user;
                ss >> user;   
                pthread_mutex_lock(&g_LobbyMutex);
                string sendMsg;
                //add to g_AllUsers and add to connected_Users
                if (g_AllUsers.count(user)) {
                    // User exists, just assign session data
                    connected_Users[mySock] = g_AllUsers[user];
                    sendMsg = "OK LOGGED_IN " + user + ". Wins: " + to_string(g_AllUsers[user].numWins);
                    
                } else {
                    g_AllUsers[user] = User{user, 0};
                    connected_Users[mySock] = g_AllUsers[user];
                    sendMsg = "OK Registered " + user + ". Wins: 0";
                }
                pthread_mutex_unlock(&g_LobbyMutex);
                SendText(mySock, sendMsg);
                continue;
            }

            //check if registered
            pthread_mutex_lock(&g_LobbyMutex);
            bool isRegistered = connected_Users.find(mySock) == connected_Users.end();
            pthread_mutex_unlock(&g_LobbyMutex);

            if(isRegistered){
                SendText(mySock, "ERROR Please REGISTER first.");
                continue;
            }
            
            // --- 2. LIST ---
            if (cmd == "LIST") {
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
                int newID = g_GameIDCounter++;
                GameRoom room = { newID, mySock, -1, false, true };
                pthread_mutex_lock(&g_LobbyMutex);
                g_Games.push_back(room);
                pthread_mutex_unlock(&g_LobbyMutex);

                SendText(mySock, "CREATED " + to_string(newID) + " WAIT...");

                // HOST WAITING LOOP
                while (true) {
                    usleep(100000); // 100ms polling
                    pthread_mutex_lock(&g_LobbyMutex);// VERY LONG LOCK DANGER ZONE
                    GameRoom myRoom;
                    bool haveRoom = false;
                    for (auto& g : g_Games) {
                        if (g.id == newID) { myRoom = g; haveRoom = true; break; }
                    }
                    pthread_mutex_unlock(&g_LobbyMutex);
                    if (!haveRoom) {
                        // Room was deleted, probably due to disconnection
                        SendText(mySock, "ERROR Room closed.");
                        break;
                    }
                    if (myRoom.isFull) {
                        // Match found!
                        SendText(mySock, "MATCH_START");
                        
                        char readyBuffer[1024];

                        // 1. Wait for Host's ACK
                        memset(readyBuffer, 0, 1024);
                        if (recv(myRoom.hostSocket, readyBuffer, 1024, 0) <= 0) {
                            cerr << "[LOBBY] Host " << myRoom.hostSocket << " disconnected during ACK handshake." << endl;
                            close(myRoom.hostSocket); close(myRoom.joinerSocket); // Close both on failure
                            return NULL; 
                        }

                        // 2. Wait for Joiner's ACK
                        memset(readyBuffer, 0, 1024);
                        if (recv(myRoom.joinerSocket, readyBuffer, 1024, 0) <= 0) {
                            cerr << "[LOBBY] Joiner " << myRoom.joinerSocket << " disconnected during ACK handshake." << endl;
                            close(myRoom.hostSocket); close(myRoom.joinerSocket);
                            return NULL;
                        }

                        // Host thread takes over as the Game Server thread
                        MatchArgs* args = new MatchArgs{ myRoom.hostSocket, myRoom.joinerSocket };
                        
                        
                        // TRANSITION TO GAME
                        HandleMatch(args); 
                        
                        inLobby = false;
                        shouldCloseSocket = false; // HandleMatch closes them
                        break;
                    }
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
                        break;
                    }
                }
                pthread_mutex_unlock(&g_LobbyMutex);
                
                if (!found) {SendText(mySock, "ERROR Game full/missing.");
                }else{
                    SendText(mySock, "MATCH_START");
                    inLobby = false;
                    shouldCloseSocket = false; // Do not close, Host thread owns it now
                }
            }
            // --- 5. CHAT ---
            else if (cmd == "CHAT") {
                string msg;
                getline(ss, msg);
                SendText(mySock, "ECHO: " + msg);
                //send to all connected users 
                pthread_mutex_lock(&g_LobbyMutex);
                sendToAllInLobby("CHAT " + connected_Users[mySock].username + ": " + msg);
                pthread_mutex_unlock(&g_LobbyMutex);
            }else if(cmd == "LEADERBOARD"){
                string leaderboard = generateLeaderboard();
                SendText(mySock, leaderboard);
            }else if(cmd == "EXIT"){
                SendText(mySock, "GOODBYE");
                pthread_mutex_lock(&g_LobbyMutex);
                connected_Users.erase(mySock);
                pthread_mutex_unlock(&g_LobbyMutex);
                break;
            }else if(cmd == "UNREGISTER"){
                SendText(mySock, "UNREGISTERED");
                pthread_mutex_lock(&g_LobbyMutex);
                g_AllUsers.erase(connected_Users[mySock].username);
                connected_Users.erase(mySock);
                pthread_mutex_unlock(&g_LobbyMutex);
                break;
            }
            else {
                SendText(mySock, "ERROR Unknown command.");
            }
            
        }
    }

    if (shouldCloseSocket) {
        close(mySock);
    }
    return NULL;
}