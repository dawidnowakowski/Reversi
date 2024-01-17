import socket

HOST = "127.0.0.1"
PORT = 1101

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    GAME_STATUS = True
    # Receive initial message from the server
    initial_message = s.recv(1024).decode()
    print(initial_message)


    temp = s.recv(1024).decode()
    print(temp)

    if 'left the game' in temp:
        GAME_STATUS = False


    while GAME_STATUS:
    
        msg = input("Enter a message (type 'exit' to quit):\n")
        if msg == "exit":
            break
        s.sendall(bytes(msg, "utf-8"))
        data = s.recv(1024).decode()
        if 'left the game' in data:
            print(data)
            break
        print(data)

    print("Closing the connection.")