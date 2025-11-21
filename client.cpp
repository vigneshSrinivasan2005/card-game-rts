bool Connect(const char* address, int port){
    #ifdef _WIN32  // windows specific initialization
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed." << std::endl;
            return false;
        }
    #endif

    

}