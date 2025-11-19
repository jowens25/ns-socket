import socket
import sys

socket_path = "/tmp/serial.sock"
client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
client.connect(socket_path)

try:
    while True:
        response = client.recv(1024)
        if not response:
            break
        sys.stdout.write(response.decode())  # No extra newline
        sys.stdout.flush()
finally:
    client.close()
