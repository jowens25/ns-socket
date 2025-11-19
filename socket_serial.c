
#include "socket_serial.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "circular_buffer.h"
#include <errno.h>

char chunk[CHUNK_SIZE] = {0};
char socket_chunk[CHUNK_SIZE] = {0};

void readSerial(int ser)
{

    int n = read(ser, chunk, CHUNK_SIZE);

    if (n > 0)
    {

        cb_write_chunk(&ser_cb, chunk, n);
    }
}

// should combine both socket connections in to the same cir buf
void readSocket(int client)
{

    int n = read(client, socket_chunk, CHUNK_SIZE);
    if (n > 0)
    {

        cb_write_chunk(&sock_cb, socket_chunk, n);
    }
}

void writeSerial(int ser)
{

    char tx[128];
    int bytes_read = cb_read_chunk(&sock_cb, tx, 128);

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

void writeSockets(int *clients)
{

    char tx[128];
    int bytes_read = cb_read_chunk(&ser_cb, tx, 128);

    if (bytes_read == 0)
    {
        return;
    }

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (clients[i] == -1)
            continue;

        int n = write(clients[i], tx, bytes_read);

        if (n < 0)
        {
            if (errno == EPIPE || errno == ECONNRESET || errno == EBADF)
            {
                printf("Client %d disconnected\n", i);
                close(clients[i]);
                clients[i] = -1;
            }
        }
    }
}

void removeClosedClients(void)
{
    char buf[1];
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (clients[i] == -1) // Skip invalid clients - THIS IS CRITICAL
            continue;

        int result = recv(clients[i], buf, 1, MSG_PEEK | MSG_DONTWAIT);

        if (result == 0)
        {
            printf("Client %d disconnected gracefully (fd=%d)\n", i, clients[i]);
            close(clients[i]);
            clients[i] = -1;
        }
        else if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            printf("Client %d error: %s\n", i, strerror(errno));
            close(clients[i]);
            clients[i] = -1;
        }
    }
}

void addNewConnections(int sock)
{

    int new_client = accept(sock, NULL, NULL);
    if (new_client == -1)
    {
        perror("unable to accept");
    }
    else
    {
        // Find empty slot for new client
        int added = 0;
        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (clients[i] == -1)
            {
                clients[i] = new_client;
                printf("Client %d connected (fd=%d)\n", i, new_client);
                added = 1;
                break;
            }
        }
        if (!added)
        {
            printf("Max clients reached, rejecting connection\n");
            close(new_client);
        }
    }
}

void socketSetup(int sock)
{

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("unable to bind?");
        close(sock);
        exit(-1);
    }

    if (listen(sock, MAX_CONNECTIONS) == -1)
    {
        perror("unable to listen");
        close(sock);
        exit(-1);
    }
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
