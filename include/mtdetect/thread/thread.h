#pragma once

#include <ntifs.h>

enum mtdetect_thread_verdict_e {
  MTDETECT_THREAD_UNKNOWN = 0,
  MTDETECT_THREAD_OK = 1,
  MTDETECT_THREAD_SPOOFED = 2
};

/* Learns the start-address offsets. Passive only, call at load. */
void mtdetect_thread_init(void);

/* Start routine of a thread. Any IRQL, NULL when unknown. */
PVOID mtdetect_thread_start(PETHREAD thread);

/* Both start twins. Any IRQL, each NULL when unknown. */
void mtdetect_thread_starts(PETHREAD thread, PVOID* first, PVOID* second);

/* True when twins disagree. Any IRQL, false when twin unknown. */
BOOLEAN mtdetect_thread_twins_mismatch(PVOID first, PVOID second);

/* Compares live values to creation baseline. At most DISPATCH_LEVEL. */
enum mtdetect_thread_verdict_e mtdetect_thread_check(HANDLE tid, PVOID current, PVOID current2);
