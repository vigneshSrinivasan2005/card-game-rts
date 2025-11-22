#include "client.h"


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
            return false;
        }
    #else
        if (sock == -1) {
            return false;
        }
    #endif

	struct hostent* host = gethostbyname(address);
	if (host == nullptr) {
        CLOSE_SOCKET(sock);
        return false;
    }
	sockaddr_in sendSockAddr;
	memset((char*) &sendSockAddr, 0, sizeof(sendSockAddr));

	sendSockAddr.sin_family = AF_INET;   // using IP
	sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list)); // set the address
	sendSockAddr.sin_port = htons(port);  // set the port

    if (connect(sock, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr)) != 0) {
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
    cmd.target_x = target_x;
    cmd.target_y = target_y;
    gCommandBuffer.push_back(cmd);
    unprocessedCommands.push_back(cmd); //is this smart? //TODO CHECK
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
    uint32_t num_opp_commands = 0;
    ssize_t bytes_received = recv(gSocket, (char*)&num_opp_commands, sizeof(num_opp_commands), 0);
    if (bytes_received <= 0) {
        return false;
    }

    for (uint32_t i = 0; i < num_opp_commands; ++i) {
        Command cmd;
        bytes_received = recv(gSocket, (char*)&cmd, sizeof(Command), 0);
        if (bytes_received <= 0) {
            return false;
        }
        unprocessedCommands.push_back(cmd);
    }
    gCommandBuffer.clear(); // Clear the command buffer after sending
    return true;
}
