import socket

HOST = "127.0.0.1"
PORT = 1105

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))

    # Receive initial message from the server
    initial_message = s.recv(1024).decode()
    print('init: ' + initial_message)


    temp = s.recv(1024).decode()
    print('msg:' + temp)

    while True:
    
        msg = input("Enter a message (type 'exit' to quit): ")
        if msg == "exit":
            break
        s.sendall(bytes(msg, "utf-8"))
        data = s.recv(1024)
        print(f"Received from server: {data.decode()}")

    print("Closing the connection.")

