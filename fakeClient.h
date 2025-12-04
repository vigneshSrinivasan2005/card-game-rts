#ifndef CLIENT_H
#define CLIENT_H

// --- Platform-Specific Setup ---
#ifdef _WIN32
    #define EXPORT_API __declspec(dllexport)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
    typedef SOCKET SocketHandle;
    #define CLOSE_SOCKET(s) closesocket(s)
    #pragma comment(lib, "ws2_32.lib")
#else
    #define EXPORT_API
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <netdb.h> 
    #include <cstring> 
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
    EXPORT_API double DLLConnect(const char* address, double port_double);
    EXPORT_API void AddLocalCommand(double unit_id_double, double command_type_double, double target_x, double target_y);
    EXPORT_API void addPlaceCommand(double unit_type_double, double target_x, double target_y);
    
    // NEW
    EXPORT_API void addEndGameCommand(double winner_id);

    EXPORT_API double SendStep();
    EXPORT_API double hasUnprocessedCommands();
    EXPORT_API double GetNextCommand(const char* buffer_address);
    EXPORT_API void Cleanup();
}

#endif // CLIENT_H