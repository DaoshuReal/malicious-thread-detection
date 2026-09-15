#pragma once

#include <ntifs.h>

/* One slot per cpu. Filled in later once NMIs fire. */
typedef struct {
  ULONG processor_index;
  BOOLEAN captured;
  ULONG64 rip;
  ULONG64 rsp;
} MtdetectNmiSlot;

/* Allocates and zeroes the slots. */
void mtdetect_nmi_capture_init(void);

/* How many slots we got. 0 means alloc failed. */
ULONG mtdetect_nmi_capture_count(void);

/* NULL when out of range. */
MtdetectNmiSlot* mtdetect_nmi_capture_slot(ULONG index);
