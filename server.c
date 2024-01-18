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
#define PORT 1103
#define BOARD_SIZE 8
#define MAX_MESSAGE_SIZE 256

typedef struct GameSession
{
  int sessionID;
  int fp_socket;
  int sp_socket;
  char board[BOARD_SIZE][BOARD_SIZE];
} game_session;

typedef struct Client
{
    int socket;
    int index;
    int player_number; // 1 for the first player, 2 for the second player
    struct GameSession *session_ptr; // Pointer to a GameSession
} client;

typedef enum {
    NONE = 0,
    HORIZONTAL = 1,
    VERTICAL = 2,
    DIAGONAL_LEFT_UP_RIGHT_DOWN = 4,
    DIAGONAL_RIGHT_UP_LEFT_DOWN = 8
} MoveDirection;

struct Client clients[MAX_CLIENTS] = {0};
struct GameSession sessions[MAX_SESSIONS] = {0};


// reversi game
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

void sendBoardInfo(int playerSocket, const game_session *session) {
    char boardInfo[256];

    // Reset the boardInfo string
    memset(boardInfo, 0, sizeof(boardInfo));

    // Format the board information into a string
    snprintf(boardInfo, sizeof(boardInfo), "Board Information:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // printf("test %c\n", session->board[i][j]);
            snprintf(boardInfo + strlen(boardInfo), sizeof(boardInfo) - strlen(boardInfo), "%c ", session->board[i][j]);
        }
        snprintf(boardInfo + strlen(boardInfo), sizeof(boardInfo) - strlen(boardInfo), "\n");
    }

    // Send the formatted board information to the player
    // printf("%s\n", boardInfo);
    send(playerSocket, boardInfo, strlen(boardInfo), 0);
}

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

    // Place the current player's piece in the specified position
    session->board[x][y] = currentPlayer;

    // Update the board based on the direction
    if (direction & HORIZONTAL) {
        // Update horizontally
        for (int j = y - 1; j >= 0 && session->board[x][j] == opponent; --j) {
            session->board[x][j] = currentPlayer;
        }
        for (int j = y + 1; j < BOARD_SIZE && session->board[x][j] == opponent; ++j) {
            session->board[x][j] = currentPlayer;
        }
    }

    if (direction & VERTICAL) {
        // Update vertically
        for (int i = x - 1; i >= 0 && session->board[i][y] == opponent; --i) {
            session->board[i][y] = currentPlayer;
        }
        for (int i = x + 1; i < BOARD_SIZE && session->board[i][y] == opponent; ++i) {
            session->board[i][y] = currentPlayer;
        }
    }

    if (direction & DIAGONAL_LEFT_UP_RIGHT_DOWN) {
        // Update diagonally (left-up to right-down)
        for (int i = x - 1, j = y - 1; i >= 0 && j >= 0 && session->board[i][j] == opponent; --i, --j) {
            session->board[i][j] = currentPlayer;
        }
        for (int i = x + 1, j = y + 1; i < BOARD_SIZE && j < BOARD_SIZE && session->board[i][j] == opponent; ++i, ++j) {
            session->board[i][j] = currentPlayer;
        }
    }

    if (direction & DIAGONAL_RIGHT_UP_LEFT_DOWN) {
        // Update diagonally (right-up to left-down)
        for (int i = x - 1, j = y + 1; i >= 0 && j < BOARD_SIZE && session->board[i][j] == opponent; --i, ++j) {
            session->board[i][j] = currentPlayer;
        }
        for (int i = x + 1, j = y - 1; i < BOARD_SIZE && j >= 0 && session->board[i][j] == opponent; ++i, --j) {
            session->board[i][j] = currentPlayer;
        }
    }
}


// client-server communication
void notifyPlayer(int playerSocket, const char *message)
{
    send(playerSocket, message, strlen(message), 0);
}

void *socketThread(void *arg)
{
    printf("New thread\n");
    struct Client* newSocket = (struct Client*)arg;

    int n;

    char client_message[2000]; // Declare client_message here
    
    
    int player_number = newSocket->player_number;
    printf("player number: %d - Session: %d\n", player_number, newSocket->session_ptr->sessionID);

    // Notify the first client in the session
    if (player_number == 1)
    {
        notifyPlayer(newSocket->socket, "You are the first player (X), wait for the second player to connect.\n");
    }
    else
    {
        notifyPlayer(newSocket->socket, "You are the second player (O), wait for the first player to enter some message.\n");
    }


    //set the socket of others player
    int otherPlayerSocket;
    if (player_number == 2)
    {
      otherPlayerSocket = newSocket->session_ptr->fp_socket;
    }

    
    // make p1 wait until p2 connects and check if it didn't disconnect during waittime
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
        otherPlayerSocket = newSocket->session_ptr->sp_socket;
    //   notifyPlayer(newSocket->socket, "The second player joined the game session. You can send a message now.\n");
        initializeBoard(newSocket->session_ptr);
        sendBoardInfo(newSocket->socket, newSocket->session_ptr);
      
        displayBoard(newSocket->session_ptr);
    }
    
    // both players connected, the game is on
    for (;;) {
        n = recv(newSocket->socket, client_message, sizeof(client_message), 0);
        // check if player disconected
        if (n < 1) {
            break;
        }
        // Check if the received message is "exit"
        if (strcmp(client_message, "exit") == 0) {
            break;
        }

        // handle moves
        client_message[n] = '\0';

        int x, y;
        char col;
        MoveDirection is_valid = 0;

        if (sscanf(client_message, "%c%d", &col, &y) == 2) {
            x = toupper(col) - 'A';
            
            if (x >= 0 && x < BOARD_SIZE && y >= 1 && y <= BOARD_SIZE) {
                printf("%d %d\n", x, y);
                if (newSocket->session_ptr->board[y-1][x] == '.') { // CHECK IF POSITION EMPTY
                    is_valid = validateMove(newSocket->session_ptr, newSocket->socket, y-1, x); // VALIDATE MOVE
                    printf("%d\n", is_valid);
                    if (is_valid > 0)
                    { // EXECUTE MOVE AND SEND NEW BOARD TO OTHER PLAYER
                        executeMove(newSocket->session_ptr, y-1, x, is_valid, newSocket->socket);
                        sendBoardInfo(otherPlayerSocket, newSocket->session_ptr);
                    } else{ // SEND INFO ABOUT NOT VALID MOVE AND REPEAT BOARD INFO
                        send(newSocket->socket, "Invalid move. You can't put a piece in this position.\n", strlen("Invalid move. You can't put a piece in this position.\n"), 0);
                        sendBoardInfo(newSocket->socket, newSocket->session_ptr);
                    }
                } else { // POSITION NOT EMPTY
                    send(newSocket->socket, "The selected position is not empty.\n", strlen("The selected position is not empty.\n"), 0);
                    sendBoardInfo(newSocket->socket, newSocket->session_ptr);
                    // is_valid = 0;
                }            
            } else { // OUT OF RANGE
                send(newSocket->socket, "Invalid position (out of range).\n", strlen("Invalid position (out of range).\n"), 0);
                sendBoardInfo(newSocket->socket, newSocket->session_ptr);
                // is_valid = 0;
            }
        } else { // INVALID FORMAT 
            send(newSocket->socket, "Invalid input format. Please use 'Char Number' format.\n", strlen("Invalid input format. Please use 'Char Number' format.\n"), 0);
            sendBoardInfo(newSocket->socket, newSocket->session_ptr);
            // is_valid = 0;
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
    notifyPlayer(otherPlayerSocket, "The oponent has left the game, you win.\n");
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

    int session = 0;
    
    int i = 0;
    while (1)
    {
        addr_size = sizeof serverStorage;
        newSocket = accept(serverSocket, (struct sockaddr *)&serverStorage, &addr_size);

        if (i < MAX_CLIENTS)
        {
            if (clients[i].socket <= 0)
            {
                clients[i].socket = newSocket;
                clients[i].index = i;

                if (pthread_create(&thread_id, NULL, socketThread, &clients[i]) != 0)
                    printf("Failed to create thread\n");

                clients[i].session_ptr = &sessions[session];
                sessions[session].sessionID = session;

                // printf("fp przed %d\n",sessions[session].fp_socket);
                // printf("sp przed %d\n",sessions[session].sp_socket);

                if (sessions[session].fp_socket <= 0)
                {
                  sessions[session].fp_socket = newSocket;
                  clients[i].player_number = 1;
                //   printf("fp po %d\n",sessions[session].fp_socket);
                }

                else if (sessions[session].sp_socket <= 0)
                {
                  sessions[session].sp_socket = newSocket;
                  clients[i].player_number = 2;
                //   printf("sp po %d\n",sessions[session].sp_socket);
                  session++;
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



