#include "client.h"

int main(){
    // Example usage
    if (!DLLConnect("127.0.0.1", 8080)) {
        cerr << "Failed to connect to server." << endl;
        return -1;
    }

}