#include "server.h"

// POSIX Socket Headers (Linux/macOS)
#include <cstring>   // For memset
#include <unistd.h>  // For close()
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <signal.h>  // For signal handling
#include <pthread.h> // NEW: POSIX Threads library
#include <cstdlib>   // For exit()
#include <iostream>
#include <atomic>

using namespace std;

// Multithreaded Match arguments
struct MatchArgs {
    int client1_sock;
    int client2_sock;
};

atomic<uint32_t> g_NextUnitID (1000); 
int g_server_sock = -1; // Globalize the listening socket for cleanup

bool SendData(int sock, const char* buffer, int size) {
    int total_sent = 0;
    while (total_sent < size) {
        int result = send(sock, buffer + total_sent, size - total_sent, 0);
        if (result <= 0) {
            return false;
        }
        total_sent += result;
    }
    return true;
}

bool RecvData(int sock, char* buffer, int expected_size) {
    int bytes_received = 0;
    while(bytes_received < expected_size) {
        int result = recv(sock, buffer + bytes_received, expected_size - bytes_received, 0);
        if (result <= 0) { 
            return false;
        }
        bytes_received += result;
    }
    return true;
}


// multithreaded method to handle a match between two players
void* HandleMatch(void* args) {
    MatchArgs* match_args = static_cast<MatchArgs*>(args);
    int client1_sock = match_args->client1_sock;
    int client2_sock = match_args->client2_sock;
    delete match_args; 

    int clientSockets[MAX_PLAYERS] = {client1_sock, client2_sock};
    int current_tick = 0;
    const uint32_t COMMAND_TYPE_PLACE = 3;

    cout << "[MATCH " << client1_sock << " vs " << client2_sock << "] Match started." << endl;

    while (true) {
        current_tick++;
        cout << "\n[MATCH " << client1_sock << "] === TICK " << current_tick << " START === " << endl;
        
        // 1. RECEIVE PHASE (Blocks until both players submit input)
        vector<Command> requests[MAX_PLAYERS]; 
        bool match_error = false;

        for (int i = 0; i < MAX_PLAYERS; ++i) { // i = Player Index (0 or 1)
            uint32_t count = 0;
            cout << "[MATCH " << client1_sock << "] Waiting for input from Player " << i + 1 << "..." << endl;
            if (!RecvData(clientSockets[i], (char*)&count, sizeof(count))) {
                cerr << "[MATCH " << client1_sock << "] Player " << i + 1 << " disconnected. Ending match." << endl;
                match_error = true;
                break; 
            }

            cout << "[MATCH " << client1_sock << "] Player " << i + 1 << " sent " << count << " commands." << endl;

            int data_size = count * sizeof(Command);
            requests[i].resize(count);
            if (!RecvData(clientSockets[i], (char*)requests[i].data(), data_size)) {
                cerr << "[MATCH " << client1_sock << "] Player " << i + 1 << " data error. Ending match." << endl;
                match_error = true;
                break; 
            }
        }
        
        if (match_error) break;

        // 2. PROCESSING AND ID ASSIGNMENT PHASE (NO VALIDATION)
        cout << "[MATCH " << client1_sock << "] Processing commands..." << endl;
        vector<Command> finalized_commands;
        
        for (int i = 0; i < MAX_PLAYERS; ++i) { // i = Sender's Index (0 or 1)
            for (Command& cmd : requests[i]) {
                
                if (cmd.command_type == COMMAND_TYPE_PLACE) {
                    // --- PLACE: Assign global ID (No ownership check needed) ---
                    // Assign the next available global ID thread-safely
                    cmd.unit_id = g_NextUnitID.fetch_add(1); 
                    cout << "[MATCH " << client1_sock << "]  > Player " << i + 1 << " PLACE command. New Unit ID: " << cmd.unit_id << endl;
                } else {
                    cout << "[MATCH " << client1_sock << "]  > Player " << i + 1 << " command (Type: " << cmd.command_type << ", Unit: " << cmd.unit_id << ")" << endl;
                }

                // Since there is no validation, all received commands are broadcasted.
                finalized_commands.push_back(cmd);
            }
        }
        
        // 3. BROADCAST PHASE
        
        uint32_t total_count = finalized_commands.size();
        cout << "[MATCH " << client1_sock << "] Broadcasting " << total_count << " finalized commands to both players." << endl;
        int total_data_size = total_count * sizeof(Command);

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (!SendData(clientSockets[i], (const char*)&total_count, sizeof(total_count)) ||
                !SendData(clientSockets[i], (const char*)finalized_commands.data(), total_data_size)) {
                cerr << "[MATCH " << client1_sock << "] Error broadcasting to Player " << i + 1 << endl;
                match_error = true;
                break;
            }
        }

        if (match_error) break;
        cout << "[MATCH " << client1_sock << "] === TICK " << current_tick << " END === " << endl;
    }

    // Match thread cleanup
    cout << "[MATCH " << client1_sock << " vs " << client2_sock << "] Match ended. Closing sockets." << endl;
    close(client1_sock);
    close(client2_sock);
    return NULL; 
}



bool ListenForPlayers(int port) {
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_sock < 0) {
        cerr << "Error: Failed to create server socket." << endl;
        return false;
    }

    // ... (Socket setup remains the same) ...
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    int opt = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(g_server_sock, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error: Failed to bind socket to port " << port << endl;
        close(g_server_sock);
        return false;
    }

    if (listen(g_server_sock, 100) < 0) { 
        cerr << "Error: Failed to listen on socket." << endl;
        close(g_server_sock);
        return false;
    }

    cout << "Server successfully initialized and is listening on port " << port << endl;

    while (true) {
        int client1_sock = -1;
        int client2_sock = -1;
        pthread_t match_thread_id; // pthread declaration

        cout << "Waiting for Player 1..." << endl;
        client1_sock = accept(g_server_sock, NULL, NULL);
        if (client1_sock < 0) { cerr << "Error accepting Player 1 connection." << endl; continue; }
        cout << "Player 1 connected (Socket ID: " << client1_sock << "). Waiting for Player 2..." << endl;

        client2_sock = accept(g_server_sock, NULL, NULL);
        if (client2_sock < 0) { 
            cerr << "Error accepting Player 2 connection. Closing P1 socket." << endl;
            close(client1_sock);
            continue; 
        }
        cout << "Player 2 connected (Socket ID: " << client2_sock << ")." << endl;

        // NEW: 1. Allocate arguments on the heap
        MatchArgs* args = new MatchArgs{client1_sock, client2_sock};
        
        // NEW: 2. Launch the match logic using pthread_create
        if (pthread_create(&match_thread_id, NULL, HandleMatch, args) != 0) {
            cerr << "Error creating thread for new match." << endl;
            delete args; // Clean up arguments
            close(client1_sock);
            close(client2_sock);
            continue;
        }

        pthread_detach(match_thread_id); //allow to run independently

        cout << "--- New Match Launched ---" << endl;
    }

    return true; 
}


void cleanup_and_exit(int sig) {
    cerr << "\nServer received signal " << sig << ". Shutting down..." << endl;
    if (g_server_sock != -1) {
        close(g_server_sock);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    
    signal(SIGINT, cleanup_and_exit);
    
    ListenForPlayers(SERVER_PORT);
    
    return 0;
}