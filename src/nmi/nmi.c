#include "mtdetect/nmi/nmi.h"

#include <ntifs.h>

#include "mtdetect/nmi/callback.h"
#include "mtdetect/nmi/capture.h"
#include "mtdetect/nmi/send.h"
#include "mtdetect/nmi/trigger.h"
#include "mtdetect/mm/mm.h"
#include "mtdetect/thread/thread.h"

void mtdetect_nmi_init(void)
{
  mtdetect_nmi_capture_init();
  mtdetect_nmi_callback_init();
  mtdetect_thread_init();
  mtdetect_mm_init();
  mtdetect_nmi_send_init();
  mtdetect_nmi_trigger_init();

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] nmi ready cpus=%lu hits=%d\n",
      mtdetect_nmi_capture_count(), mtdetect_nmi_callback_hits());
}
