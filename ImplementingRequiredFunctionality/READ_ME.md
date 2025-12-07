Basic Client Server WorkFlow

Client
-> connect ->                           -> Use SMTP like protocol to send chat, view create and join lobbies-> On Join Lobby                                                                     -> On Recieve Start Match -> Send ACK ->                                          -> Recieve Player Id's and send binary command messages from here on out.
Server 
a                Send Welcome to Lobby                                                                                           -> Wait For Two Players-> Send Start Match Message                                     ->On Recieve Ack-> Send Player ID's 