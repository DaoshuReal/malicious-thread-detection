#include "mtdetect/detect/detect.h"

#include <ntifs.h>

#include "mtdetect/mm/mm.h"
#include "mtdetect/nmi/capture.h"

void mtdetect_detect_scan(void)
{
  ULONG count = mtdetect_nmi_capture_count();
  ULONG index = 0;

  for (index = 0; index < count; index++)
  {
    MtdetectNmiSlot* slot = mtdetect_nmi_capture_slot(index);

    if (!slot || !slot->captured)
    {
      continue;
    }

    slot->captured = FALSE;

    /* Backed by a module means legit, reset and move on. */
    if (mtdetect_mm_known(slot->start))
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

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] malicious thread tid=%p start=%p cpu=%lu\n",
        slot->tid, slot->start, slot->processor_index);
  }
}
