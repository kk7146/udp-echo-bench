#include <stdlib.h>
#include <string.h>

#include "stats.h"

int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

long long percentile_sorted(const long long *arr, uint64_t n, double pct) {
    if (n == 0) return 0;
    if (pct <= 0.0) return arr[0];
    if (pct >= 100.0) return arr[n - 1];

    double pos = (pct / 100.0) * (double)(n - 1);
    uint64_t idx = (uint64_t)(pos + 0.5);
    if (idx >= n) idx = n - 1;
    return arr[idx];
}

void compute_rtt_stats(long long *rtts, uint64_t count, worker_result_t *result) {
    if (count == 0) {
        result->rtt_avg_ns = 0;
        result->rtt_min_ns = 0;
        result->rtt_max_ns = 0;
        result->rtt_p50_ns = 0;
        result->rtt_p95_ns = 0;
        result->rtt_p99_ns = 0;
        return;
    }

    qsort(rtts, (size_t)count, sizeof(long long), cmp_ll);

    long long sum = 0;
    for (uint64_t i = 0; i < count; i++) {
        sum += rtts[i];
    }

    result->rtt_avg_ns = sum / (long long)count;
    result->rtt_min_ns = rtts[0];
    result->rtt_max_ns = rtts[count - 1];
    result->rtt_p50_ns = percentile_sorted(rtts, count, 50.0);
    result->rtt_p95_ns = percentile_sorted(rtts, count, 95.0);
    result->rtt_p99_ns = percentile_sorted(rtts, count, 99.0);
}

overall_summary_t build_overall_summary(worker_file_data_t *workers_data, int workers) {
    overall_summary_t summary;
    memset(&summary, 0, sizeof(summary));

    for (int i = 0; i < workers; i++) {
        summary.total_sent += workers_data[i].meta.sent;
        summary.total_received += workers_data[i].meta.received;
        summary.total_mismatch += workers_data[i].meta.mismatch_count;
        summary.total_rtt_count += workers_data[i].meta.rtt_count;
    }

    summary.total_lost = (summary.total_sent >= summary.total_received)
        ? (summary.total_sent - summary.total_received)
        : 0;

    summary.total_loss_rate = (summary.total_sent > 0)
        ? 100.0 * (double)summary.total_lost / (double)summary.total_sent
        : 0.0;

    long long *all_rtts = NULL;
    if (summary.total_rtt_count > 0) {
        all_rtts = (long long *)malloc((size_t)summary.total_rtt_count * sizeof(long long));
        if (!all_rtts) {
            exit(EXIT_FAILURE);
        }
    }

    uint64_t offset = 0;
    for (int i = 0; i < workers; i++) {
        if (workers_data[i].meta.rtt_count > 0 && workers_data[i].rtts) {
            memcpy(all_rtts + offset,
                   workers_data[i].rtts,
                   (size_t)workers_data[i].meta.rtt_count * sizeof(long long));
            offset += workers_data[i].meta.rtt_count;
        }
    }

    if (summary.total_rtt_count > 0) {
        qsort(all_rtts, (size_t)summary.total_rtt_count, sizeof(long long), cmp_ll);

        long long sum = 0;
        for (uint64_t i = 0; i < summary.total_rtt_count; i++) {
            sum += all_rtts[i];
        }

        summary.rtt_avg_ns = sum / (long long)summary.total_rtt_count;
        summary.rtt_min_ns = all_rtts[0];
        summary.rtt_max_ns = all_rtts[summary.total_rtt_count - 1];
        summary.rtt_p50_ns = percentile_sorted(all_rtts, summary.total_rtt_count, 50.0);
        summary.rtt_p95_ns = percentile_sorted(all_rtts, summary.total_rtt_count, 95.0);
        summary.rtt_p99_ns = percentile_sorted(all_rtts, summary.total_rtt_count, 99.0);
    }

    free(all_rtts);
    return summary;
}