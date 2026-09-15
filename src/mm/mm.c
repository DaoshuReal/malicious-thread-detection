#include "mtdetect/mm/mm.h"

#include <aux_klib.h>
#include <ntifs.h>

#define MTDETECT_MM_POOL_TAG 'MMDT'

typedef struct {
  PVOID base;
  SIZE_T size;
} MtdetectMmEntry;

static MtdetectMmEntry* g_mm_entries = NULL;
static ULONG g_mm_mod_count = 0;
static KSPIN_LOCK g_mm_lock;
static BOOLEAN g_mm_ready = FALSE;

void mtdetect_mm_refresh(void)
{
  ULONG bytes = 0;
  AUX_MODULE_EXTENDED_INFO* info = NULL;
  MtdetectMmEntry* entries = NULL;
  ULONG modules = 0;
  ULONG index = 0;
  MtdetectMmEntry* old = NULL;
  KIRQL old_irql = PASSIVE_LEVEL;

  if (KeGetCurrentIrql() != PASSIVE_LEVEL)
  {
    return;
  }

  if (!g_mm_ready)
  {
    return;
  }

  if (!NT_SUCCESS(AuxKlibQueryModuleInformation(&bytes, sizeof(*info), NULL)) || bytes == 0)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] mm: query failed\n");

    return;
  }

  info = (AUX_MODULE_EXTENDED_INFO*)ExAllocatePool2(POOL_FLAG_NON_PAGED, bytes, MTDETECT_MM_POOL_TAG);

  if (!info)
  {
    return;
  }

  if (!NT_SUCCESS(AuxKlibQueryModuleInformation(&bytes, sizeof(*info), info)))
  {
    ExFreePool(info);

    return;
  }

  modules = bytes / sizeof(*info);
  entries = (MtdetectMmEntry*)ExAllocatePool2(POOL_FLAG_NON_PAGED, (SIZE_T)modules * sizeof(*entries),
      MTDETECT_MM_POOL_TAG);

  if (!entries)
  {
    ExFreePool(info);

    return;
  }

  for (index = 0; index < modules; index++)
  {
    entries[index].base = info[index].BasicInfo.ImageBase;
    entries[index].size = info[index].ImageSize;
  }

  ExFreePool(info);

  KeAcquireSpinLock(&g_mm_lock, &old_irql);
  old = g_mm_entries;
  g_mm_entries = entries;
  g_mm_mod_count = modules;
  KeReleaseSpinLock(&g_mm_lock, old_irql);

  if (old)
  {
    ExFreePool(old);
  }

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] mm ready mods=%lu\n", modules);
}

void mtdetect_mm_init(void)
{
  KeInitializeSpinLock(&g_mm_lock);
  g_mm_ready = TRUE;
  AuxKlibInitialize();
  mtdetect_mm_refresh();
}

BOOLEAN mtdetect_mm_known(PVOID addr)
{
  ULONG index = 0;
  BOOLEAN known = FALSE;
  BOOLEAN at_dpc = FALSE;
  KIRQL old_irql = PASSIVE_LEVEL;

  if (!addr)
  {
    return TRUE;
  }

  if (KeGetCurrentIrql() > DISPATCH_LEVEL)
  {
    return TRUE;
  }

  at_dpc = (KeGetCurrentIrql() == DISPATCH_LEVEL);

  if (at_dpc)
  {
    KeAcquireSpinLockAtDpcLevel(&g_mm_lock);
  }
  else
  {
    KeAcquireSpinLock(&g_mm_lock, &old_irql);
  }

  for (index = 0; index < g_mm_mod_count; index++)
  {
    if (addr >= g_mm_entries[index].base &&
        addr < (PVOID)((PUCHAR)g_mm_entries[index].base + g_mm_entries[index].size))
    {
      known = TRUE;

      break;
    }
  }

  if (at_dpc)
  {
    KeReleaseSpinLockFromDpcLevel(&g_mm_lock);
  }
  else
  {
    KeReleaseSpinLock(&g_mm_lock, old_irql);
  }

  return known;
}
