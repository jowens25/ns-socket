#ifndef SOCKET_SERIAL_H

#define SOCKET_SERIAL_H

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>

#define SOCKET_PATH "/tmp/serial.sock"
// #define SERIAL_PORT "/dev/ttyUSB0"
#define MAX_CONNECTIONS 4

#define CHUNK_SIZE 8
#define SERIAL_PORT_LEN 128
extern struct termios tty;
// extern struct sockaddr_un addr;

extern int clients[MAX_CONNECTIONS];

extern char chunk[CHUNK_SIZE];
// extern int index;
extern int offset;
extern int head;
extern int tail;
extern char SERIAL_PORT[SERIAL_PORT_LEN];

int serRead(int ser, char data[], size_t dataLength);
int serWrite(int ser, char data[], size_t dataLength);

int serialSetup(int);

int readSerial(int ser);
void writeSerial(int ser);

void readSocket(int client);
// void writeSockets(int *clients, fd_set fds);
void writeSockets(int *clients, fd_set writefds, fd_set readfds);
void writeSocket(int client, char *tx, int bytes_read);

void removeClosedClients(void);

void addNewConnections(int sock);

void socketSetup(int sock);

void set_nonblocking(int fd);

#endif // SOCKET_SERIAL_H