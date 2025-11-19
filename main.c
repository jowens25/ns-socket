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

struct termios tty;

struct sockaddr_un addr;

int clients[MAX_CONNECTIONS] = {-1, -1, -1, -1};

int max_fd;

int main()
{

    cb_init(&ser_cb);
    cb_init(&sock_cb);

    // Open serial port
    int ser = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (ser < 0)
    {
        perror("unable to open serial port");
        return -1;
    }
    serialSetup(ser);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("unable to create socket");
        close(sock);
        return -1;
    }
    socketSetup(sock);

    printf("Serving %s on %s\r\n", SERIAL_PORT, SOCKET_PATH);

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(ser, &readfds);
        max_fd = ser;

        FD_SET(sock, &readfds);
        if (sock > max_fd)
        {
            max_fd = sock;
        }

        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (clients[i] != -1)
            {
                FD_SET(clients[i], &readfds);
                if (clients[i] > max_fd)
                    max_fd = clients[i];
            }
        }

        struct timeval timeout = {1, 0};
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            perror("select error");
            break;
        }

        if (activity == 0)
        {
            continue;
        }

        if (FD_ISSET(sock, &readfds))
        {
            addNewConnections(sock);
        }

        removeClosedClients();

        if (FD_ISSET(ser, &readfds))
        {
            readSerial(ser);
        }

        writeSockets(&clients);

        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (clients[i] != -1 && FD_ISSET(clients[i], &readfds))

            {

                readSocket(clients[i]);
            }
        }
    }

    return 0;
}