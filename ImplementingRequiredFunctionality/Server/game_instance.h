#ifndef GAME_INSTANCE_H
#define GAME_INSTANCE_H

struct MatchArgs {
    int client1_sock;
    int client2_sock;
    int gameId;
};

// The main loop for the actual game
void* HandleMatch(void* args);

#endif