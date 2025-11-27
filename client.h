#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

typedef struct {
    uint32_t unit_id; //4 bytes //0 means no id yet 
    uint32_t command_type; //4 bytes 1 means move 2 means attack 3 means place
    uint32_t unit_type; //4 bytes used only for place command
    double target_x; //8 bytes
    double target_y; //8 bytes
} Command;

#ifdef _WIN32 // Check if compiling on Windows
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") 

    typedef SOCKET SocketHandle; // Define a consistent type for sockets
    #define CLOSE_SOCKET(s) closesocket(s) 

    #include <BaseTsd.h> 
    typedef SSIZE_T ssize_t;

#else // Assume macOS/Linux (POSIX)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>   
    #include <unistd.h>
    #include <cstring>
    #include <sys/types.h>
    
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

using namespace std;

extern SocketHandle gSocket;
static vector<Command> gCommandBuffer;
static vector<Command> unprocessedCommands;

extern "C" EXPORT_API bool DLLConnect(const char* address, int port);
extern "C" EXPORT_API void AddLocalCommand(int unit_id, int command_type, double target_x, double target_y);
extern "C" EXPORT_API void addPlaceCommand(int unit_type, double target_x, double target_y);
extern "C" EXPORT_API bool SendStep();
extern "C" EXPORT_API bool GetNextCommand(char* buffer, int buffer_size); // Simplified return signature
extern "C" EXPORT_API bool hasUnprocessedCommands();
extern "C" EXPORT_API void Cleanup();

bool RecieveData(char* buffer , int expected_size);
