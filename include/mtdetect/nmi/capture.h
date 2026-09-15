#pragma once

#include <ntifs.h>

/* One slot per cpu. NMI writes tid/start, tick reads them. */
typedef struct {
  ULONG processor_index;
  volatile BOOLEAN captured;
  volatile HANDLE tid;
  volatile PVOID start;
  volatile PVOID start2;
  BOOLEAN reported;
  PVOID reported_start;
  BOOLEAN spoof_reported;
  HANDLE spoof_tid;
  PVOID spoof_start;
  PVOID spoof_start2;
  ULONG64 rip;
  ULONG64 rsp;
} MtdetectNmiSlot;

/* Allocates and zeroes the slots. */
void mtdetect_nmi_capture_init(void);

/* How many slots we got. 0 means alloc failed. */
ULONG mtdetect_nmi_capture_count(void);

/* NULL when out of range. */
MtdetectNmiSlot* mtdetect_nmi_capture_slot(ULONG index);
