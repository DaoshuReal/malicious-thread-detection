#pragma once

#include <ntifs.h>

/* Registers our callback. Does not send NMIs. */
void mtdetect_nmi_callback_init(void);

/* Expect this many of our own NMIs. Call before sending. */
void mtdetect_nmi_callback_arm(ULONG cpus);

/* How many of our NMIs the callback claimed. */
long mtdetect_nmi_callback_hits(void);
