#pragma once

#include <ntifs.h>

/* Pairs read per NMI, bounds the RDMSR cost up here. */
#define MTDETECT_LBR_DEPTH 4

/* Probe MSRs once. Passive only, call at load after cpu init. */
void mtdetect_lbr_init(void);

/* True when the probe passed and NMI capture fills slots. */
BOOLEAN mtdetect_lbr_ready(void);

/* Flavor id when ready, -1 when dormant. Any IRQL, cached. */
int mtdetect_lbr_status(void);

/* Raw gated reads for NMI context. No locks, prints, or faults. */
void mtdetect_lbr_capture(PUCHAR tos, PVOID* from, PVOID* to);

/* LSTAR at load, 0 when unreadable. Any IRQL, cached value. */
ULONG64 mtdetect_lbr_lstar_baseline(void);

/* Current LSTAR. Passive only, FALSE on fault or wrong IRQL. */
BOOLEAN mtdetect_lbr_lstar_now(ULONG64* value);
