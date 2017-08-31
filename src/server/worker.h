#ifndef WORKER_H
#define WORKER_H

int
respawn_worker();

void
reap_workers();

int
is_respawn_needed();

int
is_reaping_needed();

void
init_workers();

int
worker_fd_pass(int fd);

#endif
