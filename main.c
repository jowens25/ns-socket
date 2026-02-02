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
//

#define SOCKET_PATH "/tmp/serial.sock"

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define SERIAL_PORT_PARM "SERIAL_PORT"

struct termios tty;
//int clients[MAX_CONNECTIONS] = {-1, -1, -1, -1};
//int max_fd;
int debug = false;
	
#define DEF_BUFSIZE 512 




char SERIAL_PORT[SERIAL_PORT_LEN] = {0};

int getrval(char *sstr, char *rval)
{
	int retVal = 0; // Error 
	char *eq;
	char *q;
	if (sstr && rval)
	{
		if ((eq = strchr(sstr, '=')) != NULL)
		{
			strcpy(rval, eq+1);
			ltrim(rval);
			rtrim(rval);

			// Strip any quotes

			if (rval[0] == '\"')
			{
				strcpy(&rval[0], &rval[1]);
				if ((q = strchr(rval, '\"')) != NULL)
					*q = 0;
			}
				
			retVal = 1;
		}
	}

	return retVal;
}	

char* rtrim(char* string)
{
    int i = strlen(string) - 1;

    while (i)
    { 
       if (string[i] == ' ')
       {
            string[i] = 0;
            i--;
       } else {
            break;
       } 
    }

    return string;
}


int getlval(char *sstr, char *lval)
{
	int retVal = 0; // Error 
	char *eq;

	if (sstr && lval)
	{
		if ((eq = strchr(sstr, '=')) != NULL)
		{
			memcpy(lval, sstr, eq - sstr);
			lval[eq - sstr] = 0;

			ltrim(lval);
			rtrim(lval);
			retVal = 1;
		}
	}

	return retVal;
}

char* ltrim(char *string)
{
    while (string[0] == ' ')
    	strcpy(&string[0], &string[1]);

    return string;
}




void read_serial_port_from_conf(void) {

    FILE *fp = NULL;
    char aline[DEF_BUFSIZE];
    size_t alen = DEF_BUFSIZE - 1;
    char *aptr = aline;
    char rval[512], lval[512];


    if ((fp = fopen(CONFIG_FILE, "rb")) != NULL) {

        while((getline(&aptr, &alen, fp)) != -1) {

            ltrim(aline);
			rtrim(aline);




            if (aline[0] == '#' || aline[0] == 0)
			{
				// Ignore comment lines
			} else {
				if (getlval(aline, lval) && getrval(aline, rval))
				{
					if (strcasecmp(lval, SERIAL_PORT_PARM) == 0)
					{
						strcpy(SERIAL_PORT, rval);
			        } 
				
				}
			}



        }

    }

}






void handle_new_connection(int listener, fd_set *master, int *fdmax)
{
	int newfd;        // newly accept()ed socket descriptor
    newfd = accept(listener, NULL, NULL);

    set_nonblocking(newfd);

	if (newfd == -1) {
		perror("accept");
	} else {
		FD_SET(newfd, master); // add to master set
		if (newfd > *fdmax) {  // keep track of the max
			*fdmax = newfd;
		}
		printf("selectserver: new connection on socket %d\n", newfd);
	}
}

// socket is ready, read from it, write to serial and other sockets
void handle_socket(int client, int serial, int listener, fd_set *master, int fdmax)
{
    char socket_buf[CHUNK_SIZE];
    int n;
    if ((n = read(client, socket_buf, CHUNK_SIZE)) <= 0) {
        if (n == 0) {
            // connection closed
            printf("select server: socket %d hung up\n", client);
        } else {
            perror("read in readSocket");
        }
        printf("closing: %d\n", client);
        close(client);
        FD_CLR(client, master);
    }

    else {
        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, master)) {
                if (i != client && i != listener){
                    write(i, socket_buf, n);
                }
            }
        }
    }
}

// serial port is ready, read from it, write to sockets.
void handle_serial(int client, int serial, int listener, fd_set *master, int fdmax)
{
    char serial_buf[CHUNK_SIZE];
    int n = read(serial, serial_buf, CHUNK_SIZE);
    //printf("n: %d\n", n);

    //printf("%s\n", serial_buf);

    if (n > 0)
    {
        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, master)) {
                if (i != listener && i != serial){
                    //printf("writing to: %d\n", i);
                    write(i, serial_buf, n);
                }
            }
        }
    }

}



int main(int argc, char *argv[]) 
{

    read_serial_port_from_conf();

    if (argv[2] != NULL) {

        if ( strncmp(argv[2], "debug", 5) == 0) {
            debug = true;
            printf("DEBUGGING\r\n");
        }

    }


    fd_set read_fds;
    fd_set master;
    FD_ZERO(&read_fds);
    FD_ZERO(&master);
    int fdmax;
    int listener_fd;
    int serial_fd;



    //strncpy(SERIAL_PORT, argv[1], strlen(argv[1]));
    printf("Using serial port: %s\n", SERIAL_PORT);
    int ser = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (ser < 0)
    {
        perror("unable to open serial port");
        return -1;
    }

    serial_fd = serialSetup(ser);

    set_nonblocking(serial_fd);
    FD_SET(serial_fd, &master);


    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("unable to create socket");
        close(sock);
        return -1;
    }
    listener_fd = socketSetup(sock);
    set_nonblocking(listener_fd);
    FD_SET(listener_fd, &master);


    fdmax = listener_fd > serial_fd ? listener_fd : serial_fd;



    while (1)
    {


        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100; // 100ms


        read_fds = master; // copy it
		if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
			perror("select");
			exit(4);
		}


		// run through the existing connections looking for data
		// to read
		for(int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == listener_fd) { // new connection available
					handle_new_connection(i, &master, &fdmax);
                }
                
                else if (i == serial_fd) { // serial ready!
                    handle_serial(i, serial_fd, listener_fd, &master, fdmax);
                }
				
                else { // a socket file descriptor is ready!
					handle_socket(i, serial_fd, listener_fd, &master, fdmax);
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


