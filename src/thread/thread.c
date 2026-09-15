#include "mtdetect/thread/thread.h"

#include <ntifs.h>

#define MTDETECT_THREAD_POOL_TAG 'THDT'
#define MTDETECT_THREAD_MAX 1024

typedef struct {
  HANDLE tid;
  PVOID start;
  PVOID start2;
  BOOLEAN valid;
} MtdetectThreadEntry;

static LONG g_offset = -1;
static LONG g_offset2 = -1;
static MtdetectThreadEntry* g_thread_table = NULL;
static KSPIN_LOCK g_thread_lock;
static BOOLEAN g_thread_notify_on = FALSE;

/* Ends at once, we only need its address. */
static void mtdetect_thread_dummy(PVOID context)
{
  (void)context;
}

static void mtdetect_thread_notify(HANDLE pid, HANDLE tid, BOOLEAN create)
{
  PETHREAD ethread = NULL;
  PVOID first = NULL;
  PVOID second = NULL;
  ULONG index = 0;
  ULONG free_slot = MTDETECT_THREAD_MAX;
  KIRQL old = PASSIVE_LEVEL;

  (void)pid;

  if (!tid || !g_thread_table)
  {
    return;
  }

  /* Notify runs at passive or APC. Never wait or page above that. */
  if (KeGetCurrentIrql() > APC_LEVEL)
  {
    return;
  }

  if (!create)
  {
    KeAcquireSpinLock(&g_thread_lock, &old);
    for (index = 0; index < MTDETECT_THREAD_MAX; index++)
    {
      if (g_thread_table[index].valid && g_thread_table[index].tid == tid)
      {
        g_thread_table[index].valid = FALSE;
      }
    }
    KeReleaseSpinLock(&g_thread_lock, old);

    return;
  }

  /* Creator context, so look up the new thread for its ETHREAD. */
  if (!NT_SUCCESS(PsLookupThreadByThreadId(tid, &ethread)))
  {
    return;
  }

  /* Keep the baseline to system threads, NMI only samples those. */
  if (!PsIsSystemThread(ethread))
  {
    ObDereferenceObject(ethread);

    return;
  }

  mtdetect_thread_starts(ethread, &first, &second);
  ObDereferenceObject(ethread);

  KeAcquireSpinLock(&g_thread_lock, &old);
  for (index = 0; index < MTDETECT_THREAD_MAX; index++)
  {
    if (g_thread_table[index].valid && g_thread_table[index].tid == tid)
    {
      g_thread_table[index].start = first;
      g_thread_table[index].start2 = second;
      KeReleaseSpinLock(&g_thread_lock, old);

      return;
    }

    if (!g_thread_table[index].valid && free_slot == MTDETECT_THREAD_MAX)
    {
      free_slot = index;
    }
  }

  if (free_slot == MTDETECT_THREAD_MAX)
  {
    free_slot = 0;
  }

  g_thread_table[free_slot].tid = tid;
  g_thread_table[free_slot].start = first;
  g_thread_table[free_slot].start2 = second;
  g_thread_table[free_slot].valid = TRUE;
  KeReleaseSpinLock(&g_thread_lock, old);
}

void mtdetect_thread_init(void)
{
  HANDLE handle = NULL;
  PETHREAD thread = NULL;
  LONG bytes = 0;
  LONG found = -1;
  LONG found2 = -1;
  LONG hits = 0;
  MtdetectThreadEntry* table = NULL;

  if (KeGetCurrentIrql() != PASSIVE_LEVEL)
  {
    return;
  }

  KeInitializeSpinLock(&g_thread_lock);

  if (!NT_SUCCESS(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, NULL, NULL, NULL, mtdetect_thread_dummy, NULL)))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] thread init: create failed\n");

    return;
  }

  if (NT_SUCCESS(ObReferenceObjectByHandle(handle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)&thread,
      NULL)))
  {
    for (bytes = 0; bytes < 0x1000; bytes += (LONG)sizeof(PVOID))
    {
      if (*(PVOID*)((PUCHAR)thread + bytes) == (PVOID)mtdetect_thread_dummy)
      {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] thread init: match at 0x%x\n", (int)bytes);

        if (found < 0)
        {
          found = bytes;
        }
        else if (found2 < 0)
        {
          found2 = bytes;
        }

        hits++;
      }
    }

    ObDereferenceObject(thread);
  }

  ZwClose(handle);

  /* Twins are the two start fields, same on system threads. Take lower. */
  if (hits == 1 || hits == 2)
  {
    g_offset = found;
    g_offset2 = (hits == 2) ? found2 : -1;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] thread start offset=0x%x offset2=0x%x\n",
        (int)found, (int)g_offset2);
  }
  else
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] thread init: unsure hits=%d\n", (int)hits);

    return;
  }

  table = (MtdetectThreadEntry*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(*table) * MTDETECT_THREAD_MAX,
      MTDETECT_THREAD_POOL_TAG);

  if (!table)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] thread init: table alloc failed\n");

    return;
  }

  RtlZeroMemory(table, sizeof(*table) * MTDETECT_THREAD_MAX);
  g_thread_table = table;

  if (!NT_SUCCESS(PsSetCreateThreadNotifyRoutine(mtdetect_thread_notify)))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] thread init: notify failed\n");

    return;
  }

  g_thread_notify_on = TRUE;
}

PVOID mtdetect_thread_start(PETHREAD thread)
{
  PVOID first = NULL;

  mtdetect_thread_starts(thread, &first, NULL);

  return first;
}

void mtdetect_thread_starts(PETHREAD thread, PVOID* first, PVOID* second)
{
  if (first)
  {
    *first = NULL;
  }

  if (second)
  {
    *second = NULL;
  }

  if (!thread)
  {
    return;
  }

  if (g_offset >= 0 && first)
  {
    *first = *(PVOID*)((PUCHAR)thread + g_offset);
  }

  if (g_offset2 >= 0 && second)
  {
    *second = *(PVOID*)((PUCHAR)thread + g_offset2);
  }
}

BOOLEAN mtdetect_thread_twins_mismatch(PVOID first, PVOID second)
{
  if (g_offset2 < 0)
  {
    return FALSE;
  }

  if (!first || !second)
  {
    return FALSE;
  }

  return first != second;
}

enum mtdetect_thread_verdict_e mtdetect_thread_check(HANDLE tid, PVOID current, PVOID current2)
{
  ULONG index = 0;
  enum mtdetect_thread_verdict_e verdict = MTDETECT_THREAD_UNKNOWN;
  KIRQL old = PASSIVE_LEVEL;
  BOOLEAN at_dpc = FALSE;

  if (KeGetCurrentIrql() > DISPATCH_LEVEL)
  {
    return MTDETECT_THREAD_UNKNOWN;
  }

  if (!tid || !g_thread_table)
  {
    return MTDETECT_THREAD_UNKNOWN;
  }

  at_dpc = (KeGetCurrentIrql() == DISPATCH_LEVEL);

  if (at_dpc)
  {
    KeAcquireSpinLockAtDpcLevel(&g_thread_lock);
  }
  else
  {
    KeAcquireSpinLock(&g_thread_lock, &old);
  }

  for (index = 0; index < MTDETECT_THREAD_MAX; index++)
  {
    if (g_thread_table[index].valid && g_thread_table[index].tid == tid)
    {
      if (!g_thread_table[index].start)
      {
        verdict = MTDETECT_THREAD_UNKNOWN;
      }
      else if (g_thread_table[index].start != current)
      {
        verdict = MTDETECT_THREAD_SPOOFED;
      }
      else if (g_offset2 >= 0 && g_thread_table[index].start2 && g_thread_table[index].start2 != current2)
      {
        verdict = MTDETECT_THREAD_SPOOFED;
      }
      else
      {
        verdict = MTDETECT_THREAD_OK;
      }

      break;
    }
  }

  if (at_dpc)
  {
    KeReleaseSpinLockFromDpcLevel(&g_thread_lock);
  }
  else
  {
    KeReleaseSpinLock(&g_thread_lock, old);
  }

  return verdict;
}
