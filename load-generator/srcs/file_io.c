#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "file_io.h"
#include "time_utils.h"

void ensure_log_dir(void) {
    struct stat st;
    if (stat(LOG_DIR, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "%s exists but is not a directory\n", LOG_DIR);
            exit(EXIT_FAILURE);
        }
        return;
    }

    if (mkdir(LOG_DIR, 0755) != 0) {
        die("mkdir log");
    }
}

void worker_log_path(int worker_id, char *buf, size_t buf_sz) {
    snprintf(buf, buf_sz, "%s/worker_%d.bin", LOG_DIR, worker_id);
}

void cleanup_old_logs(int workers) {
    char path[256];
    for (int i = 0; i < workers; i++) {
        worker_log_path(i, path, sizeof(path));
        unlink(path);
    }
}

void write_worker_file(const char *path, const worker_result_t *result, const long long *rtts) {
    FILE *fp = fopen(path, "wb");
    if (!fp) die("fopen worker log");

    if (fwrite(result, sizeof(*result), 1, fp) != 1) {
        fclose(fp);
        die("fwrite worker result");
    }

    if (result->rtt_count > 0) {
        if (fwrite(rtts, sizeof(long long), (size_t)result->rtt_count, fp) != result->rtt_count) {
            fclose(fp);
            die("fwrite worker rtts");
        }
    }

    fclose(fp);
}

worker_file_data_t read_worker_file(const char *path) {
    worker_file_data_t data;
    data.meta = (worker_result_t){0};
    data.rtts = NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) die("fopen read worker log");

    if (fread(&data.meta, sizeof(data.meta), 1, fp) != 1) {
        fclose(fp);
        die("fread worker result");
    }

    if (data.meta.rtt_count > 0) {
        data.rtts = (long long *)malloc((size_t)data.meta.rtt_count * sizeof(long long));
        if (!data.rtts) {
            fclose(fp);
            fprintf(stderr, "malloc worker rtts failed\n");
            exit(EXIT_FAILURE);
        }

        if (fread(data.rtts, sizeof(long long), (size_t)data.meta.rtt_count, fp) != data.meta.rtt_count) {
            fclose(fp);
            free(data.rtts);
            die("fread worker rtts");
        }
    }

    fclose(fp);
    return data;
}

void write_worker_summary_csv(worker_file_data_t *workers_data,
                              int workers,
                              const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        die("fopen worker_summary.csv");
    }

    fprintf(fp,
            "worker_id,src_port,sent,received,lost,mismatch,loss_rate,"
            "rtt_count,avg_rtt_ns,min_rtt_ns,max_rtt_ns,p50_rtt_ns,p95_rtt_ns,p99_rtt_ns\n");

    for (int i = 0; i < workers; i++) {
        worker_result_t *r = &workers_data[i].meta;
        uint64_t lost = (r->sent >= r->received) ? (r->sent - r->received) : 0;

        fprintf(fp,
                "%d,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.6f,"
                "%" PRIu64 ",%lld,%lld,%lld,%lld,%lld,%lld\n",
                r->worker_id,
                r->src_port,
                r->sent,
                r->received,
                lost,
                r->mismatch_count,
                r->loss_rate,
                r->rtt_count,
                r->rtt_avg_ns,
                r->rtt_min_ns,
                r->rtt_max_ns,
                r->rtt_p50_ns,
                r->rtt_p95_ns,
                r->rtt_p99_ns);
    }

    fclose(fp);
}

void write_overall_summary_csv(const overall_summary_t *summary,
                               const char *server_ip,
                               int server_port,
                               int workers,
                               int pps,
                               int duration_sec,
                               int payload_size,
                               int base_src_port,
                               int drain_ms,
                               const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        die("fopen summary.csv");
    }

    fprintf(fp,
            "server_ip,server_port,workers,pps_per_worker,duration_sec,payload_size,"
            "base_src_port,drain_ms,total_sent,total_received,total_lost,total_mismatch,"
            "loss_rate,total_rtt_count,avg_rtt_ns,min_rtt_ns,max_rtt_ns,p50_rtt_ns,p95_rtt_ns,p99_rtt_ns\n");

    fprintf(fp,
            "%s,%d,%d,%d,%d,%d,%d,%d,"
            "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.6f,"
            "%" PRIu64 ",%lld,%lld,%lld,%lld,%lld,%lld\n",
            server_ip,
            server_port,
            workers,
            pps,
            duration_sec,
            payload_size,
            base_src_port,
            drain_ms,
            summary->total_sent,
            summary->total_received,
            summary->total_lost,
            summary->total_mismatch,
            summary->total_loss_rate,
            summary->total_rtt_count,
            summary->rtt_avg_ns,
            summary->rtt_min_ns,
            summary->rtt_max_ns,
            summary->rtt_p50_ns,
            summary->rtt_p95_ns,
            summary->rtt_p99_ns);

    fclose(fp);
}