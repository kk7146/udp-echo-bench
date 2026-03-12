#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "config.h"
#include "file_io.h"
#include "stats.h"
#include "time_utils.h"
#include "types.h"
#include "worker.h"

void *sender_thread_main(void *arg) {
    worker_ctx_t *ctx = (worker_ctx_t *)arg;

    long long interval_ns = 1000000000LL / ctx->pps;
    long long start_ns = now_ns();
    long long end_ns = start_ns + (long long)ctx->duration_sec * 1000000000LL;
    long long next_send_ns = start_ns;
    uint64_t seq = 0;

    ctx->send_start_ns = start_ns;

    while (!stop_flag) {
        long long current = now_ns();
        if (current >= end_ns) {
            break;
        }

        sleep_until_ns(next_send_ns);
        if (stop_flag) {
            break;
        }

        memset(ctx->send_buf, 'A' + (ctx->worker_id % 26), (size_t)ctx->payload_size);

        packet_header_t hdr;
        hdr.magic = MAGIC;
        hdr.worker_id = ctx->worker_id;
        hdr.seq = seq;
        hdr.send_ts_ns = now_ns();

        memcpy(ctx->send_buf, &hdr, sizeof(hdr));

        ssize_t s = sendto(ctx->sockfd,
                           ctx->send_buf,
                           (size_t)ctx->payload_size,
                           0,
                           (struct sockaddr *)&ctx->server_addr,
                           sizeof(ctx->server_addr));
        if (s < 0) {
            perror("sendto");
            break;
        }
        if (s != ctx->payload_size) {
            fprintf(stderr, "[worker %d] partial send: %zd\n", ctx->worker_id, s);
            break;
        }

        pthread_mutex_lock(&ctx->lock);
        ctx->sent++;
        pthread_mutex_unlock(&ctx->lock);

        seq++;
        next_send_ns += interval_ns;
    }

    ctx->send_end_ns = now_ns();

    pthread_mutex_lock(&ctx->lock);
    ctx->sender_done = 1;
    pthread_mutex_unlock(&ctx->lock);

    return NULL;
}

void *receiver_thread_main(void *arg) {
    worker_ctx_t *ctx = (worker_ctx_t *)arg;
    long long last_send_end = 0;

    while (!stop_flag) {
        ssize_t r = recvfrom(ctx->sockfd,
                             ctx->recv_buf,
                             (size_t)ctx->payload_size,
                             0,
                             NULL,
                             NULL);

        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pthread_mutex_lock(&ctx->lock);
                int done = ctx->sender_done;
                last_send_end = ctx->send_end_ns;
                pthread_mutex_unlock(&ctx->lock);

                if (done) {
                    long long now = now_ns();
                    long long drain_limit = last_send_end + (long long)ctx->drain_ms * 1000000LL;
                    if (now >= drain_limit) {
                        break;
                    }
                }
                continue;
            } else if (errno == EINTR) {
                continue;
            } else {
                perror("recvfrom");
                break;
            }
        }

        if ((size_t)r < sizeof(packet_header_t)) {
            pthread_mutex_lock(&ctx->lock);
            ctx->mismatch_count++;
            pthread_mutex_unlock(&ctx->lock);
            continue;
        }

        packet_header_t hdr;
        memcpy(&hdr, ctx->recv_buf, sizeof(hdr));

        if (hdr.magic != MAGIC || hdr.worker_id != ctx->worker_id) {
            pthread_mutex_lock(&ctx->lock);
            ctx->mismatch_count++;
            pthread_mutex_unlock(&ctx->lock);
            continue;
        }

        long long recv_ts = now_ns();
        long long rtt = recv_ts - (long long)hdr.send_ts_ns;
        if (rtt < 0) {
            pthread_mutex_lock(&ctx->lock);
            ctx->mismatch_count++;
            pthread_mutex_unlock(&ctx->lock);
            continue;
        }

        pthread_mutex_lock(&ctx->lock);
        if (ctx->rtt_count < ctx->max_samples) {
            ctx->rtts[ctx->rtt_count] = rtt;
            ctx->rtt_count++;
        }
        ctx->received++;
        pthread_mutex_unlock(&ctx->lock);
    }

    return NULL;
}

void worker_run(int worker_id,
                const char *server_ip,
                int server_port,
                int pps,
                int duration_sec,
                int payload_size,
                int base_src_port,
                int drain_ms) {
    worker_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.worker_id = worker_id;
    ctx.pps = pps;
    ctx.duration_sec = duration_sec;
    ctx.payload_size = payload_size;
    ctx.drain_ms = drain_ms;
    ctx.max_samples = (uint64_t)pps * (uint64_t)duration_sec + 1024;

    pthread_mutex_init(&ctx.lock, NULL);

    if (payload_size <= 0 || payload_size > 65507) {
        fprintf(stderr, "invalid payload size: %d\n", payload_size);
        exit(EXIT_FAILURE);
    }

    if ((size_t)payload_size < sizeof(packet_header_t)) {
        fprintf(stderr, "payload_size must be >= %zu\n", sizeof(packet_header_t));
        exit(EXIT_FAILURE);
    }

    ctx.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx.sockfd < 0) {
        die("socket");
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons((uint16_t)(base_src_port + worker_id));

    if (bind(ctx.sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        die("bind");
    }

    memset(&ctx.server_addr, 0, sizeof(ctx.server_addr));
    ctx.server_addr.sin_family = AF_INET;
    ctx.server_addr.sin_port = htons((uint16_t)server_port);
    if (inet_pton(AF_INET, server_ip, &ctx.server_addr.sin_addr) != 1) {
        fprintf(stderr, "invalid server ip: %s\n", server_ip);
        close(ctx.sockfd);
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if (setsockopt(ctx.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        die("setsockopt(SO_RCVTIMEO)");
    }

    ctx.send_buf = (char *)malloc((size_t)payload_size);
    ctx.recv_buf = (char *)malloc((size_t)payload_size);
    ctx.rtts = (long long *)malloc((size_t)ctx.max_samples * sizeof(long long));
    if (!ctx.send_buf || !ctx.recv_buf || !ctx.rtts) {
        fprintf(stderr, "malloc failed\n");
        close(ctx.sockfd);
        free(ctx.send_buf);
        free(ctx.recv_buf);
        free(ctx.rtts);
        exit(EXIT_FAILURE);
    }

    pthread_t sender_thread;
    pthread_t receiver_thread;

    if (pthread_create(&receiver_thread, NULL, receiver_thread_main, &ctx) != 0) {
        die("pthread_create receiver");
    }
    if (pthread_create(&sender_thread, NULL, sender_thread_main, &ctx) != 0) {
        die("pthread_create sender");
    }

    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    worker_result_t result;
    memset(&result, 0, sizeof(result));
    result.worker_id = worker_id;
    result.src_port = base_src_port + worker_id;

    pthread_mutex_lock(&ctx.lock);
    result.sent = ctx.sent;
    result.received = ctx.received;
    result.mismatch_count = ctx.mismatch_count;
    result.rtt_count = ctx.rtt_count;
    pthread_mutex_unlock(&ctx.lock);

    result.loss_rate = (result.sent > 0)
        ? 100.0 * (double)(result.sent - result.received) / (double)result.sent
        : 0.0;

    compute_rtt_stats(ctx.rtts, result.rtt_count, &result);

    char log_path[256];
    worker_log_path(worker_id, log_path, sizeof(log_path));
    write_worker_file(log_path, &result, ctx.rtts);

    pthread_mutex_destroy(&ctx.lock);
    close(ctx.sockfd);
    free(ctx.send_buf);
    free(ctx.recv_buf);
    free(ctx.rtts);
    exit(EXIT_SUCCESS);
}