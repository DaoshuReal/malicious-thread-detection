#include "mtdetect/nmi/capture.h"

#define MTDETECT_NMI_POOL_TAG 'DMTN'

static MtdetectNmiSlot* g_slots = NULL;
static ULONG g_count = 0;

void mtdetect_nmi_capture_init(void)
{
  ULONG count = 0;
  MtdetectNmiSlot* slots = NULL;
  ULONG index = 0;

  count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

  if (count == 0)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] nmi capture: no cpus\n");

    return;
  }

  slots = (MtdetectNmiSlot*)ExAllocatePool2(POOL_FLAG_NON_PAGED, (SIZE_T)count * sizeof(*slots), MTDETECT_NMI_POOL_TAG);

  if (!slots)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] nmi capture: alloc failed\n");

    return;
  }

  RtlZeroMemory(slots, (SIZE_T)count * sizeof(*slots));

  for (index = 0; index < count; index++)
  {
    slots[index].processor_index = index;
  }

  g_slots = slots;
  g_count = count;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] nmi capture ready cpus=%lu\n", g_count);
}

ULONG mtdetect_nmi_capture_count(void)
{
  return g_count;
}

MtdetectNmiSlot* mtdetect_nmi_capture_slot(ULONG index)
{
  if (!g_slots || index >= g_count)
  {
    return NULL;
  }

  return &g_slots[index];
}
