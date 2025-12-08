#include "shared.h"
vector<GameRoom> g_Games;
unordered_map<string, User> g_AllUsers; //maps username to user
unordered_map<int, User> connected_Users; // maps socket to users
pthread_mutex_t g_LobbyMutex = PTHREAD_MUTEX_INITIALIZER;

bool SendText(int sock, string msg){
    msg += "\n";
    return send(sock, msg.c_str(), msg.length(), 0) > 0;
}

static bool readString(FILE* fd, std::string& str) {
    uint32_t len;
    if (fread(&len, sizeof(len), 1, fd) != 1) return false;
    
    // Resize the string to hold the incoming data
    str.resize(len);
    
    if (fread(&str[0], 1, len, fd) != len) return false;
    return true;
}

static bool writeString(FILE* fd, const std::string& str) {
    uint32_t len = str.length();
    if (fwrite(&len, sizeof(len), 1, fd) != 1) return false;
    if (fwrite(str.c_str(), 1, len, fd) != len) return false;
    return true;
}

void getAllUsers(){
    //if file exists, load from file
    //else return empty map
    string fname = "users.bin";
    FILE* fd = fopen(fname.c_str(),"rb");
    if(!fd){
        return;
    }
        uint32_t mapSize;
    // Read the number of users
    if (fread(&mapSize, sizeof(mapSize), 1, fd) != 1) {
        fclose(fd);
        return; 
    }
    
    for (uint32_t i = 0; i < mapSize; ++i) {
        User u;
        
        // 1. Read the username (variable length)
        if (!readString(fd, u.username)) {
            fclose(fd);
            return;
        }
        
        // 2. Read numWins (fixed length integer)
        if (fread(&u.numWins, sizeof(u.numWins), 1, fd) != 1) {
            fclose(fd);
            return;
        }
        
        // Insert into map using username as the key
        g_AllUsers[u.username] = u;
    }

    fclose(fd);
    return;
}

void saveAllUsers() {
    string fname = "users.bin";
    
    // Use "wb" (write binary)
    FILE* fd = fopen(fname.c_str(), "wb");
    if (!fd) {
        cout << "Error: Could not open file " << fname << " for writing." << endl;
        return;
    }

    // 1. Write the number of elements in the map
    uint32_t mapSize = g_AllUsers.size();
    if (fwrite(&mapSize, sizeof(mapSize), 1, fd) != 1) {
        fclose(fd);
        return;
    }
    
    // 2. Write each user sequentially
    for (const auto& pair : g_AllUsers) {
        const User& u = pair.second;
        
        // A. Write the username (string helper handles length + data)
        if (!writeString(fd, u.username)) {
            fclose(fd);
            return;
        }
        
        // B. Write numWins (fixed length integer)
        if (fwrite(&u.numWins, sizeof(u.numWins), 1, fd) != 1) {
            fclose(fd);
            return;
        }
    }
    
    // Clean up
    fclose(fd);
}
bool isInGame(int sock){ // checks if the socket is in an active game
    for(const auto& game : g_Games){
        if(game.isActive){
            if(game.hostSocket == sock || game.joinerSocket == sock){
                return true;
            }
        }
    }
    return false;
}
void sendToAllInLobby(const string& message){ //Assumes that I have a mutex
    cout << "[LOBBY] Broadcasting to all in lobby: " << message << endl;
    for(const auto& pair : connected_Users){
        cout << "[LOBBY] Sending to " << pair.second.username << ": " << message << endl;
        int sock = pair.first;
        if(sock && !isInGame(sock)){
            SendText(sock, message);
        }
    }
}
