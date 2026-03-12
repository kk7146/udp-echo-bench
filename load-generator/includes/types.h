#ifndef TYPES_H
#define TYPES_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    uint32_t magic;
    int32_t worker_id;
    uint64_t seq;
    int64_t send_ts_ns;
} __attribute__((packed)) packet_header_t;

typedef struct {
    int worker_id;
    int src_port;
    uint64_t sent;
    uint64_t received;
    uint64_t mismatch_count;
    uint64_t rtt_count;
    double loss_rate;
    long long rtt_avg_ns;
    long long rtt_min_ns;
    long long rtt_max_ns;
    long long rtt_p50_ns;
    long long rtt_p95_ns;
    long long rtt_p99_ns;
} worker_result_t;

typedef struct {
    worker_result_t meta;
    long long *rtts;
} worker_file_data_t;

typedef struct {
    int worker_id;
    int sockfd;
    struct sockaddr_in server_addr;
    int pps;
    int duration_sec;
    int payload_size;
    int drain_ms;

    char *send_buf;
    char *recv_buf;

    long long *rtts;
    uint64_t max_samples;

    pthread_mutex_t lock;

    uint64_t sent;
    uint64_t received;
    uint64_t mismatch_count;
    uint64_t rtt_count;

    long long send_start_ns;
    long long send_end_ns;

    int sender_done;
} worker_ctx_t;

typedef struct {
    uint64_t total_sent;
    uint64_t total_received;
    uint64_t total_lost;
    uint64_t total_mismatch;
    uint64_t total_rtt_count;
    double total_loss_rate;
    long long rtt_avg_ns;
    long long rtt_min_ns;
    long long rtt_max_ns;
    long long rtt_p50_ns;
    long long rtt_p95_ns;
    long long rtt_p99_ns;
} overall_summary_t;

#endif