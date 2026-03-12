#ifndef WORKER_H
#define WORKER_H

void *sender_thread_main(void *arg);
void *receiver_thread_main(void *arg);

void worker_run(int worker_id,
                const char *server_ip,
                int server_port,
                int pps,
                int duration_sec,
                int payload_size,
                int base_src_port,
                int drain_ms);

#endif