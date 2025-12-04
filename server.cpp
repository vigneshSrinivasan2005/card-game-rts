#include "server.h"

// POSIX Socket Headers (Linux/macOS)
#include <cstring>   
#include <unistd.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <signal.h>  
#include <pthread.h> 
#include <cstdlib>   
#include <iostream>
#include <atomic>

using namespace std;

struct MatchArgs {
    int client1_sock;
    int client2_sock;
};

atomic<uint32_t> g_NextUnitID (1000); 
int g_server_sock = -1; 

bool SendData(int sock, const char* buffer, int size) {
    int total_sent = 0;
    while (total_sent < size) {
        int result = send(sock, buffer + total_sent, size - total_sent, 0);
        if (result <= 0) return false;
        total_sent += result;
    }
    return true;
}

bool RecvData(int sock, char* buffer, int expected_size) {
    int bytes_received = 0;
    while(bytes_received < expected_size) {
        int result = recv(sock, buffer + bytes_received, expected_size - bytes_received, 0);
        if (result <= 0) return false;
        bytes_received += result;
    }
    return true;
}

void* HandleMatch(void* args) {
    MatchArgs* match_args = static_cast<MatchArgs*>(args);
    int client1_sock = match_args->client1_sock;
    int client2_sock = match_args->client2_sock;
    delete match_args; 

    int clientSockets[MAX_PLAYERS] = {client1_sock, client2_sock};
    int current_tick = 0;
    
    // --- COMMAND CONSTANTS ---
    const uint32_t COMMAND_TYPE_PLACE = 3;
    const uint32_t COMMAND_TYPE_END_GAME = 4; // NEW: Defined ID for game over

    cout << "[MATCH " << client1_sock << " vs " << client2_sock << "] Match started." << endl;

    bool match_running = true;

    while (match_running) {
        current_tick++;
        vector<Command> requests[MAX_PLAYERS]; 
        bool match_error = false;

        // 1. Receive commands from both players
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            uint32_t count = 0;
            if (!RecvData(clientSockets[i], (char*)&count, sizeof(count))) {
                cerr << "[MATCH] Player " << i + 1 << " disconnected." << endl;
                match_error = true;
                break; 
            }
            int data_size = count * sizeof(Command);
            requests[i].resize(count);
            if (!RecvData(clientSockets[i], (char*)requests[i].data(), data_size)) {
                cerr << "[MATCH] Player " << i + 1 << " data error." << endl;
                match_error = true;
                break; 
            }
        }
        
        if (match_error) break;

        // 2. Process commands
        vector<Command> finalized_commands;
        bool game_over_signal_received = false;

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            for (Command& cmd : requests[i]) {
                if (cmd.command_type == COMMAND_TYPE_PLACE) {
                    cmd.unit_id = g_NextUnitID++; 
                }
                // NEW: Detect End Game
                if (cmd.command_type == COMMAND_TYPE_END_GAME) {
                    game_over_signal_received = true;
                    cout << "[MATCH] End Game signal received from Player " << i+1 << endl;
                }
                finalized_commands.push_back(cmd);
            }
        }
        
        // 3. Broadcast commands
        uint32_t total_count = finalized_commands.size();
        int total_data_size = total_count * sizeof(Command);

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (!SendData(clientSockets[i], (const char*)&total_count, sizeof(total_count)) ||
                !SendData(clientSockets[i], (const char*)finalized_commands.data(), total_data_size)) {
                match_error = true;
                break;
            }
        }

        if (match_error) break;

        // NEW: Graceful Exit
        // We break AFTER broadcasting so clients receive the "Game Over" command
        if (game_over_signal_received) {
            cout << "[MATCH] Ending match gracefully." << endl;
            match_running = false; 
        }
    }

    close(client1_sock);
    close(client2_sock);
    return NULL;
}

// ... (Rest of ListenForPlayers and main remains the same as your original file) ...
// Copy the ListenForPlayers, cleanup_and_exit, and main from your original upload here.
// I have omitted them for brevity, but they do not need changes.

bool ListenForPlayers(int port) {
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_sock < 0) return false;

    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    int opt = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(g_server_sock, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) return false;
    if (listen(g_server_sock, 100) < 0) return false;

    cout << "Server listening on port " << port << endl;

    while (true) {
        int c1 = accept(g_server_sock, NULL, NULL);
        if (c1 < 0) continue;
        cout << "Player 1 connected." << endl;
        
        int c2 = accept(g_server_sock, NULL, NULL);
        if (c2 < 0) { close(c1); continue; }
        cout << "Player 2 connected." << endl;

        MatchArgs* args = new MatchArgs{c1, c2};
        pthread_t t;
        if (pthread_create(&t, NULL, HandleMatch, args) != 0) {
            delete args; close(c1); close(c2);
        } else {
            pthread_detach(t);
        }
    }
    return true; 
}

void cleanup_and_exit(int sig) {
    if (g_server_sock != -1) close(g_server_sock);
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, cleanup_and_exit);
    ListenForPlayers(SERVER_PORT);
    return 0;
}