#include "client.h"
#include <vector>
#include <cstring> 
#include <iostream> 
#include <string>


// --- Global State ---
SocketHandle gSocket = -1;
std::vector<Command> gCommandBuffer;      
std::vector<Command> unprocessedCommands; 
static std::string g_ClientBuffer; // Persistent buffer to hold partial messages

// --- Helper ---
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

extern "C" {

    // --- 1. CONNECT ---
    EXPORT_API double DLLConnect(const char* address, double port_double) {
        int port = (int)port_double;
        
        // Cleanup previous connection if necessary
        if (gSocket != -1) { CLOSE_SOCKET(gSocket); gSocket = -1; }
        
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
            CLOSE_SOCKET(sock);
            #ifdef _WIN32
                WSACleanup();
            #endif
            return 1.0; // Host not found error
        }

        sockaddr_in sendSockAddr;
        memset((char*)&sendSockAddr, 0, sizeof(sendSockAddr));
        sendSockAddr.sin_family = AF_INET;
        memcpy(&sendSockAddr.sin_addr, host->h_addr_list[0], host->h_length);
        sendSockAddr.sin_port = htons(port);

        if (connect(sock, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr)) != 0) {
            CLOSE_SOCKET(sock);
            #ifdef _WIN32
                WSACleanup();
            #endif
            return 2.0; // Connection failed
        }

        gSocket = sock;
        return 1.0; // Success! Connection open.
    }

    // --- 2. LOBBY TEXT FUNCTIONS ---
    
    EXPORT_API double SendLobbyMessage(const char* msg) {
        if (gSocket == -1) return 5.0;
        std::string message(msg);
        
        // Send the message, assuming the GML adds the necessary "\n"
        if (send(gSocket, message.c_str(), message.length(), 0) < 0) return 4.0;
        return 1.0;
    }

    
    EXPORT_API double ReadLobbyMessage(char* buffer_out, double max_len) {
        if (gSocket == -1) return 0.0;
        // --- PHASE 1: Try to read new data into the persistent buffer ---
        
        // 1. NON-BLOCKING CHECK for new data
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(gSocket, &readfds);
        struct timeval timeout = {0, 0}; 

        if (select(gSocket + 1, &readfds, NULL, NULL, &timeout) > 0) {
            // Data is ready, call recv (which should not block now)
            char temp_buffer[1024];
            memset(temp_buffer, 0, 1024);
            
            // Read data without waiting.
            int bytes = recv(gSocket, temp_buffer, 1024, 0);

            if (bytes > 0) {
                // Append all new data to the persistent buffer
                g_ClientBuffer.append(temp_buffer, bytes);
            } else if (bytes == 0) {
                // Socket disconnected
                gSocket = -1;
                return -1.0; // Indicate disconnect
            }
        }
        
        // --- PHASE 2: Scan the persistent buffer for a complete message ---
        
        // Look for the newline delimiter
        size_t pos = g_ClientBuffer.find('\n');

        if (pos != std::string::npos) {
            // We found a complete message delimited by '\n'

            // 1. Extract the message (up to and including the newline)
            std::string message = g_ClientBuffer.substr(0, pos + 1);
            
            // 2. Remove the message from the start of the buffer
            g_ClientBuffer.erase(0, pos + 1);

            // 3. Copy to GameMaker buffer (ensure C-string termination)
            int copy_len = message.length();
            if (copy_len > (int)max_len) {
                // Truncate if message exceeds GML buffer size
                copy_len = (int)max_len;
            }

            // Copy the message (including the newline)
            memcpy(buffer_out, message.c_str(), copy_len);
            
            // Ensure C-string termination at the end of the copied data
            buffer_out[copy_len] = '\0';
            
            return (double)copy_len; // Success: Return number of bytes read
        }
        
        return 0.0; // No complete message found yet (waiting for more data)
    }
    
    // --- 3. TRANSITION TO GAME (ACK Protocol) ---
    EXPORT_API double WaitForGameStart() {
        if (gSocket == -1) return -1.0;

        uint32_t my_player_id = 99; // Placeholder initialization
        
        if (!RecieveData((char*)&my_player_id, sizeof(my_player_id))) {
            return -2.0; // Error or disconnection during handshake
        }
        return (double)my_player_id;
    }


    EXPORT_API void AddLocalCommand(double unit_id, double cmd_type, double tx, double ty) {
        Command cmd;
        cmd.unit_id = (uint32_t)unit_id;
        cmd.command_type = (uint32_t)cmd_type;
        cmd.unit_type = 0; 
        cmd.target_x = tx;
        cmd.target_y = ty;
        gCommandBuffer.push_back(cmd);
    }

    EXPORT_API void addPlaceCommand(double unit_type, double tx, double ty) {
        Command cmd;
        cmd.unit_id = 0;
        cmd.command_type = 3; 
        cmd.unit_type = (uint32_t)unit_type;
        cmd.target_x = tx;
        cmd.target_y = ty;
        gCommandBuffer.push_back(cmd);
    }

    EXPORT_API void addEndGameCommand(double winner_id) {
        Command cmd;
        cmd.unit_id = 0; 
        cmd.command_type = 4; 
        cmd.unit_type = 0;
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
            if (!RecieveData((char*)unprocessedCommands.data(), acked_data_size)) return 0.0;
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