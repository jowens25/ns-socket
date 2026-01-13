// This is a small no-deps example to do HW-timestamps.
// gcc -o sample_native_app main.c

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <linux/net_tstamp.h>
#include <signal.h>

#define MAX_BUF_LEN 1024


int server_socket, client_socket, len;


void scan_cb_buf(struct msghdr * msg)
{
    struct cmsghdr * cmsg;
    printf("scan_cb_buf...\n");

    cmsg = CMSG_FIRSTHDR(msg);
    if(!cmsg)
        printf("INITIAL CMSG_FIRSTHDR(msg) == NULL\n");

    for (; cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
        printf("!\n");
        if (cmsg->cmsg_level != SOL_SOCKET)
            continue;

        printf("!!\n");

        switch (cmsg->cmsg_type) {
            case SO_TIMESTAMPNS:
            case SO_TIMESTAMPING: {
                struct timespec *stamp =
                   (struct timespec *)CMSG_DATA(cmsg);
                printf("ts[0] %ld.%06ld\n",
                    (long)stamp[0].tv_sec, (long)stamp[0].tv_nsec);
                printf("ts[1] %ld.%06ld\n",
                    (long)stamp[1].tv_sec, (long)stamp[1].tv_nsec);
                printf("ts[2] %ld.%06ld\n",
                    (long)stamp[2].tv_sec, (long)stamp[2].tv_nsec);
            }
                break;
            default:
                printf("???\n");
                break;
        }
    }

    {
        struct timespec tp;


        clock_gettime(CLOCK_REALTIME, &tp);
        printf("CLOCK_REALTIME ts %ld.%06ld\n", (long)tp.tv_sec, (long)tp.tv_nsec);

        clock_gettime(CLOCK_MONOTONIC, &tp);
        printf("CLOCK_MONOTONIC ts %ld.%06ld\n", (long)tp.tv_sec, (long)tp.tv_nsec);

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
        printf("CLOCK_PROCESS_CPUTIME_ID ts %ld.%06ld\n", (long)tp.tv_sec, (long)tp.tv_nsec);

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        printf("CLOCK_THREAD_CPUTIME_ID ts %ld.%06ld\n", (long)tp.tv_sec, (long)tp.tv_nsec);
    }
}


void cleanup_and_exit(int signum)
{
    printf("\nReceived signal %d, cleaning up...\n", signum);
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    exit(0);
}

int main(int argc, char** argv) {
    int port = 55055;
    if( argc > 1 )
    {
        port = atoi(argv[1]);
    }

    struct sockaddr_in server_address, client_address;
    char buffer[MAX_BUF_LEN];
    char control_block_buf[MAX_BUF_LEN];
    struct msghdr msg;
    struct iovec iovec_storage;

        // Set up signal handlers
    signal(SIGINT, cleanup_and_exit);   // Ctrl+C
    signal(SIGTERM, cleanup_and_exit);  // kill command
    signal(SIGHUP, cleanup_and_exit);   // Terminal hangup

    iovec_storage.iov_base = buffer;
    iovec_storage.iov_len = MAX_BUF_LEN;

    msg.msg_iov = &iovec_storage;
    msg.msg_iovlen = 1;
    msg.msg_control = control_block_buf;
    msg.msg_controllen = MAX_BUF_LEN;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    // Set up the server address structure
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("bind");
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0) {
        perror("listen");
        return 1;
    }

    printf("TCP server is listening on port %d...\n", port);

    while (1) {
        len = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr*) &client_address, (socklen_t*) &len);
        if (client_socket < 0) {
            perror("accept");
            return 1;
        }

        {
            // int flags = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
            int flags = SOF_TIMESTAMPING_RX_HARDWARE
                        | SOF_TIMESTAMPING_RAW_HARDWARE
                        | SOF_TIMESTAMPING_RX_SOFTWARE
                        | SOF_TIMESTAMPING_SOFTWARE;
            if (setsockopt(client_socket, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags)) < 0) {
                perror("setsockopt");
                return 1;
            }
        }

        // Receive data from the client
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recvmsg(client_socket, &msg, 0);
        scan_cb_buf(&msg);
        // int bytes_received = recv(client_socket, buffer, MAX_BUF_LEN, 0);
        if (bytes_received < 0) {
            perror("recv");
            return 1;
        }

        // Print the received data
        printf("Received: %s\n", buffer);

        // Close the connection
        close(client_socket);
    }

    close(server_socket);

    return 0;
}