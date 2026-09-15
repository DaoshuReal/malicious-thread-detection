#pragma once

#include <ntifs.h>

/* Snapshot the module list. Passive only, call at load. */
void mtdetect_mm_init(void);

/* Refresh the snapshot. Passive only, worker calls this. */
void mtdetect_mm_refresh(void);

/* True when addr sits in a known module. At most DISPATCH_LEVEL. */
BOOLEAN mtdetect_mm_known(PVOID addr);
