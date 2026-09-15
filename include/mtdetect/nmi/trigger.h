#pragma once

/* Fires every few seconds and prints state. No NMI send yet. */
void mtdetect_nmi_trigger_init(void);

/* Ticks so far. */
long mtdetect_nmi_trigger_ticks(void);
