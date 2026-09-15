#include "mtdetect/detect/detect.h"

#include <ntifs.h>

#include "mtdetect/detect/worker.h"
#include "mtdetect/mm/mm.h"
#include "mtdetect/nmi/capture.h"
#include "mtdetect/thread/thread.h"

void mtdetect_detect_scan(void)
{
  ULONG count = mtdetect_nmi_capture_count();
  ULONG index = 0;

  for (index = 0; index < count; index++)
  {
    MtdetectNmiSlot* slot = mtdetect_nmi_capture_slot(index);
    HANDLE tid = NULL;
    PVOID start = NULL;
    PVOID start2 = NULL;
    BOOLEAN twins_bad = FALSE;
    enum mtdetect_thread_verdict_e base = MTDETECT_THREAD_UNKNOWN;

    if (!slot || !slot->captured)
    {
      continue;
    }

    tid = slot->tid;
    start = slot->start;
    start2 = slot->start2;
    slot->captured = FALSE;

    if (!tid)
    {
      continue;
    }

    twins_bad = mtdetect_thread_twins_mismatch(start, start2);
    base = mtdetect_thread_check(tid, start, start2);

    /* Spoof wins over malicious, it means ETHREAD was rewritten. */
    if (twins_bad || base == MTDETECT_THREAD_SPOOFED)
    {
      if (!slot->spoof_reported || slot->spoof_tid != tid || slot->spoof_start != start ||
          slot->spoof_start2 != start2)
      {
        slot->spoof_reported = TRUE;
        slot->spoof_tid = tid;
        slot->spoof_start = start;
        slot->spoof_start2 = start2;

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "[mtdetect] spoof tid=%p start=%p start2=%p cpu=%lu twins=%d base=%d\n", tid, start, start2,
            slot->processor_index, (int)twins_bad, (int)base);
      }

      slot->reported = FALSE;
      slot->reported_start = NULL;
      mtdetect_worker_queue(tid, start, start2);

      continue;
    }

    slot->spoof_reported = FALSE;
    slot->spoof_tid = NULL;
    slot->spoof_start = NULL;
    slot->spoof_start2 = NULL;

    /* Backed by a module means legit, reset and move on. */
    if (mtdetect_mm_known(start))
    {
      slot->reported = FALSE;
      slot->reported_start = NULL;

      continue;
    }

    /* Same bad thread as last tick, skip. */
    if (slot->reported && slot->start == slot->reported_start)
    {
      continue;
    }

    slot->reported = TRUE;
    slot->reported_start = slot->start;
    mtdetect_worker_queue(tid, start, start2);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] malicious thread tid=%p start=%p cpu=%lu\n",
        slot->tid, slot->start, slot->processor_index);
  }
}
