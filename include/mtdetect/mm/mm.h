#pragma once

#include <ntifs.h>

/* Snapshot the module list. Passive only, call at load. */
void mtdetect_mm_init(void);

/* True when addr sits in a known module. Any IRQL. */
BOOLEAN mtdetect_mm_known(PVOID addr);
