#include "lobby.h"
#include "shared.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

// --- GLOBAL VARIABLE DEFINITIONS ---
// These are declared 'extern' in shared.h, but MUST be defined (created) here.
std::vector<GameRoom> g_Games;
pthread_mutex_t g_LobbyMutex = PTHREAD_MUTEX_INITIALIZER;
int g_server_sock = -1;

void cleanup_and_exit(int sig) {
    cout << "\nServer shutting down..." << endl;
    if (g_server_sock != -1) close(g_server_sock);
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, cleanup_and_exit);

    int port = 8080;
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (g_server_sock < 0) {
        cerr << "Socket creation failed" << endl;
        return 1;
    }

    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    int opt = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(g_server_sock, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Bind failed on port " << port << endl;
        return 1;
    }

    if (listen(g_server_sock, 100) < 0) {
        cerr << "Listen failed" << endl;
        return 1;
    }

    cout << "--- RTS SERVER ONLINE ---" << endl;
    cout << "Listening on port " << port << endl;

    while (true) {
        int clientSock = accept(g_server_sock, NULL, NULL);
        if (clientSock < 0) continue;
        
        cout << "New Client Connected: " << clientSock << endl;

        // Spawn a Lobby Thread for the new client
        int* arg = new int(clientSock);
        pthread_t t;
        // We call the function from lobby.cpp
        if (pthread_create(&t, NULL, HandleClientLobby, arg) != 0) {
            cerr << "Failed to create thread" << endl;
            delete arg;
            close(clientSock);
        } else {
            pthread_detach(t);
        }
    }
    return 0;
}