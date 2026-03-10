#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PAYLOAD_SIZE 1024
#define DEFAULT_WORKERS 4
#define DEFAULT_PPS 1000
#define DEFAULT_DURATION 10
#define DEFAULT_BASE_SRC_PORT 40000

static volatile sig_atomic_t stop_flag = 0;

static void on_signal(int signo) {
    (void)signo;
    stop_flag = 1;
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static long long now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        die("clock_gettime");
    }
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_until_ns(long long target_ns) {
    while (!stop_flag) {
        long long current = now_ns();
        if (current >= target_ns) {
            return;
        }

        long long remain = target_ns - current;
        struct timespec req;
        req.tv_sec = remain / 1000000000LL;
        req.tv_nsec = remain % 1000000000LL;

        if (nanosleep(&req, NULL) == 0) {
            return;
        }
        if (errno != EINTR) {
            die("nanosleep");
        }
    }
}

static void worker_loop(int worker_id, const char *server_ip, int server_port, int pps, int duration_sec, int payload_size, int base_src_port) {
    int sockfd;
    struct sockaddr_in local_addr, server_addr;
    char *send_buf = NULL;
    char *recv_buf = NULL;

    if (payload_size <= 0 || payload_size > 65507) {
        fprintf(stderr, "invalid payload size: %d\n", payload_size);
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        die("socket");
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(base_src_port + worker_id);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        die("bind");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "invalid server ip: %s\n", server_ip);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;  // 200ms receive timeout
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        die("setsockopt(SO_RCVTIMEO)");
    }

    send_buf = (char *)malloc((size_t)payload_size);
    recv_buf = (char *)malloc((size_t)payload_size);
    if (!send_buf || !recv_buf) {
        fprintf(stderr, "malloc failed\n");
        close(sockfd);
        free(send_buf);
        free(recv_buf);
        exit(EXIT_FAILURE);
    }

    memset(send_buf, 0, (size_t)payload_size);

    long long interval_ns = 1000000000LL / pps;
    long long start_ns = now_ns();
    long long end_ns = start_ns + (long long)duration_sec * 1000000000LL;
    long long next_send_ns = start_ns;

    uint64_t seq = 0;
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t timeout_count = 0;
    uint64_t mismatch_count = 0;

    while (!stop_flag) {
        long long current = now_ns();
        if (current >= end_ns) {
            break;
        }

        sleep_until_ns(next_send_ns);

        if (stop_flag) {
            break;
        }

        memset(send_buf, 'A' + (worker_id % 26), (size_t)payload_size);

        // 앞부분에 worker_id와 seq를 심어서 검증
        memcpy(send_buf, &worker_id, sizeof(worker_id));
        memcpy(send_buf + sizeof(worker_id), &seq, sizeof(seq));

        ssize_t s = sendto(
            sockfd,
            send_buf,
            (size_t)payload_size,
            0,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        );
        if (s < 0) {
            perror("sendto");
            break;
        }
        if (s != payload_size) {
            fprintf(stderr, "[worker %d] partial send: %zd\n", worker_id, s);
            break;
        }
        sent++;

        ssize_t r = recvfrom(sockfd, recv_buf, (size_t)payload_size, 0, NULL, NULL);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                timeout_count++;
            } else {
                perror("recvfrom");
                break;
            }
        } else {
            if (r == payload_size && memcmp(send_buf, recv_buf, (size_t)payload_size) == 0) {
                received++;
            } else {
                mismatch_count++;
            }
        }

        seq++;
        next_send_ns += interval_ns;
    }

    double loss_rate = 0.0;
    if (sent > 0) {
        loss_rate = 100.0 * (double)(sent - received) / (double)sent;
    }

    printf(
        "[worker %d | src_port=%d] sent=%llu received=%llu timeout=%llu mismatch=%llu loss=%.2f%%\n",
        worker_id,
        base_src_port + worker_id,
        (unsigned long long)sent,
        (unsigned long long)received,
        (unsigned long long)timeout_count,
        (unsigned long long)mismatch_count,
        loss_rate
    );
    fflush(stdout);

    close(sockfd);
    free(send_buf);
    free(recv_buf);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 8) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> [workers] [pps_per_worker] [duration_sec] [payload_size] [base_src_port]\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 8888 4 1000 10 1024 40000\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int workers = (argc >= 4) ? atoi(argv[3]) : DEFAULT_WORKERS;
    int pps = (argc >= 5) ? atoi(argv[4]) : DEFAULT_PPS;
    int duration_sec = (argc >= 6) ? atoi(argv[5]) : DEFAULT_DURATION;
    int payload_size = (argc >= 7) ? atoi(argv[6]) : DEFAULT_PAYLOAD_SIZE;
    int base_src_port = (argc >= 8) ? atoi(argv[7]) : DEFAULT_BASE_SRC_PORT;

    if (workers <= 0 || workers > 1024) {
        fprintf(stderr, "invalid workers: %d\n", workers);
        return EXIT_FAILURE;
    }
    if (pps <= 0) {
        fprintf(stderr, "invalid pps: %d\n", pps);
        return EXIT_FAILURE;
    }
    if (duration_sec <= 0) {
        fprintf(stderr, "invalid duration: %d\n", duration_sec);
        return EXIT_FAILURE;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("server=%s:%d workers=%d pps/worker=%d duration=%d payload=%d base_src_port=%d\n",
           server_ip, server_port, workers, pps, duration_sec, payload_size, base_src_port);

    for (int i = 0; i < workers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        }
        if (pid == 0) {
            worker_loop(i, server_ip, server_port, pps, duration_sec, payload_size, base_src_port);
        }
    }

    int status;
    while (wait(&status) > 0) {
        // wait for all children
    }

    return EXIT_SUCCESS;
}