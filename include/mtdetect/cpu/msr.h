#pragma once

#include <ntifs.h>

/* Raw rdmsr. No checks. */
ULONG64 mtdetect_read_msr(ULONG msr);

/* Raw wrmsr. No checks. */
void mtdetect_write_msr(ULONG msr, ULONG64 value);
