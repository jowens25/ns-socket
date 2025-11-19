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
#define SERIAL_PORT "/dev/ttyUSB0"
#define MAX_CONNECTIONS 4

#define CHUNK_SIZE 8

extern struct termios tty;
extern struct sockaddr_un addr;

extern int clients[MAX_CONNECTIONS];

extern char chunk[CHUNK_SIZE];
// extern int index;
extern int offset;
extern int head;
extern int tail;

int serRead(int ser, char data[], size_t dataLength);
int serWrite(int ser, char data[], size_t dataLength);

int serialSetup(int);

void readSerial(int ser);
void writeSerial(int ser);

void readSocket(int client);
void writeSockets(int *client);

void removeClosedClients(void);

void addNewConnections(int sock);

void socketSetup(int sock);

#endif // SOCKET_SERIAL_H