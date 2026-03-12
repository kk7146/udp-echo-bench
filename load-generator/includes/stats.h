#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include "types.h"

int cmp_ll(const void *a, const void *b);
long long percentile_sorted(const long long *arr, uint64_t n, double pct);
void compute_rtt_stats(long long *rtts, uint64_t count, worker_result_t *result);
overall_summary_t build_overall_summary(worker_file_data_t *workers_data, int workers);

#endif