#include "fakeClient.h"
#include <vector>
#include <cstring> 
#include <iostream> 

// --- Global State ---
SocketHandle gSocket = -1;
std::vector<Command> gCommandBuffer;      
std::vector<Command> unprocessedCommands; 


// --- Private Helper Function ---
bool RecieveData(char* buffer, int expected_size) {
    if (gSocket == -1) return false;
    
    ssize_t bytes_received = 0;
    while (bytes_received < expected_size) {
        ssize_t result = recv(gSocket, buffer + bytes_received, expected_size - bytes_received, 0);
        if (result <= 0) return false;
        bytes_received += result;
    }
    return true;
}


// --- Exported Functions ---
extern "C" {

    EXPORT_API double DLLConnect(const char* address, double port_double) {
        int port = (int)port_double;
        #ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1.0; 
        #endif
        SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            #ifdef _WIN32
                WSACleanup();
            #endif
            return 0.0;
        }
        struct hostent* host = gethostbyname(address);
        if (host == nullptr) {
            #ifdef _WIN32
                WSACleanup();
            #endif
            CLOSE_SOCKET(sock);
            return 1.0;
        }
        sockaddr_in sendSockAddr;
        memset((char*)&sendSockAddr, 0, sizeof(sendSockAddr));
        sendSockAddr.sin_family = AF_INET;
        memcpy(&sendSockAddr.sin_addr, host->h_addr_list[0], host->h_length);
        sendSockAddr.sin_port = htons(port);
        if (connect(sock, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr)) != 0) {
            #ifdef _WIN32
                WSACleanup();
            #endif
            CLOSE_SOCKET(sock);
            return 2.0; 
        }
        gSocket = sock;
        uint32_t my_player_id = 99; 
        if (!RecieveData((char*)&my_player_id, sizeof(my_player_id))) {
            CLOSE_SOCKET(gSocket);
            gSocket = -1;
            #ifdef _WIN32
                WSACleanup();
            #endif
            return -2.0; 
        }
        return (double)my_player_id;
    }

    EXPORT_API void AddLocalCommand(double unit_id_double, double command_type_double, double target_x, double target_y) {
        Command cmd;
        cmd.unit_id = (uint32_t)unit_id_double;
        cmd.command_type = (uint32_t)command_type_double;
        cmd.unit_type = 0; 
        cmd.target_x = target_x;
        cmd.target_y = target_y;
        gCommandBuffer.push_back(cmd);
    }

    EXPORT_API void addPlaceCommand(double unit_type_double, double target_x, double target_y) {
        Command cmd;
        cmd.unit_id = 0;
        cmd.command_type = 3; 
        cmd.unit_type = (uint32_t)unit_type_double;
        cmd.target_x = target_x;
        cmd.target_y = target_y;
        gCommandBuffer.push_back(cmd);
    }

    // --- NEW: End Game Command ---
    // winner_id: 0.0 or 1.0 depending on who won
    EXPORT_API void addEndGameCommand(double winner_id) {
        Command cmd;
        cmd.unit_id = 0; 
        cmd.command_type = 4; // 4 = END GAME
        cmd.unit_type = 0;
        // We can use target_x to store who won
        cmd.target_x = winner_id; 
        cmd.target_y = 0;
        gCommandBuffer.push_back(cmd);
    }

    EXPORT_API double SendStep() {
        if (gSocket == -1) return 0.0;

        uint32_t command_count = static_cast<uint32_t>(gCommandBuffer.size());
        
        if (send(gSocket, (char*)&command_count, sizeof(command_count), 0) == SOCKET_ERROR) return 0.0;

        if (command_count > 0) {
            int local_data_size = command_count * sizeof(Command);
            if (send(gSocket, (const char*)gCommandBuffer.data(), local_data_size, 0) < 0) return 0.0;
        }

        uint32_t num_acked_commands = 0;
        if (!RecieveData((char*)&num_acked_commands, sizeof(num_acked_commands))) return 0.0;

        unprocessedCommands.clear();
        if (num_acked_commands > 0) {
            int acked_data_size = num_acked_commands * sizeof(Command);
            unprocessedCommands.resize(num_acked_commands);
            
            if (!RecieveData((char*)unprocessedCommands.data(), acked_data_size)) {
                unprocessedCommands.clear(); 
                return 0.0;
            }
        }

        gCommandBuffer.clear();
        return 1.0; 
    }

    EXPORT_API double hasUnprocessedCommands() {
        return !unprocessedCommands.empty() ? 1.0 : 0.0;
    }

    EXPORT_API double GetNextCommand(const char* buffer_address) {
        if (unprocessedCommands.empty()) return 0.0; 
        
        Command cmd = unprocessedCommands.front();
        unprocessedCommands.erase(unprocessedCommands.begin()); 

        uint8_t* p_buffer = (uint8_t*)buffer_address;
        memcpy(p_buffer, &cmd, sizeof(Command));
        
        return 1.0; 
    }

    EXPORT_API void Cleanup() {
        if (gSocket != -1) {
            CLOSE_SOCKET(gSocket);
            gSocket = -1;
        }
        #ifdef _WIN32
            WSACleanup();
        #endif
    }
}