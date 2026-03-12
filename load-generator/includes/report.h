#ifndef REPORT_H
#define REPORT_H

#include "types.h"

void print_worker_report(const worker_result_t *r);
void print_all_worker_reports(worker_file_data_t *workers_data, int workers);
void print_overall_summary(const overall_summary_t *summary);

#endif