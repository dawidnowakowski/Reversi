#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 50
#define MAX_SESSIONS 25
#define PORT 1101
#define BOARD_SIZE 8

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
    printf("  0 1 2 3 4 5 6 7\n");
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        printf("%d ", i);
        for (int j = 0; j < BOARD_SIZE; j++)
        {
            printf("%c ", session->board[i][j]);
        }
        printf("\n");
    }
    printf("\n");
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
        notifyPlayer(newSocket->socket, "You are the first player, wait for the second player to connect.\n");
    }
    else
    {
        notifyPlayer(newSocket->socket, "You are the second player, wait for the first player to enter some message.\n");
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
      notifyPlayer(newSocket->socket, "The second player joined the game session. You can send a message now.\n");
      initializeBoard(newSocket->session_ptr);
      displayBoard(newSocket->session_ptr);
    }
  

    for (;;) {
    n = recv(newSocket->socket, client_message, sizeof(client_message), 0);
    if (n < 1) {
        break;
    }

    // Check if the received message is "exit"
    if (strcmp(client_message, "exit") == 0) {
        break;
    }

    // Send the received message to the other player
    send(otherPlayerSocket, client_message, n, 0);
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



