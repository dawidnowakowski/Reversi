import socket

HOST = "127.0.0.1"
PORT = 1104

def parse_board_info(message):
    # Split the message into lines
    lines = message.split('\n')
    if 'Board' not in lines[0]:
        print(lines[0] + '\n')
        lines.remove(lines[0])
    
    print(lines[0])
    lines.remove(lines[0])
    
    # Extract the board information
    board_info = [line.split() for line in lines]

    # Convert characters to a 2D array
    board_array = [[char for char in line] for line in board_info]

    return board_array

def printBoard(board_array):
    print("  A B C D E F G H")
    # print(len(board_array))
    # print(board_array)
    for i, row in enumerate(board_array[:len(board_array)-1]):
        print(f"{i + 1} {' '.join(row)}")
    print()


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    GAME_STATUS = True

    # Receive initial message from the server
    initial_message = s.recv(1024).decode()
    print(initial_message)


    while GAME_STATUS:
        data = s.recv(1024).decode()

        if 'left the game' in data: # p2 disconnected
            print(data)
            break
        else: # game info
            if 'Board' not in data:
                print(data)
                data = s.recv(1024).decode()
                board_array = parse_board_info(data)
                printBoard(board_array)
            else:
                
                board_array = parse_board_info(data)
                printBoard(board_array)

        
        msg = input("\nEnter a move (type 'exit' to quit):\n")
        if msg == "exit":
            break
        s.sendall(bytes(msg, "utf-8"))
        print("Wait for oponents move")
        

        

    print("Closing the connection.")
