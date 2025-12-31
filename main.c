#include "main.h"
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
//
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

struct termios tty;
int clients[MAX_CONNECTIONS] = {-1, -1, -1, -1};
int max_fd;

char SERIAL_PORT[SERIAL_PORT_LEN] = {0};

int main(int argc, char *argv[])
{

    if (argv[1] == NULL)
    {
        printf("Please include a serial port\r\n");
        printf("example: %s /dev/ttymxc2\r\n", argv[0]);
        exit(-1);
    }

    strncpy(SERIAL_PORT, argv[1], strlen(argv[1]));

    cb_init(&ser_cb);
    cb_init(&sock_cb);

    // open and setup serial port
    int ser = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (ser < 0)
    {
        perror("unable to open serial port");
        return -1;
    }

    // open and setup socket
    int err = serialSetup(ser);
    if (err != 0)
    {
        printf("serial setup error");
    }
    set_nonblocking(ser);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("unable to create socket");
        close(sock);
        return -1;
    }
    socketSetup(sock);

    fd_set readfds;
    fd_set writefds;

    while (1)
    {

        // collect active file descriptors
        max_fd = 0;

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        // Always monitor serial for reading
        FD_SET(ser, &readfds);
        max_fd = MAX(ser, max_fd);

        // Only monitor serial for writing if we have data from sockets to send
        if (!cb_is_empty(&sock_cb))
        {
            FD_SET(ser, &writefds);
        }

        // Always monitor socket for new connections
        FD_SET(sock, &readfds);
        max_fd = MAX(sock, max_fd);

        // Monitor client connections
        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (clients[i] != -1)
            {
                // Always monitor clients for reading
                FD_SET(clients[i], &readfds);

                // Only monitor clients for writing if we have serial data to send
                if (!cb_is_empty(&ser_cb))
                {
                    FD_SET(clients[i], &writefds);
                }
            
                max_fd = MAX(clients[i], max_fd);
            }
        }

        struct timeval timeout = {0, 100000};
        int activity = select(max_fd + 1, &readfds, &writefds, NULL, &timeout);

        if (activity < 0)
        {
            perror("select error\r\n");
            break;
        }

        else if (activity == 0)
        {
            perror("timeout\r\n");
            continue;
        }

        else
        {

            // serial handlers
            if (FD_ISSET(ser, &readfds))
            {
                readSerial(ser);
            }
            if (FD_ISSET(ser, &writefds))
            {
                writeSerial(ser);
            }

            // read sockets
            for (int i = 0; i < MAX_CONNECTIONS; i++)
            {
                // if clients are valid and set
                if (clients[i] != -1 && FD_ISSET(clients[i], &readfds))
                {
                    readSocket(clients[i]);
                }

                // if clients are valid and set
                if (clients[i] != -1 && FD_ISSET(clients[i], &writefds))
                {
                    readSocket(clients[i]);
                }
            }

            // write writefds sockets
            writeSockets(clients, writefds, readfds);

            // new connection handler
            // if socket is ready, trying to connect, accept a connection
            if (FD_ISSET(sock, &readfds))
            {

                int new_client = accept(sock, NULL, NULL);
                if (new_client < 0)
                {
                    perror("accept failed\r\n");
                    continue;
                }

                set_nonblocking(new_client);
                bool added = false;

                // ADD to clients if its not taken
                for (int i = 0; i < MAX_CONNECTIONS; i++)
                {
                    if (clients[i] < 0)
                    { // -1
                        clients[i] = new_client;
                        added = true;
                        printf("Client %d connected  (fd=%d)\n", i, clients[i]);
                        break;
                    }
                }
                if (!added)
                {
                    fprintf(stderr, "Max connections reached, rejecting client\n");
                    close(new_client);
                }
            }

            // remove old connections
            for (int i = 0; i < MAX_CONNECTIONS; i++)
            {

                if (clients[i] != -1)
                {

                    char buf[1];
                    int result = recv(clients[i], buf, 1, MSG_PEEK | MSG_DONTWAIT);
                    if (result == 0)
                    {
                        printf("Client %d disconnected gracefully (fd=%d)\n", i, clients[i]);
                        close(clients[i]);
                        clients[i] = -1;
                        continue;
                    }

                    if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        printf("Client %d error: %s\n", i, strerror(errno));
                        close(clients[i]);
                        clients[i] = -1;
                        continue;
                    }
                }
            }
        }
    }
    return 0;
}
