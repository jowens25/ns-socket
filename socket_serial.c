
#include "socket_serial.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "circular_buffer.h"
#include <errno.h>
#include <sys/select.h>
#include "stdio.h"
#include "string.h" //strlen
#include "stdlib.h"
#include "errno.h"
#include "unistd.h"    //close
#include "arpa/inet.h" //close
#include "sys/types.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "sys/time.h" //FD_SET, FD_ISSET, FD_ZERO macros

#include "socket_serial.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include "circular_buffer.h"

char chunk[CHUNK_SIZE] = {0};
char socket_chunk[CHUNK_SIZE] = {0};
// read from serial file descriptor into ser buff
int readSerial(int ser)
{
    int n = read(ser, chunk, CHUNK_SIZE);
    if (n > 0)
    {
        cb_write_chunk(&ser_cb, chunk, n);
    }
    return n;
}

// write to serial file descriptor from sock buff
void writeSerial(int ser)
{
    char tx[BUFFER_SIZE];
    int bytes_read = cb_read_chunk(&sock_cb, tx, BUFFER_SIZE);

    if (bytes_read == 0)
    {
        return;
    }

    int n = write(ser, tx, bytes_read);

    if (n < 0)
    {
        if (errno == EPIPE || errno == ECONNRESET || errno == EBADF)
        {
            printf("Serial write error");
        }
    }

}

// read socket client data in to socket buffer
void readSocket(int client)
{
    int n = read(client, socket_chunk, CHUNK_SIZE);
    if (n > 0)
    {
        cb_write_chunk(&sock_cb, socket_chunk, n);
    }
}

// write data from serial buffer to each socket client
void writeSocket(int client, char *tx, int bytes_read)
{
    int n = write(client, tx, bytes_read);
    if (n < 0)
    {
        if (errno == EPIPE || errno == ECONNRESET || errno == EBADF)
        {
            printf("Client %d disconnected\n", client);
            // close(client);
        }
    }
}

//
void writeSockets(int *clients, fd_set writefds, fd_set readfds)
{

    char tx[BUFFER_SIZE];
    int bytes_read = cb_read_chunk(&ser_cb, tx, BUFFER_SIZE);

    if (bytes_read == 0)
    {
        return;
    }

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        // if clients are valid and set
        if (clients[i] != -1 && (FD_ISSET(clients[i], &writefds)))
        {
            // printf("write to client: %d\n", clients[i]);

            int n = write(clients[i], tx, bytes_read);

            if (n < 0)
            {
                if (errno == EPIPE || errno == ECONNRESET || errno == EBADF)
                {
                    printf("Client %d disconnected\n", clients[i]);
                    // close(client);
                }
            }
        }
    }
}

void socketSetup(int sock)
{
    // socket setup
    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH); // Remove stale file
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind failed");
        exit(1);
    }
    if (listen(sock, MAX_CONNECTIONS) == -1)
    {
        perror("listen failed");
        exit(1);
    }
    set_nonblocking(sock);
    printf("Serving %s on %s\r\n", SERIAL_PORT, SOCKET_PATH);
}

int serialSetup(int fd)
{
    // printf("setup termios\n");
    memset(&tty, 0, sizeof tty);

    if (tcgetattr(fd, &tty) != 0)
    {
        printf("tcgetattr");
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, B38400);
    cfsetispeed(&tty, B38400);

    // 8N1 configuration
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    // tty.c_cflag &= ~CRTSCTS; // Disable hardware flow control
    tty.c_cflag |= (CLOCAL | CREAD);

    // Input processing - disable all special processing
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Output processing - raw output
    tty.c_oflag &= ~OPOST;

    // CRITICAL FIX: Disable canonical mode for character-by-character reading
    tty.c_lflag &= ~ICANON; // Raw mode - read character by character
    tty.c_lflag &= ~(ECHO | ECHONL | ISIG | IEXTEN);

    // Timeout settings for raw mode
    tty.c_cc[VMIN] = 8;   // wait for some chars
    tty.c_cc[VTIME] = 15; // Timeout in deciseconds (1.5 seconds)

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        printf("tcsetattr error? \n");
        perror("tcsetattr");

        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return 0;
}

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}