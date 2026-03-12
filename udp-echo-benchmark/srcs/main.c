#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 8888
#define BUFFER_SIZE 1024

static void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error_exit("socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        error_exit("bind");
    }

    printf("UDP server listening on port %d...\n", port);

    while (1) {
        client_addr_len = sizeof(client_addr);

        ssize_t received_len = recvfrom(
            sockfd,
            buffer,
            BUFFER_SIZE,
            0,
            (struct sockaddr *)&client_addr,
            &client_addr_len
        );
        if (received_len < 0) {
            perror("recvfrom");
            break;
        }

        ssize_t sent_len = sendto(
            sockfd,
            buffer,
            (size_t)received_len,
            0,
            (struct sockaddr *)&client_addr,
            client_addr_len
        );
        if (sent_len < 0) {
            perror("sendto");
            break;
        }
    }

    close(sockfd);
    return 0;
}