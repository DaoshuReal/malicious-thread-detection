#include "mtdetect/detect/detect.h"

#include <ntifs.h>

#include "mtdetect/detect/worker.h"
#include "mtdetect/lbr/lbr.h"
#include "mtdetect/mm/mm.h"
#include "mtdetect/nmi/capture.h"
#include "mtdetect/thread/thread.h"

/* Kernel half only, user branches never count. */
#define MTDETECT_LBR_KERNEL_MIN 0xffff800000000000ULL

/* Global unseen table, same idea as the 50-slot dedup. DPC only. */
#define MTDETECT_LBR_SEEN_MAX 50

static PVOID g_detect_lbr_seen[MTDETECT_LBR_SEEN_MAX];
static ULONG g_detect_lbr_seen_pos = 0;
static BOOLEAN g_detect_lbr_dumped = FALSE;

/* True first time, records the address. */
static BOOLEAN mtdetect_detect_lbr_unseen(PVOID addr)
{
  ULONG index = 0;

  for (index = 0; index < MTDETECT_LBR_SEEN_MAX; index++)
  {
    if (g_detect_lbr_seen[index] == addr)
    {
      return FALSE;
    }
  }

  g_detect_lbr_seen[g_detect_lbr_seen_pos] = addr;
  g_detect_lbr_seen_pos = (g_detect_lbr_seen_pos + 1) % MTDETECT_LBR_SEEN_MAX;

  return TRUE;
}

static BOOLEAN mtdetect_detect_lbr_kernel(PVOID addr)
{
  return addr && (ULONG64)addr >= MTDETECT_LBR_KERNEL_MIN;
}

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

    /* Silicon check, needs no ETHREAD trust. Runs on every sample. */
    if (slot->lbr_valid)
    {
      PVOID evil_from = NULL;
      PVOID evil_to = NULL;
      ULONG evil_kind = 0;
      ULONG pair = 0;

      /* One-shot silicon truth, settles zero-vs-live MSRs. */
      if (!g_detect_lbr_dumped)
      {
        g_detect_lbr_dumped = TRUE;

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] lbr raw cpu=%lu tos=%u from=%p to=%p\n",
            slot->processor_index, slot->lbr_tos, slot->lbr_from[0], slot->lbr_to[0]);
      }

      for (pair = 0; pair < MTDETECT_LBR_DEPTH; pair++)
      {
        PVOID from = slot->lbr_from[pair];
        PVOID to = slot->lbr_to[pair];

        /* Zero means C-state cleared or empty, user means other CPL. */
        if (!mtdetect_detect_lbr_kernel(from) || !mtdetect_detect_lbr_kernel(to))
        {
          continue;
        }

        if (mtdetect_mm_known(from))
        {
          continue;
        }

        /* First unknown FROM with a known TO is a branch into us. */
        if (!evil_from && mtdetect_mm_known(to))
        {
          evil_from = from;
          evil_to = to;
          evil_kind = 1;
        }
      }

      /* Pair zero is the youngest branch, unknown means running it now. */
      if (!evil_from && mtdetect_detect_lbr_kernel(slot->lbr_from[0]) &&
          mtdetect_detect_lbr_kernel(slot->lbr_to[0]) && !mtdetect_mm_known(slot->lbr_from[0]))
      {
        evil_from = slot->lbr_from[0];
        evil_to = slot->lbr_to[0];
        evil_kind = 2;
      }

      if (evil_from)
      {
        if (!slot->lbr_reported || slot->lbr_reported_from != evil_from || slot->lbr_reported_to != evil_to)
        {
          slot->lbr_reported = TRUE;
          slot->lbr_reported_from = evil_from;
          slot->lbr_reported_to = evil_to;

          if (mtdetect_detect_lbr_unseen(evil_from))
          {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "[mtdetect] lbr tid=%p cpu=%lu from=%p to=%p kind=%lu\n", tid, slot->processor_index, evil_from,
                evil_to, evil_kind);
          }
        }

        slot->reported = FALSE;
        slot->reported_start = NULL;
        mtdetect_worker_queue(tid, start, start2);
        mtdetect_worker_queue_lbr(tid, evil_from, evil_to, slot->processor_index);

        continue;
      }

      slot->lbr_reported = FALSE;
      slot->lbr_reported_from = NULL;
      slot->lbr_reported_to = NULL;
    }

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
