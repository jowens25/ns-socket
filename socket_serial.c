
#include "socket_serial.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
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



int socketSetup(int sock)
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

    return sock;
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
    tty.c_cc[VMIN] = 0;   // wait for some chars
    tty.c_cc[VTIME] = 0; // Timeout in deciseconds (1.5 seconds)

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        printf("tcsetattr error? \n");
        perror("tcsetattr");

        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    return fd;
}

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}