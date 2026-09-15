#include "mtdetect/nmi/callback.h"

#include "mtdetect/lbr/lbr.h"
#include "mtdetect/nmi/capture.h"
#include "mtdetect/thread/thread.h"

static PVOID g_handle = NULL;
static volatile LONG g_hits = 0;
static volatile LONG g_pending = 0;

/* Runs at NMI level, so keep it tiny. No prints here. */
static BOOLEAN mtdetect_nmi_callback(PVOID context, BOOLEAN handled)
{
  PETHREAD thread = NULL;
  MtdetectNmiSlot* slot = NULL;
  PVOID first = NULL;
  PVOID second = NULL;

  (void)context;
  (void)handled;

  /* Nobody claimed means bugcheck, so swallow only what we sent. */
  if (InterlockedDecrement(&g_pending) < 0)
  {
    InterlockedIncrement(&g_pending);

    return FALSE;
  }

  /* Just field reads, fine up here. User threads never count. */
  thread = PsGetCurrentThread();

  if (PsIsSystemThread(thread))
  {
    slot = mtdetect_nmi_capture_slot(KeGetCurrentProcessorIndex());

    if (slot)
    {
      mtdetect_thread_starts(thread, &first, &second);
      slot->tid = PsGetThreadId(thread);
      slot->start = first;
      slot->start2 = second;
      mtdetect_lbr_capture(&slot->lbr_tos, slot->lbr_from, slot->lbr_to);
      slot->lbr_valid = mtdetect_lbr_ready();
      slot->captured = TRUE;
    }
  }

  InterlockedIncrement(&g_hits);

  return TRUE;
}

void mtdetect_nmi_callback_init(void)
{
  g_handle = KeRegisterNmiCallback(mtdetect_nmi_callback, NULL);

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, g_handle ? DPFLTR_INFO_LEVEL : DPFLTR_ERROR_LEVEL,
      "[mtdetect] nmi callback %s\n", g_handle ? "ready" : "failed");
}

long mtdetect_nmi_callback_hits(void)
{
  return (long)g_hits;
}

void mtdetect_nmi_callback_arm(ULONG cpus)
{
  InterlockedExchange(&g_pending, (LONG)cpus);
}
