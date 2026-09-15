#include "mtdetect/nmi/trigger.h"

#include <ntifs.h>

#include "mtdetect/nmi/callback.h"
#include "mtdetect/nmi/capture.h"
#include "mtdetect/nmi/send.h"

#define MTDETECT_NMI_PERIOD_MS 2000

static KTIMER g_timer;
static KDPC g_dpc;
static volatile LONG g_ticks = 0;

/* Runs every period. Sends one broadcast, then prints. */
static void mtdetect_nmi_trigger_dpc(PKDPC dpc, PVOID context, PVOID arg1, PVOID arg2)
{
  LONG tick = 0;

  (void)dpc;
  (void)context;
  (void)arg1;
  (void)arg2;

  mtdetect_nmi_callback_arm(mtdetect_nmi_capture_count());
  mtdetect_nmi_send_all();

  tick = InterlockedIncrement(&g_ticks);

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] nmi tick %d cpus=%lu hits=%d\n",
      (int)tick, mtdetect_nmi_capture_count(), (int)mtdetect_nmi_callback_hits());
}

void mtdetect_nmi_trigger_init(void)
{
  LARGE_INTEGER due;

  KeInitializeTimer(&g_timer);
  KeInitializeDpc(&g_dpc, mtdetect_nmi_trigger_dpc, NULL);

  /* First fire in one period, then repeat. Negative = relative. */
  due.QuadPart = -(LONGLONG)MTDETECT_NMI_PERIOD_MS * 10000LL;
  KeSetTimerEx(&g_timer, due, MTDETECT_NMI_PERIOD_MS, &g_dpc);

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] nmi trigger every %d ms\n",
      MTDETECT_NMI_PERIOD_MS);
}

long mtdetect_nmi_trigger_ticks(void)
{
  return (long)g_ticks;
}
