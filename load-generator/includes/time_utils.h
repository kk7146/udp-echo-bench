#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <signal.h>

extern volatile sig_atomic_t stop_flag;

void on_signal(int signo);
void die(const char *msg);
long long now_ns(void);
void sleep_until_ns(long long target_ns);

#endif