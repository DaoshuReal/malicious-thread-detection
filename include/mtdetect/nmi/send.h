#pragma once

#include <ntifs.h>

/* Maps the local APIC. FALSE means sending stays off. */
BOOLEAN mtdetect_nmi_send_init(void);

/* One NMI to every cpu, including self. No-op when init failed. */
void mtdetect_nmi_send_all(void);
