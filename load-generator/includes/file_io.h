#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>
#include "types.h"

void ensure_log_dir(void);
void worker_log_path(int worker_id, char *buf, size_t buf_sz);
void cleanup_old_logs(int workers);

void write_worker_file(const char *path, const worker_result_t *result, const long long *rtts);
worker_file_data_t read_worker_file(const char *path);

void write_worker_summary_csv(worker_file_data_t *workers_data, int workers, const char *path);
void write_overall_summary_csv(const overall_summary_t *summary,
                               const char *server_ip,
                               int server_port,
                               int workers,
                               int pps,
                               int duration_sec,
                               int payload_size,
                               int base_src_port,
                               int drain_ms,
                               const char *path);

#endif