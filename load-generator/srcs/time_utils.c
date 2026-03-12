#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "time_utils.h"

volatile sig_atomic_t stop_flag = 0;

void on_signal(int signo) {
    (void)signo;
    stop_flag = 1;
}

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

long long now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        die("clock_gettime");
    }
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void sleep_until_ns(long long target_ns) {
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