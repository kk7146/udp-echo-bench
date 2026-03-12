#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "file_io.h"
#include "report.h"
#include "stats.h"
#include "time_utils.h"
#include "types.h"
#include "worker.h"

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 9) {
        fprintf(stderr,
                "Usage: %s <server_ip> <server_port> [workers] [pps_per_worker] [duration_sec] [payload_size] [base_src_port] [drain_ms]\n",
                argv[0]);
        fprintf(stderr,
                "Example: %s 127.0.0.1 8888 4 1000 10 1024 40000 1000\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int workers = (argc >= 4) ? atoi(argv[3]) : DEFAULT_WORKERS;
    int pps = (argc >= 5) ? atoi(argv[4]) : DEFAULT_PPS;
    int duration_sec = (argc >= 6) ? atoi(argv[5]) : DEFAULT_DURATION;
    int payload_size = (argc >= 7) ? atoi(argv[6]) : DEFAULT_PAYLOAD_SIZE;
    int base_src_port = (argc >= 8) ? atoi(argv[7]) : DEFAULT_BASE_SRC_PORT;
    int drain_ms = (argc >= 9) ? atoi(argv[8]) : DEFAULT_DRAIN_MS;

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
    if (payload_size <= 0 || payload_size > 65507) {
        fprintf(stderr, "invalid payload size: %d\n", payload_size);
        return EXIT_FAILURE;
    }
    if (drain_ms < 0) {
        fprintf(stderr, "invalid drain_ms: %d\n", drain_ms);
        return EXIT_FAILURE;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    ensure_log_dir();
    cleanup_old_logs(workers);

    printf("server=%s:%d workers=%d pps/worker=%d duration=%d payload=%d base_src_port=%d drain_ms=%d\n",
           server_ip, server_port, workers, pps, duration_sec, payload_size, base_src_port, drain_ms);

    for (int i = 0; i < workers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        }
        if (pid == 0) {
            worker_run(i, server_ip, server_port, pps, duration_sec, payload_size, base_src_port, drain_ms);
        }
    }

    int status;
    while (wait(&status) > 0) {
    }

    worker_file_data_t *workers_data =
        (worker_file_data_t *)calloc((size_t)workers, sizeof(worker_file_data_t));
    if (!workers_data) {
        fprintf(stderr, "calloc workers_data failed\n");
        return EXIT_FAILURE;
    }

    char path[256];
    for (int i = 0; i < workers; i++) {
        worker_log_path(i, path, sizeof(path));
        workers_data[i] = read_worker_file(path);
    }

    overall_summary_t summary = build_overall_summary(workers_data, workers);

    print_all_worker_reports(workers_data, workers);
    print_overall_summary(&summary);

    char worker_csv_path[256];
    char summary_csv_path[256];
    snprintf(worker_csv_path, sizeof(worker_csv_path), "%s/worker_summary.csv", LOG_DIR);
    snprintf(summary_csv_path, sizeof(summary_csv_path), "%s/summary.csv", LOG_DIR);

    write_worker_summary_csv(workers_data, workers, worker_csv_path);
    write_overall_summary_csv(&summary,
                              server_ip,
                              server_port,
                              workers,
                              pps,
                              duration_sec,
                              payload_size,
                              base_src_port,
                              drain_ms,
                              summary_csv_path);

    for (int i = 0; i < workers; i++) {
        free(workers_data[i].rtts);
    }
    free(workers_data);

    return EXIT_SUCCESS;
}