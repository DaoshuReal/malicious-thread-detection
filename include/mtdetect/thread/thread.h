#pragma once

#include <ntifs.h>

/* Learns the start-address offset. Passive only, call at load. */
void mtdetect_thread_init(void);

/* Start routine of a thread. Any IRQL, NULL when unknown. */
PVOID mtdetect_thread_start(PETHREAD thread);
