#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_CLIENTS 50
#define MAX_SESSIONS 25
#define PORT 1100
#define BOARD_SIZE 8



// structure containing information about the game session
// each game session needs 2 player to start the game
typedef struct GameSession
{
  int sessionID; // index of the session on the sessions array
  int fp_socket; // firsts player socket
  int sp_socket; // seconds player socket
  char board[BOARD_SIZE][BOARD_SIZE]; 
  int board_pieces; // number of pieces on the board - used for checking the game end condition
  int game_end; // variable that is used while determing if the game is supposed to end
  int fp_legal_moves; // 1 - has legal moves, 0 - no legal moves
  int sp_legal_moves; // 1 - has legal moves, 0 - no legal moves
} game_session;


// stucter containing information about client connection
typedef struct Client
{
    int socket; 
    int index; // index on the users array
    int player_number; // 1 for the first player, 2 for the second player
    struct GameSession *session_ptr; // Pointer to a GameSession that Client is in
} client;


// enums for move validation
typedef enum {
    NONE = 0,
    HORIZONTAL = 1,
    VERTICAL = 2,
    DIAGONAL_LEFT_UP_RIGHT_DOWN = 4,
    DIAGONAL_RIGHT_UP_LEFT_DOWN = 8
} MoveDirection;

// arays containing clients and sessions
struct Client clients[MAX_CLIENTS] = {0};
struct GameSession sessions[MAX_SESSIONS] = {0};


// reversi game

// sets up starting position for reversi game and initializes other game_session variables
void initializeBoard(game_session *session)
{
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        for (int j = 0; j < BOARD_SIZE; j++)
        {
            session->board[i][j] = '.';
        }
    }

    // Set initial player pieces
    session->board[3][3] = 'X';
    session->board[3][4] = 'O';
    session->board[4][3] = 'O';
    session->board[4][4] = 'X';
    session->board_pieces = 4;
    session->game_end = 0;
    session->sp_legal_moves = 1;
    session->fp_legal_moves = 1;
}

void displayBoard(const game_session *session)
{
    printf("  A B C D E F G H\n");
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        printf("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++)
        {
            printf("%c ", session->board[i][j]);
        }
        printf("\n");
    }

}

// function that transforms the board 2d array to a string and sends it to the player on the playerSocket
void sendBoardInfo(int playerSocket, const game_session *session) {
    char boardInfo[256];
    memset(boardInfo, 0, sizeof(boardInfo));

    // format the board information into a string
    snprintf(boardInfo, sizeof(boardInfo), "Board Information:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            snprintf(boardInfo + strlen(boardInfo), sizeof(boardInfo) - strlen(boardInfo), "%c ", session->board[i][j]);
        }
        snprintf(boardInfo + strlen(boardInfo), sizeof(boardInfo) - strlen(boardInfo), "\n");
    }
    send(playerSocket, boardInfo, strlen(boardInfo), 0);
}

// function that returns MoveDirection (see structs at the begginig)
// function checks if the move is valid by iterating through the table in each direction 
//      from the passed move cooridantes. It does iterate as long as he touch the bound or until there are any enemys pieces
// if the move is valid in the direction it uses AND on enum and temporary variable
MoveDirection validateMove(const game_session *session, int socket, int x, int y){
    char opponent, currentPlayer;
    if(socket == session->fp_socket)
    {
        currentPlayer = 'X';
        opponent = 'O';
    } else{
        currentPlayer = 'O';
        opponent = 'X';
    }

    MoveDirection validDirections = NONE;
    int i = x, j;
    // Check horizontally
    for (j = y - 1; j >= 0 && session->board[i][j] == opponent; --j);
    if (j >= 0 && j != y - 1 && session->board[i][j] == currentPlayer)
        validDirections |= HORIZONTAL;

    for (j = y + 1; j < BOARD_SIZE && session->board[i][j] == opponent; ++j);
    if (j < BOARD_SIZE && j != y + 1 && session->board[i][j] == currentPlayer)
        validDirections |= HORIZONTAL;

    // Check vertically
    i = x - 1;
    for (j = y; i >= 0 && session->board[i][j] == opponent; --i);
    if (i >= 0 && i != x - 1 && session->board[i][j] == currentPlayer)
        validDirections |= VERTICAL;

    for (i = x + 1; i < BOARD_SIZE && session->board[i][j] == opponent; ++i);
    if (i < BOARD_SIZE && i != x + 1 && session->board[i][j] == currentPlayer)
        validDirections |= VERTICAL;

    // Check diagonally (left-up to right-down)
    i = x - 1, j = y - 1;
    while (i >= 0 && j >= 0 && session->board[i][j] == opponent) {
        --i;
        --j;
    }
    if (i >= 0 && j >= 0 && i != x - 1 && j != y - 1 && session->board[i][j] == currentPlayer)
        validDirections |= DIAGONAL_LEFT_UP_RIGHT_DOWN;

    i = x + 1, j = y + 1;
    while (i < BOARD_SIZE && j < BOARD_SIZE && session->board[i][j] == opponent) {
        ++i;
        ++j;
    }
    if (i < BOARD_SIZE && j < BOARD_SIZE && i != x + 1 && j != y + 1 && session->board[i][j] == currentPlayer)
        validDirections |= DIAGONAL_LEFT_UP_RIGHT_DOWN;

    // Check diagonally (right-up to left-down)
    i = x - 1, j = y + 1;
    while (i >= 0 && j < BOARD_SIZE && session->board[i][j] == opponent) {
        --i;
        ++j;
    }
    if (i >= 0 && j < BOARD_SIZE && i != x - 1 && j != y + 1 && session->board[i][j] == currentPlayer)
        validDirections |= DIAGONAL_RIGHT_UP_LEFT_DOWN;

    i = x + 1, j = y - 1;
    while (i < BOARD_SIZE && j >= 0 && session->board[i][j] == opponent) {
        ++i;
        --j;
    }
    if (i < BOARD_SIZE && j >= 0 && i != x + 1 && j != y - 1 && session->board[i][j] == currentPlayer)
        validDirections |= DIAGONAL_RIGHT_UP_LEFT_DOWN;

    return validDirections;
} 

// similar to the previous one but this one is changing the board
void executeMove(game_session *session, int x, int y, MoveDirection direction, int socket) {
    char opponent, currentPlayer;
    if(socket == session->fp_socket)
    {
        currentPlayer = 'X';
        opponent = 'O';
    } else{
        currentPlayer = 'O';
        opponent = 'X';
    }

    session->board[x][y] = currentPlayer;

    if (direction & HORIZONTAL) {
        for (int j = y - 1; j >= 0 && session->board[x][j] == opponent; --j) {
            session->board[x][j] = currentPlayer;
        }
        for (int j = y + 1; j < BOARD_SIZE && session->board[x][j] == opponent; ++j) {
            session->board[x][j] = currentPlayer;
        }
    }

    if (direction & VERTICAL) {
        for (int i = x - 1; i >= 0 && session->board[i][y] == opponent; --i) {
            session->board[i][y] = currentPlayer;
        }
        for (int i = x + 1; i < BOARD_SIZE && session->board[i][y] == opponent; ++i) {
            session->board[i][y] = currentPlayer;
        }
    }

    if (direction & DIAGONAL_LEFT_UP_RIGHT_DOWN) {
        for (int i = x - 1, j = y - 1; i >= 0 && j >= 0 && session->board[i][j] == opponent; --i, --j) {
            session->board[i][j] = currentPlayer;
        }
        for (int i = x + 1, j = y + 1; i < BOARD_SIZE && j < BOARD_SIZE && session->board[i][j] == opponent; ++i, ++j) {
            session->board[i][j] = currentPlayer;
        }
    }

    if (direction & DIAGONAL_RIGHT_UP_LEFT_DOWN) {
        for (int i = x - 1, j = y + 1; i >= 0 && j < BOARD_SIZE && session->board[i][j] == opponent; --i, ++j) {
            session->board[i][j] = currentPlayer;
        }
        for (int i = x + 1, j = y - 1; i < BOARD_SIZE && j >= 0 && session->board[i][j] == opponent; ++i, --j) {
            session->board[i][j] = currentPlayer;
        }
    }
    session->board_pieces++;
}

void checkEndGameConditions(game_session *session, int socket){
    int opponent, currentPlayer; // assign sockets for players in session
    if(socket == session->fp_socket)
    {
        currentPlayer = session->fp_socket;
        opponent = session->sp_socket;
    } else{
        currentPlayer = session->sp_socket;
        opponent = session->fp_socket;
    }

    if (session->board_pieces == BOARD_SIZE*BOARD_SIZE) // check if board is full
    {
        session->game_end = 1;
    } else{ 
        session->game_end = 1;
        // check if oponent has any legal moves left
        for (int i = 0; i<BOARD_SIZE; i++){
            for (int j = 0; j<BOARD_SIZE; i++){
                if (validateMove(session, opponent, i, j) > 0);
                    session->game_end = 0;
                    
                    break;
            }
        }

        // if oponent doesnt have any legal moves left then check if current player (who made a move) has any legal moves
        if (session->game_end == 1)
        {
            if(socket == session->fp_socket) // mark legal_moves flag
            {
                session->sp_legal_moves = 0;
            } else{
                session->fp_legal_moves = 0;
            }
            for (int i = 0; i<BOARD_SIZE; i++){
                for (int j = 0; j<BOARD_SIZE; i++){
                    if (validateMove(session, currentPlayer, i, j) > 0);
                        session->game_end = 0;
                        break;
                }
            }
        }
    }

    
}
// client-server communication

void *socketThread(void *arg)
{
    printf("New thread\n");
    struct Client* newSocket = (struct Client*)arg; // pointer to an address passed as argument which is element of clients arrays - which is Client struct

    int n;
    char client_message[2000]; // declare client_message here
    
    int player_number = newSocket->player_number;
    printf("player number: %d - Session: %d\n", player_number, newSocket->session_ptr->sessionID);

    // notify the first client in the session
    if (player_number == 1)
    {
        send(newSocket->socket, "You are the first player (X), wait for the second player to connect.\n", strlen("You are the first player (X), wait for the second player to connect.\n"), 0);
    }
    else // notify the second player in the session
    {
        send(newSocket->socket, "You are the second player (O), wait for the first player to make a move.\n", strlen("You are the second player (O), wait for the first player to make a move.\n"), 0);
    }


    //set the socket of others player
    int otherPlayerSocket;
    if (player_number == 2)
    {
      otherPlayerSocket = newSocket->session_ptr->fp_socket;
    }

    
    // make p1 wait until p2 connects and check if p1 didn't disconnect during waittime
    if (player_number == 1){ 
        while (newSocket->session_ptr->sp_socket <= 0){ 
            n = recv(newSocket->socket, client_message, sizeof(client_message), MSG_DONTWAIT);
            if (n == 0) 
            {
              printf("client %d disconnected\n", newSocket->socket);
              close(newSocket->socket);
              // mark the client as disconnected

              sessions[newSocket->session_ptr->sessionID].fp_socket = -1;
              clients[newSocket->index].socket = -1;

              pthread_exit(NULL);
            }
        }

        // start the game after the 2nd player has connected
        otherPlayerSocket = newSocket->session_ptr->sp_socket;
        initializeBoard(newSocket->session_ptr);
        sendBoardInfo(newSocket->socket, newSocket->session_ptr);
    }
    
    // both players connected, the game is on
    for (;;) {
        n = recv(newSocket->socket, client_message, sizeof(client_message), 0);
        // check if player disconected
        if (n < 1) {
            break;
        }
        // check if the received message is "exit"
        if (strcmp(client_message, "exit") == 0) {
            break;
        }

        client_message[n] = '\0';

        int x, y;
        char col;
        MoveDirection is_valid = 0;

        // handle move input
        if (sscanf(client_message, "%c%d", &col, &y) == 2) { // CHECK IF VALID FORMAT OF INPUT
            x = toupper(col) - 'A';
            
            if (x >= 0 && x < BOARD_SIZE && y >= 1 && y <= BOARD_SIZE) { // CHECK IF PASSED COORIDANTES ARE IN RANGE
                printf("%d %d\n", x, y);

                if (newSocket->session_ptr->board[y-1][x] == '.') { // CHECK IF PASSED POSITION US EMPTY

                    is_valid = validateMove(newSocket->session_ptr, newSocket->socket, y-1, x); //validate move
                    printf("%d\n", is_valid);

                    if (is_valid > 0) // EXECUTE MOVE AND CHECK IF THE GAME HAS ENDED
                    { 
                        executeMove(newSocket->session_ptr, y-1, x, is_valid, newSocket->socket);
                        checkEndGameConditions(newSocket->session_ptr, newSocket->socket);
                        
                        if (newSocket->session_ptr->game_end == 1) break; // END GAME
                        else{ // IF GAME IS STILL ON THEN CHECK IF ANY PLAYER HAS NO MORE LEGAL MOVES AND LET THE OTHER PLAY UNTIL BOTH HAVE 0 LEGAL MOVES
                            // if (newSocket->session_ptr->fp_socket == newSocket->socket)
                            if (newSocket->player_number == 1)
                            {
                                if (newSocket->session_ptr->sp_legal_moves == 1)
                                {
                                    sendBoardInfo(otherPlayerSocket, newSocket->session_ptr); // IF OTHER PLAYER HAS MOVES LET HIM PLAY
                                } else{
                                    sendBoardInfo(newSocket->socket, newSocket->session_ptr); // IF NO THEN LET THE PLAYER MOVE AGAIN
                                }
                            } else{
                                if (newSocket->session_ptr->fp_legal_moves == 1)
                                {
                                    sendBoardInfo(otherPlayerSocket, newSocket->session_ptr);
                                } else{
                                    sendBoardInfo(newSocket->socket, newSocket->session_ptr);
                                }
                            }
                        }

                        
                    } else{ // SEND INFO ABOUT NOT VALID MOVE AND REPEAT BOARD INFO
                        send(newSocket->socket, "Invalid move. You can't put a piece in this position.\n", strlen("Invalid move. You can't put a piece in this position.\n"), 0);
                        sendBoardInfo(newSocket->socket, newSocket->session_ptr);
                    }
                } else { // POSITION NOT EMPTY
                    send(newSocket->socket, "The selected position is not empty.\n", strlen("The selected position is not empty.\n"), 0);
                    sendBoardInfo(newSocket->socket, newSocket->session_ptr);
                }            
            } else { // OUT OF RANGE
                send(newSocket->socket, "Invalid position (out of range).\n", strlen("Invalid position (out of range).\n"), 0);
                sendBoardInfo(newSocket->socket, newSocket->session_ptr);
            }
        } else { // INVALID FORMAT 
            send(newSocket->socket, "Invalid input format. Please use 'Char Number' format.\n", strlen("Invalid input format. Please use 'Char Number' format.\n"), 0);
            sendBoardInfo(newSocket->socket, newSocket->session_ptr);
        }  
        
    }

    printf("client %d disconnected\n", newSocket->socket);
    close(newSocket->socket);
    // Mark the client as disconnected
    if (player_number == 1)
    {
      sessions[newSocket->session_ptr->sessionID].fp_socket = -1;
    } else if (player_number == 2){ 
      sessions[newSocket->session_ptr->sessionID].sp_socket = -1;
    }
    clients[newSocket->index].socket = -1;
    
    if (newSocket->session_ptr->game_end == 1){
        //count pieces
        int xs = 0;
        int os = 0;
        for (int i = 0; i < BOARD_SIZE; i++){
            for (int j = 0; j < BOARD_SIZE; j++){
                if (newSocket->session_ptr->board[i][j] == 'X') xs++;
                if (newSocket->session_ptr->board[i][j] == 'O') os++;
            }
        }
        if (xs > os){
            send(newSocket->socket, "The game has ended. Winner: Player 1 (X)\n", strlen("The game has ended. Winner: Player 1 (X)\n"), 0);
            send(otherPlayerSocket, "The game has ended. Winner: Player 1 (X)\n", strlen("The game has ended. Winner: Player 1 (X)\n"), 0);
        } else if (xs == os){
            send(newSocket->socket, "The game has ended. There was a tie.\n", strlen("The game has ended. There was a tie.\n"), 0);
            send(otherPlayerSocket, "The game has ended. There was a tie.\n", strlen("The game has ended. There was a tie.\n"), 0);
        } else{
            send(newSocket->socket, "The game has ended. Winner: Player 2 (X)\n", strlen("The game has ended. Winner: Player 2 (X)\n"), 0);
            send(otherPlayerSocket, "The game has ended. Winner: Player 2 (X)\n", strlen("The game has ended. Winner: Player 2 (X)\n"), 0);
        }
        
    } else{
        send(otherPlayerSocket, "The oponent has left the game, you win.\n", strlen("The oponent has left the game, you win.\n"), 0);
    }
    pthread_exit(NULL);
}

int main()
{
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;

    // Create the socket.
    serverSocket = socket(PF_INET, SOCK_STREAM, 0);

    // Configure settings of the server address struct
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    // Bind the address struct to the socket
    bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    // Listen on the socket
    if (listen(serverSocket, MAX_CLIENTS) == 0)
        printf("Listening\n");
    else
        printf("Error\n");

    pthread_t thread_id;

    int session = 0; // session counter
    
    int i = 0; // user counter
    while (1)
    {
        addr_size = sizeof serverStorage;
        newSocket = accept(serverSocket, (struct sockaddr *)&serverStorage, &addr_size);

        if (i < MAX_CLIENTS)
        {
            if (clients[i].socket <= 0) // if socket not taken
            {
                clients[i].socket = newSocket;
                clients[i].index = i;

                if (pthread_create(&thread_id, NULL, socketThread, &clients[i]) != 0)
                    printf("Failed to create thread\n");

                clients[i].session_ptr = &sessions[session]; // connect player with session
                sessions[session].sessionID = session;

                if (sessions[session].fp_socket <= 0) // if the first player hasn't joined yet then assign currents player socket to fp_socket
                {
                  sessions[session].fp_socket = newSocket;
                  clients[i].player_number = 1;
                }
                else if (sessions[session].sp_socket <= 0) // if the first player has joined then check if the second slot is empty
                {
                  sessions[session].sp_socket = newSocket; // attack socket to sp_socket
                  clients[i].player_number = 2;
                  session++; // close the session for other players
                }


                pthread_detach(thread_id);
            }
        }
        else
        {
            printf("Maximum number of clients reached. Connection rejected.\n");
            close(newSocket);
            break;
        }

        // Move to the next client slot
        ++i;
    }
    return 0;
}



