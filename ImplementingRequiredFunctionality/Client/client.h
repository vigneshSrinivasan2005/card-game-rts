#ifndef CLIENT_H
#define CLIENT_H

// --- Platform-Specific Setup ---
#ifdef _WIN32
    #define EXPORT_API __declspec(dllexport)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int SocketHandle;
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    // This attribute ensures the function is visible to GameMaker
    #define EXPORT_API __attribute__((visibility("default")))
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h> 
    #include <cstring> 
    #include <sys/select.h> // Required for select() on Mac
    typedef int SocketHandle;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET(s) close(s)
#endif


#include <cstdint> 
#include <vector>

#pragma pack(push, 1)
struct Command {
    uint32_t unit_id;
    uint32_t command_type;
    uint32_t unit_type;
    double   target_x;
    double   target_y;
};
#pragma pack(pop)

extern "C" {
    // 1. CONNECT ONLY
    EXPORT_API double DLLConnect(const char* address, double port_double);
    
    // 2. LOBBY FUNCTIONS
    EXPORT_API double SendLobbyMessage(const char* msg);
    EXPORT_API double ReadLobbyMessage(char* buffer_out, double max_len);
    
    // 3. START GAME
    EXPORT_API double WaitForGameStart();

    // 4. GAME FUNCTIONS
    EXPORT_API void AddLocalCommand(double unit_id, double cmd_type, double tx, double ty);
    EXPORT_API void addPlaceCommand(double unit_type, double tx, double ty);
    EXPORT_API void addEndGameCommand(double winner_id);
    EXPORT_API double SendStep();
    EXPORT_API double hasUnprocessedCommands();
    EXPORT_API double GetNextCommand(const char* buffer_address);
    EXPORT_API void Cleanup();
}

#endif // CLIENT_H