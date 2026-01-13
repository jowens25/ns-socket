// gcc -o sample_native_app main.c

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

#include <linux/net_tstamp.h>

#define MAX_BUF_LEN 1024

// Global socket descriptor for cleanup in signal handler
static int g_server_socket = -1;

void cleanup_and_exit(int signum)
{
    printf("\nReceived signal %d, cleaning up...\n", signum);
    if (g_server_socket >= 0) {
        close(g_server_socket);
        g_server_socket = -1;
    }
    exit(0);
}

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


int main(int argc, char** argv) {
    int port = 55055;
    if( argc > 1 )
    {
        port = atoi(argv[1]);
    }

    int client_socket, len;
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
    g_server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_server_socket < 0) {
        perror("socket");
        return 1;
    }

    // Set SO_REUSEADDR to allow quick restart
    int reuse = 1;
    if (setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(g_server_socket);
        return 1;
    }

    // Set up the server address structure
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind the socket to the address
    if (bind(g_server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("bind");
        close(g_server_socket);
        return 1;
    }

    // Set timestamping options on the server socket
    int flags = SOF_TIMESTAMPING_RX_HARDWARE
                | SOF_TIMESTAMPING_RAW_HARDWARE
                | SOF_TIMESTAMPING_RX_SOFTWARE
                | SOF_TIMESTAMPING_SOFTWARE;
    if (setsockopt(g_server_socket, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags)) < 0) {
        perror("setsockopt SO_TIMESTAMPING");
        close(g_server_socket);
        return 1;
    }

    printf("UDP server is listening on port %d...\n", port);
    printf("Press Ctrl+C to exit cleanly\n");

    while (1) {
        // Receive data from any client
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recvmsg(g_server_socket, &msg, 0);
        
        if (bytes_received < 0) {
            perror("recvmsg");
            continue;  // Continue on error instead of exiting
        }

        scan_cb_buf(&msg);

        // Print the received data
        printf("Received %d bytes: %s\n", bytes_received, buffer);
    }

    // Cleanup (in case we break out of the loop somehow)
    close(g_server_socket);
    return 0;
}