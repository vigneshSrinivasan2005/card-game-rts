Basic Client Server WorkFlow

Client
-> connect ->                           -> Use SMTP like protocol to send chat, view create and join lobbies-> On Join Lobby                                                                     -> On Recieve Start Match -> Send ACK ->                                          -> Recieve Player Id's and send binary command messages from here on out.
Server 
a                Send Welcome to Lobby                                                                                           -> Wait For Two Players-> Send Start Match Message                                     ->On Recieve Ack-> Send Player ID's 



Compiling the Project

For the server naviagate to the server file and run the following command

g++ -o server main.cpp lobby.cpp game_instance.cpp user_persistence.cpp shared.cpp -std=c++11 -lpthread

./server

For the Client Game you have 2 options
1. If on MAC

https://gamemaker.io/en/download // go to this link and install

Click on the YYC file and open in gamemaker

2. If on windows can just Run the .exe file. If the code fails run the following line to recompile the dll in the client folder and drop it and replace the dll in the application folder.
g++ -shared -o Client.so fakeClient.cpp -std=c++11 -fPIC





