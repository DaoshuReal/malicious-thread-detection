#include "mtdetect/detect/worker.h"

#include <ntifs.h>

#include "mtdetect/lbr/lbr.h"
#include "mtdetect/mm/mm.h"
#include "mtdetect/thread/thread.h"

#define MTDETECT_WORKER_MAX 64
#define MTDETECT_WORKER_LBR_MAX 16

typedef struct {
  HANDLE tid;
  PVOID start;
  PVOID start2;
} MtdetectWorkerEntry;

typedef struct {
  HANDLE tid;
  PVOID from;
  PVOID to;
  ULONG cpu;
} MtdetectWorkerLbr;

static MtdetectWorkerEntry g_worker_queue[MTDETECT_WORKER_MAX];
static ULONG g_worker_count = 0;
static MtdetectWorkerLbr g_worker_lbr[MTDETECT_WORKER_LBR_MAX];
static ULONG g_worker_lbr_count = 0;
static KSPIN_LOCK g_worker_lock;
static KEVENT g_worker_event;
static BOOLEAN g_worker_ready = FALSE;

/* Passive only. Re-reads suspects with fresh modules, never unwinds. */
static void mtdetect_worker_thread(PVOID context)
{
  (void)context;

  for (;;)
  {
    LARGE_INTEGER nap;
    NTSTATUS wait = STATUS_SUCCESS;
    MtdetectWorkerEntry local[MTDETECT_WORKER_MAX];
    MtdetectWorkerLbr lbr_local[MTDETECT_WORKER_LBR_MAX];
    ULONG found = 0;
    ULONG lbr_found = 0;
    ULONG index = 0;
    KIRQL old = PASSIVE_LEVEL;

    nap.QuadPart = -50000LL * 1000LL;

    wait = KeWaitForSingleObject(&g_worker_event, Executive, KernelMode, FALSE, &nap);
    (void)wait;

    KeAcquireSpinLock(&g_worker_lock, &old);
    for (index = 0; index < g_worker_count; index++)
    {
      local[index] = g_worker_queue[index];
    }
    found = g_worker_count;
    g_worker_count = 0;
    for (index = 0; index < g_worker_lbr_count; index++)
    {
      lbr_local[index] = g_worker_lbr[index];
    }
    lbr_found = g_worker_lbr_count;
    g_worker_lbr_count = 0;
    KeReleaseSpinLock(&g_worker_lock, old);

    /* LSTAR watch, read-only, baseline from load. */
    if (mtdetect_lbr_lstar_baseline() != 0)
    {
      ULONG64 lstar = 0;

      if (mtdetect_lbr_lstar_now(&lstar) && lstar != 0 && lstar != mtdetect_lbr_lstar_baseline())
      {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] worker lstar changed now=%p base=%p\n",
            (PVOID)lstar, (PVOID)mtdetect_lbr_lstar_baseline());
      }
    }

    if (found == 0 && lbr_found == 0)
    {
      continue;
    }

    /* Fresh modules before judging, avoids new-driver false hits. */
    mtdetect_mm_refresh();

    for (index = 0; index < lbr_found; index++)
    {
      BOOLEAN from_known = mtdetect_mm_known(lbr_local[index].from);
      BOOLEAN to_known = mtdetect_mm_known(lbr_local[index].to);

      /* Module arrived after the NMI, was a false hit. */
      if (from_known)
      {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] worker lbr clear tid=%p cpu=%lu from=%p\n",
            lbr_local[index].tid, lbr_local[index].cpu, lbr_local[index].from);

        continue;
      }

      DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] worker lbr tid=%p cpu=%lu from=%p to=%p tok=%d\n",
          lbr_local[index].tid, lbr_local[index].cpu, lbr_local[index].from, lbr_local[index].to, (int)to_known);
    }

    for (index = 0; index < found; index++)
    {
      PETHREAD ethread = NULL;
      PVOID live = NULL;
      PVOID live2 = NULL;
      enum mtdetect_thread_verdict_e base = MTDETECT_THREAD_UNKNOWN;
      BOOLEAN known = TRUE;

      if (!NT_SUCCESS(PsLookupThreadByThreadId(local[index].tid, &ethread)))
      {
        continue;
      }

      mtdetect_thread_starts(ethread, &live, &live2);
      ObDereferenceObject(ethread);

      base = mtdetect_thread_check(local[index].tid, live, live2);
      known = mtdetect_mm_known(live);

      /* Queued values raced with exit or reuse, note and move on. */
      if (live != local[index].start || live2 != local[index].start2)
      {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[mtdetect] worker race tid=%p queued=%p/%p live=%p/%p\n", local[index].tid, local[index].start,
            local[index].start2, live, live2);

        continue;
      }

      DbgPrintEx(DPFLTR_IHVDRIVER_ID,
          (!known || base == MTDETECT_THREAD_SPOOFED) ? DPFLTR_ERROR_LEVEL : DPFLTR_INFO_LEVEL,
          "[mtdetect] worker tid=%p start=%p start2=%p base=%d known=%d\n", local[index].tid, live, live2,
          (int)base, (int)known);
    }
  }

  PsTerminateSystemThread(STATUS_SUCCESS);
}

void mtdetect_worker_init(void)
{
  HANDLE handle = NULL;

  if (KeGetCurrentIrql() != PASSIVE_LEVEL)
  {
    return;
  }

  KeInitializeSpinLock(&g_worker_lock);
  KeInitializeEvent(&g_worker_event, SynchronizationEvent, FALSE);
  g_worker_count = 0;
  g_worker_lbr_count = 0;

  if (!NT_SUCCESS(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, NULL, NULL, NULL, mtdetect_worker_thread, NULL)))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] worker: create failed\n");

    return;
  }

  ZwClose(handle);
  g_worker_ready = TRUE;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] worker ready\n");
}

void mtdetect_worker_queue(HANDLE tid, PVOID start, PVOID start2)
{
  KIRQL old = PASSIVE_LEVEL;
  BOOLEAN at_dpc = FALSE;

  if (!tid || !g_worker_ready)
  {
    return;
  }

  if (KeGetCurrentIrql() > DISPATCH_LEVEL)
  {
    return;
  }

  at_dpc = (KeGetCurrentIrql() == DISPATCH_LEVEL);

  if (at_dpc)
  {
    KeAcquireSpinLockAtDpcLevel(&g_worker_lock);
  }
  else
  {
    KeAcquireSpinLock(&g_worker_lock, &old);
  }

  if (g_worker_count < MTDETECT_WORKER_MAX)
  {
    g_worker_queue[g_worker_count].tid = tid;
    g_worker_queue[g_worker_count].start = start;
    g_worker_queue[g_worker_count].start2 = start2;
    g_worker_count++;
  }

  if (at_dpc)
  {
    KeReleaseSpinLockFromDpcLevel(&g_worker_lock);
  }
  else
  {
    KeReleaseSpinLock(&g_worker_lock, old);
  }

  KeSetEvent(&g_worker_event, IO_NO_INCREMENT, FALSE);
}

void mtdetect_worker_queue_lbr(HANDLE tid, PVOID from, PVOID to, ULONG cpu)
{
  KIRQL old = PASSIVE_LEVEL;
  BOOLEAN at_dpc = FALSE;

  if (!tid || !from || !g_worker_ready)
  {
    return;
  }

  if (KeGetCurrentIrql() > DISPATCH_LEVEL)
  {
    return;
  }

  at_dpc = (KeGetCurrentIrql() == DISPATCH_LEVEL);

  if (at_dpc)
  {
    KeAcquireSpinLockAtDpcLevel(&g_worker_lock);
  }
  else
  {
    KeAcquireSpinLock(&g_worker_lock, &old);
  }

  if (g_worker_lbr_count < MTDETECT_WORKER_LBR_MAX)
  {
    g_worker_lbr[g_worker_lbr_count].tid = tid;
    g_worker_lbr[g_worker_lbr_count].from = from;
    g_worker_lbr[g_worker_lbr_count].to = to;
    g_worker_lbr[g_worker_lbr_count].cpu = cpu;
    g_worker_lbr_count++;
  }

  if (at_dpc)
  {
    KeReleaseSpinLockFromDpcLevel(&g_worker_lock);
  }
  else
  {
    KeReleaseSpinLock(&g_worker_lock, old);
  }

  KeSetEvent(&g_worker_event, IO_NO_INCREMENT, FALSE);
}
