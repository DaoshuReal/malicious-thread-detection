#pragma once

#include <ntifs.h>

/* Starts the passive verifier. Passive only, call at load. */
void mtdetect_worker_init(void);

/* Queues a suspect for passive recheck. At most DISPATCH_LEVEL. */
void mtdetect_worker_queue(HANDLE tid, PVOID start, PVOID start2);

/* Queues an LBR pair for post-refresh recheck. At most DISPATCH_LEVEL. */
void mtdetect_worker_queue_lbr(HANDLE tid, PVOID from, PVOID to, ULONG cpu);
