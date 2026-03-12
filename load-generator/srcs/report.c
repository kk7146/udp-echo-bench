#include <inttypes.h>
#include <stdio.h>

#include "report.h"

void print_worker_report(const worker_result_t *r) {
    uint64_t lost = (r->sent >= r->received) ? (r->sent - r->received) : 0;

    printf("[worker %d | src_port=%d]\n", r->worker_id, r->src_port);
    printf("  sent=%" PRIu64 " received=%" PRIu64 " lost=%" PRIu64
           " mismatch=%" PRIu64 " loss=%.4f%%\n",
           r->sent, r->received, lost, r->mismatch_count, r->loss_rate);

    if (r->rtt_count > 0) {
        printf("  RTT(ns): avg=%lld min=%lld max=%lld p50=%lld p95=%lld p99=%lld\n",
               r->rtt_avg_ns,
               r->rtt_min_ns,
               r->rtt_max_ns,
               r->rtt_p50_ns,
               r->rtt_p95_ns,
               r->rtt_p99_ns);
    } else {
        printf("  RTT(ns): no successful samples\n");
    }
}

void print_all_worker_reports(worker_file_data_t *workers_data, int workers) {
    printf("\n===== Per-Worker Report =====\n");
    for (int i = 0; i < workers; i++) {
        print_worker_report(&workers_data[i].meta);
    }
}

void print_overall_summary(const overall_summary_t *summary) {
    printf("\n===== Overall Summary =====\n");
    printf("sent=%" PRIu64 " received=%" PRIu64 " lost=%" PRIu64
           " mismatch=%" PRIu64 " loss=%.4f%%\n",
           summary->total_sent,
           summary->total_received,
           summary->total_lost,
           summary->total_mismatch,
           summary->total_loss_rate);

    if (summary->total_rtt_count > 0) {
        printf("RTT(ns): avg=%lld min=%lld max=%lld p50=%lld p95=%lld p99=%lld\n",
               summary->rtt_avg_ns,
               summary->rtt_min_ns,
               summary->rtt_max_ns,
               summary->rtt_p50_ns,
               summary->rtt_p95_ns,
               summary->rtt_p99_ns);
    } else {
        printf("RTT(ns): no successful samples\n");
    }
}