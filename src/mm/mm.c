#include "mtdetect/mm/mm.h"

#include <aux_klib.h>

#define MTDETECT_MM_POOL_TAG 'MMDT'

typedef struct {
  PVOID base;
  SIZE_T size;
} MtdetectMmEntry;

static MtdetectMmEntry* g_entries = NULL;
static ULONG g_mod_count = 0;

/* Passive only. Redo when drivers load later. */
static void mtdetect_mm_refresh(void)
{
  ULONG bytes = 0;
  AUX_MODULE_EXTENDED_INFO* info = NULL;
  MtdetectMmEntry* entries = NULL;
  ULONG modules = 0;
  ULONG index = 0;

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

  if (g_entries)
  {
    ExFreePool(g_entries);
  }

  g_entries = entries;
  g_mod_count = modules;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] mm ready mods=%lu\n", g_mod_count);
}

void mtdetect_mm_init(void)
{
  AuxKlibInitialize();
  mtdetect_mm_refresh();
}

BOOLEAN mtdetect_mm_known(PVOID addr)
{
  ULONG index = 0;

  if (!addr)
  {
    return TRUE;
  }

  for (index = 0; index < g_mod_count; index++)
  {
    if (addr >= g_entries[index].base &&
        addr < (PVOID)((PUCHAR)g_entries[index].base + g_entries[index].size))
    {
      return TRUE;
    }
  }

  return FALSE;
}
