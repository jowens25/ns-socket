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
int debug = false;

char SERIAL_PORT[SERIAL_PORT_LEN] = {0};

int main(int argc, char *argv[])
{

    if (argv[1] == NULL)
    {
        printf("Please include a serial port\r\n");
        printf("example: %s /dev/ttymxc2\r\n", argv[0]);
        exit(-1);
    }

    if (argv[2] != NULL) {

        if ( strncmp(argv[2], "debug", 5) == 0) {
            debug = true;
            printf("DEBUGGING\r\n");
        }

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
        perror("serial setup error");
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

    char tx[BUFFER_SIZE];
    int bytes_read =0;

    while (1)
    {

        // collect active file descriptors
        max_fd = 0;

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        // check serial for reading
        FD_SET(ser, &readfds);
        max_fd = MAX(ser, max_fd);


        // removed because serial is non blocking and almost alwyas writeable???
        // only check serial for writing if we have something to write
        //if (!cb_is_empty(&sock_cb)) {

        //    FD_SET(ser, &writefds);
        //}



        // check socket for new connections
        FD_SET(sock, &readfds);
        max_fd = MAX(sock, max_fd);


        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (clients[i] != -1)
            {
                //printf("adding clients\r\n");
                // if a client is valid check for reading
                FD_SET(clients[i], &readfds);
                max_fd = MAX(clients[i], max_fd);

                //if (!cb_is_empty(&ser_cb)){
                //    FD_SET(clients[i], &writefds);
                //
                //}



            }
        }

        struct timeval timeout = {0, 100000};
        //int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (debug){
            printf("act: %d\r\n", activity);
        }

        if (activity < 0)
        {
            perror("select error\r\n");
            break;
        }

        else if (activity == 0)
        {   
            if (debug) {
                perror("timeout\r\n");
            }
            continue;
        }

        else
        {

            

            // serial handlers
            if (FD_ISSET(ser, &readfds))
            {
                readSerial(ser);
            }

            if (!cb_is_empty(&sock_cb))
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
            }

            // if data available write sockets

            if (!cb_is_empty(&ser_cb)) {

                bytes_read = cb_read_chunk(&ser_cb, tx, BUFFER_SIZE);

                if (bytes_read > 0) {
                    // Write to all writable clients
                    for (int i = 0; i < MAX_CONNECTIONS; i++) {
                        if (clients[i] != -1 ) {
                            writeSocket(clients[i], tx, bytes_read);
                        }
                    }
                }
            }
        


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


    close(ser);
    close(sock);
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (clients[i] != -1)
            close(clients[i]);
    }


    return 0;
}
