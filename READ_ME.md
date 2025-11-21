# PLAN FOR THE NETWORK ARCHITECTURE
# client.cpp must compile into a dll, so that it can be used in the game
# 3 main methods I think for now
# QUESTIONS:
# udp or tcp: TCP because I want reliability in terms of the messages getting transferred , as each player is running a seperate game with just the opp moving as the messages say they are moving 
# Client server because we want to prevent cheating
# Client methods
# Connect -> simply connects to the server and creates the connection
# SendStep() - > sends the step to the server and waits until the server responds with the opponents step
# AddMove -> Unit ID, X, Y
# 
#