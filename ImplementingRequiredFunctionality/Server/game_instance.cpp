#include "game_instance.h"
#include "shared.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <atomic>

using namespace std;

// Atomic counter for Unit IDs (Started at 1000)
static atomic<uint32_t> g_NextUnitID(1000);

// --- Helpers ---
static bool SendData(int sock, const char* buffer, int size) {
    int total_sent = 0;
    while (total_sent < size) {
        int result = send(sock, buffer + total_sent, size - total_sent, 0);
        if (result <= 0) return false;
        total_sent += result;
    }
    return true;
}

static bool RecvData(int sock, char* buffer, int expected_size) {
    int bytes_received = 0;
    while(bytes_received < expected_size) {
        int result = recv(sock, buffer + bytes_received, expected_size - bytes_received, 0);
        if (result <= 0) return false;
        bytes_received += result;
    }
    return true;
}

// --- Main Game Loop ---
void* HandleMatch(void* args) {
    MatchArgs* match_args = static_cast<MatchArgs*>(args);
    int client1_sock = match_args->client1_sock;
    int client2_sock = match_args->client2_sock;
    delete match_args; 

    int clientSockets[2] = {client1_sock, client2_sock};
    
    const uint32_t COMMAND_TYPE_PLACE = 3;
    const uint32_t COMMAND_TYPE_END_GAME = 4;

    cout << "[GAME_INSTANCE] Match Started: " << client1_sock << " vs " << client2_sock << endl;

    // --- 1. HANDSHAKE (Send Player IDs) ---
    uint32_t p1_id = 0;
    uint32_t p2_id = 1;
    usleep(100000);
    // We send these strictly as binary data
    if (!SendData(client1_sock, (const char*)&p1_id, sizeof(p1_id))) {
        cerr << "[GAME_INSTANCE] Error sending Handshake to P1" << endl;
        close(client1_sock); close(client2_sock); return NULL;
    }
    if (!SendData(client2_sock, (const char*)&p2_id, sizeof(p2_id))) {
        cerr << "[GAME_INSTANCE] Error sending Handshake to P2" << endl;
        close(client1_sock); close(client2_sock); return NULL;
    }

    // --- 2. LOCKSTEP LOOP ---
    bool match_running = true;
    while (match_running) {
        vector<Command> requests[2]; 
        bool match_error = false;

        // A. Receive commands from both players
        for (int i = 0; i < 2; ++i) {
            uint32_t count = 0;
            if (!RecvData(clientSockets[i], (char*)&count, sizeof(count))) {
                match_error = true; break; 
            }
            int data_size = count * sizeof(Command);
            requests[i].resize(count);
            if (!RecvData(clientSockets[i], (char*)requests[i].data(), data_size)) {
                match_error = true; break; 
            }
        }
        
        if (match_error) break;

        vector<Command> finalized_commands;
        bool game_over_signal = false;

        // B. Process commands & Assign IDs
        for (int i = 0; i < 2; ++i) {
            for (Command& cmd : requests[i]) {
                
                // ID Logic: P1 uses 1000s, P2 uses 2000s
                if (cmd.command_type == COMMAND_TYPE_PLACE) {
                    uint32_t base_id = g_NextUnitID++;
                    cmd.unit_id = base_id + (i * 1000);
                }
                
                if (cmd.command_type == COMMAND_TYPE_END_GAME) {
                    game_over_signal = true;
                }

                finalized_commands.push_back(cmd);
            }
        }
        
        // C. Broadcast
        uint32_t total_count = finalized_commands.size();
        int total_data_size = total_count * sizeof(Command);

        for (int i = 0; i < 2; ++i) {
            if (!SendData(clientSockets[i], (const char*)&total_count, sizeof(total_count)) ||
                !SendData(clientSockets[i], (const char*)finalized_commands.data(), total_data_size)) {
                match_error = true; break;
            }
        }

        if (match_error) break;

        // D. Check Game Over
        if (game_over_signal) {
            cout << "[GAME_INSTANCE] End Game signal received. Closing match." << endl;
            match_running = false;
        }
    }

    cout << "[GAME_INSTANCE] Match ended. Closing sockets." << endl;
    close(client1_sock);
    close(client2_sock);
    return NULL;
}