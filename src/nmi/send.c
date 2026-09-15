#include "mtdetect/nmi/send.h"

#include "mtdetect/cpu/msr.h"

#define MTDETECT_APIC_BASE_MSR 0x1b
#define MTDETECT_X2APIC_ICR_MSR 0x830
#define MTDETECT_APIC_ICR_LOW 0x300
#define MTDETECT_APIC_ICR_HIGH 0x310

/* Delivery NMI + shorthand all-including-self. */
#define MTDETECT_ICR_NMI_ALL 0x80400u

static PUCHAR g_apic = NULL;
static BOOLEAN g_x2apic = FALSE;
static BOOLEAN g_ready = FALSE;

BOOLEAN mtdetect_nmi_send_init(void)
{
  unsigned __int64 base = 0;
  PHYSICAL_ADDRESS pa;

  pa.QuadPart = 0;
  base = mtdetect_read_msr(MTDETECT_APIC_BASE_MSR);

  if ((base & (1ULL << 11)) == 0)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] nmi send: apic off\n");

    return FALSE;
  }

  if ((base & (1ULL << 10)) != 0)
  {
    g_x2apic = TRUE;
    g_ready = TRUE;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] nmi send ready (x2apic)\n");

    return TRUE;
  }

  pa.QuadPart = (LONGLONG)(base & 0xfffff000ULL);
  g_apic = (PUCHAR)MmMapIoSpace(pa, PAGE_SIZE, MmNonCached);

  if (!g_apic)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] nmi send: map failed\n");

    return FALSE;
  }

  g_ready = TRUE;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] nmi send ready (xapic)\n");

  return TRUE;
}

void mtdetect_nmi_send_all(void)
{
  if (!g_ready)
  {
    return;
  }

  if (g_x2apic)
  {
    mtdetect_write_msr(MTDETECT_X2APIC_ICR_MSR, MTDETECT_ICR_NMI_ALL);

    return;
  }

  WRITE_REGISTER_ULONG((PULONG)(g_apic + MTDETECT_APIC_ICR_HIGH), 0);
  WRITE_REGISTER_ULONG((PULONG)(g_apic + MTDETECT_APIC_ICR_LOW), MTDETECT_ICR_NMI_ALL);
}
