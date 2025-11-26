#include "client.h"

SocketHandle gSocket = -1;


bool Connect(const char* address, int port){
    #ifdef _WIN32  // windows specific initialization
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed." << endl;
            return false;
        }
    #endif
    SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
    //in windows SOCKET returns INVALID_SOCKET on error, in Mac/Linux it returns -1
    #ifdef _WIN32
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }
    #else
        if (sock == -1) {
            return false;
        }
    #endif

	struct hostent* host = gethostbyname(address);
	if (host == nullptr) {
        #ifdef _WIN32
            WSACleanup();
        #endif
        CLOSE_SOCKET(sock);
        return false;
    }
	sockaddr_in sendSockAddr;
	memset((char*) &sendSockAddr, 0, sizeof(sendSockAddr));

	sendSockAddr.sin_family = AF_INET;   // using IP
	sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list)); // set the address
	sendSockAddr.sin_port = htons(port);  // set the port

    if (connect(sock, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr)) != 0) {
        #ifdef _WIN32
            WSACleanup();
        #endif
        CLOSE_SOCKET(sock);
        return false;
    }
    gSocket = sock;
    return true;
}

void AddLocalCommand(int unit_id, int command_type, double target_x, double target_y){
    Command cmd;
    cmd.unit_id = unit_id;
    cmd.command_type = command_type;
    cmd.unit_type = 0;//unused here
    cmd.target_x = target_x;
    cmd.target_y = target_y;
    gCommandBuffer.push_back(cmd);
}

void addPlaceCommand(int unit_type, double target_x, double target_y){
    Command cmd;
    cmd.unit_id = 0; //reusing unit_id to store unit_type for place command
    cmd.command_type = 3; //3 means place
    cmd.unit_type = unit_type;
    cmd.target_x = target_x;
    cmd.target_y = target_y;
    gCommandBuffer.push_back(cmd);
}

bool SendStep(){
    if (gSocket == -1) {
        return false; // Not connected
    }
    uint32_t command_count = static_cast<uint32_t>(gCommandBuffer.size()); // Number of commands to send
    // Send the number of commands first
    if (send(gSocket, (char*)&command_count, sizeof(command_count), 0) == -1) {
        return false;
    }
    int local_data_size = command_count * sizeof(Command);
    // Send each command
    if (send(gSocket, (const char*)gCommandBuffer.data(), local_data_size, 0) < 0) {
        return false;
    }
    //wait for response from the server with the oppoenent's commands
    uint32_t num_acked_commands = 0;
    bool success = RecieveData((char*)&num_acked_commands,sizeof(num_acked_commands));
    if (!success) {
        return false;
    }

    vector<char> ackedCommands(num_acked_commands * sizeof(Command));
    if(!RecieveData(ackedCommands.data(), ackedCommands.size()))
        return false;

    unprocessedCommands.clear();  
    if (num_acked_commands > 0) {
        unprocessedCommands.resize(num_acked_commands);
        // Copy the entire memory block into the vector of Command structs
        std::memcpy(unprocessedCommands.data(), ackedCommands.data(), ackedCommands.size());
    }

    gCommandBuffer.clear(); // Clear the command buffer after sending
    
    return true;
}

bool RecieveData(char* buffer , int expected_size){ 
    if (gSocket == -1) {
        return false; // Not connected
    }
    ssize_t bytes_received = 0;
    while(bytes_received < expected_size){
        bytes_received += recv(gSocket, buffer + bytes_received, expected_size - bytes_received, 0);
        if (bytes_received <= 0) {
            return false;
        }
    }
    return bytes_received > 0;
}
bool hasUnprocessedCommands(){
    return !unprocessedCommands.empty();
}

bool GetNextCommand(char* buffer){ // Assume buffer is large enough to hold a Command
    if (unprocessedCommands.empty()) {
        return false; // No commands available
    }
    Command cmd = unprocessedCommands.front();
    unprocessedCommands.erase(unprocessedCommands.begin());
    memcpy(buffer, &cmd, sizeof(Command));
    return true;
}
void Cleanup(){
    if (gSocket != -1) {
        CLOSE_SOCKET(gSocket);
        gSocket = -1;
    }
    #ifdef _WIN32
        WSACleanup();
    #endif
}