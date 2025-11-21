#include <iostream>
#include <string>

// --- OS-SPECIFIC NETWORKING HEADERS AND MACROS ---

#ifdef _WIN32 // Check if compiling on Windows
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") 

    typedef SOCKET SocketHandle; // Define a consistent type for sockets
    #define CLOSE_SOCKET(s) closesocket(s) 
#else // Assume macOS/Linux (POSIX)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cstring>

    typedef int SocketHandle; //use a type for the sockets
    #define CLOSE_SOCKET(s) close(s)
#endif 

// ... DLL/DYLIB Export Macro (Needs to be adjusted for macOS/GCC/Clang) ...
// For macOS/Linux, the syntax for dynamic library exports is different.
// GCC/Clang often use __attribute__((visibility("default"))).
#ifdef _WIN32
    #define EXPORT_API __declspec(dllexport)
#else
    #define EXPORT_API __attribute__((visibility("default"))) // For .dylib
#endif

extern "C" EXPORT_API bool Connect(const char* address, int port);
extern "C" EXPORT_API void AddLocalCommand(int unit_id, int command_type, double target_x, double target_y);
extern "C" EXPORT_API bool SendStep();
extern "C" EXPORT_API bool GetNextCommand(char* buffer, int buffer_size); // Simplified return signature

typedef struct {
    int unit_id;
    int command_type;
    double target_x;
    double target_y;
} Command;