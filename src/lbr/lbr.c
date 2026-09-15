#include "mtdetect/lbr/lbr.h"

#include <ntifs.h>

#include "mtdetect/cpu/cpu.h"
#include "mtdetect/cpu/msr.h"

#define MTDETECT_LBR_DEBUGCTL 0x1d9UL
#define MTDETECT_LBR_TOS 0x1c9UL
#define MTDETECT_LBR_NHM_FROM 0x680UL
#define MTDETECT_LBR_NHM_TO 0x6c0UL
#define MTDETECT_LBR_C2_FROM 0x40UL
#define MTDETECT_LBR_C2_TO 0x60UL
#define MTDETECT_LBR_P6_FROM 0x1dbUL
#define MTDETECT_LBR_P6_TO 0x1dcUL
#define MTDETECT_LBR_LSTAR 0xc0000082UL

static enum mtdetect_lbr_flavor_e g_lbr_flavor = MTDETECT_LBR_OFF;
static BOOLEAN g_lbr_ready = FALSE;
static ULONG64 g_lbr_lstar = 0;

static void mtdetect_lbr_zero(PUCHAR tos, PVOID* from, PVOID* to)
{
  ULONG index = 0;

  if (tos)
  {
    *tos = 0;
  }

  for (index = 0; index < MTDETECT_LBR_DEPTH; index++)
  {
    if (from)
    {
      from[index] = NULL;
    }

    if (to)
    {
      to[index] = NULL;
    }
  }
}

/* Probe once here so NMI never risks a fault up there. */
void mtdetect_lbr_init(void)
{
  ULONG64 dbg = 0;
  ULONG64 tos = 0;
  ULONG64 from = 0;
  ULONG64 to = 0;
  volatile ULONG64 acc = 0;
  ULONG spin = 0;

  if (KeGetCurrentIrql() != PASSIVE_LEVEL)
  {
    return;
  }

  g_lbr_flavor = mtdetect_cpu_lbr_flavor();

  if (g_lbr_flavor == MTDETECT_LBR_OFF)
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] lbr off, no flavor\n");

    return;
  }

  __try
  {
    dbg = mtdetect_read_msr(MTDETECT_LBR_DEBUGCTL);

    if ((dbg & 0x1ULL) == 0)
    {
      mtdetect_write_msr(MTDETECT_LBR_DEBUGCTL, dbg | 0x1ULL);
    }

    /* Force branches, dead registers still read back zero after this. */
    for (spin = 0; spin < 32; spin++)
    {
      if ((spin & 1UL) == 0)
      {
        acc += (ULONG64)spin;
      }
    }

    if (g_lbr_flavor == MTDETECT_LBR_INTEL_NHM)
    {
      tos = mtdetect_read_msr(MTDETECT_LBR_TOS) & 0xfULL;
      from = mtdetect_read_msr(MTDETECT_LBR_NHM_FROM + (ULONG)tos);
      to = mtdetect_read_msr(MTDETECT_LBR_NHM_TO + (ULONG)tos);
    }
    else if (g_lbr_flavor == MTDETECT_LBR_INTEL_CORE2)
    {
      tos = mtdetect_read_msr(MTDETECT_LBR_TOS) & 0x3ULL;
      from = mtdetect_read_msr(MTDETECT_LBR_C2_FROM + (ULONG)tos);
      to = mtdetect_read_msr(MTDETECT_LBR_C2_TO + (ULONG)tos);
    }
    else
    {
      from = mtdetect_read_msr(MTDETECT_LBR_P6_FROM);
      to = mtdetect_read_msr(MTDETECT_LBR_P6_TO);
    }

    if (from == 0 && to == 0)
    {
      g_lbr_flavor = MTDETECT_LBR_OFF;

      DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] lbr empty after branches, off\n");

      return;
    }

    g_lbr_lstar = mtdetect_read_msr(MTDETECT_LBR_LSTAR);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    g_lbr_flavor = MTDETECT_LBR_OFF;
    g_lbr_lstar = 0;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] lbr probe faulted, off\n");

    return;
  }

  g_lbr_ready = TRUE;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] lbr ready flavor=%d lstar=%p\n",
      (int)g_lbr_flavor, (PVOID)g_lbr_lstar);
}

BOOLEAN mtdetect_lbr_ready(void)
{
  return g_lbr_ready;
}

int mtdetect_lbr_status(void)
{
  if (!g_lbr_ready)
  {
    return -1;
  }

  return (int)g_lbr_flavor;
}

void mtdetect_lbr_capture(PUCHAR tos, PVOID* from, PVOID* to)
{
  ULONG64 raw_tos = 0;
  ULONG64 raw_dbg = 0;
  ULONG mask = 0;
  ULONG base_from = 0;
  ULONG base_to = 0;
  ULONG index = 0;
  ULONG slot = 0;

  if (!g_lbr_ready)
  {
    mtdetect_lbr_zero(tos, from, to);

    return;
  }

  if (g_lbr_flavor == MTDETECT_LBR_INTEL_NHM)
  {
    mask = 0xfUL;
    base_from = MTDETECT_LBR_NHM_FROM;
    base_to = MTDETECT_LBR_NHM_TO;
  }
  else if (g_lbr_flavor == MTDETECT_LBR_INTEL_CORE2)
  {
    mask = 0x3UL;
    base_from = MTDETECT_LBR_C2_FROM;
    base_to = MTDETECT_LBR_C2_TO;
  }
  else
  {
    /* Single pair, no TOS on this layout. */
    if (tos)
    {
      *tos = 0;
    }

    if (from)
    {
      from[0] = (PVOID)(mtdetect_read_msr(MTDETECT_LBR_P6_FROM) & 0xffffffffffffULL);

      for (index = 1; index < MTDETECT_LBR_DEPTH; index++)
      {
        from[index] = NULL;
      }
    }

    if (to)
    {
      to[0] = (PVOID)(mtdetect_read_msr(MTDETECT_LBR_P6_TO) & 0xffffffffffffULL);

      for (index = 1; index < MTDETECT_LBR_DEPTH; index++)
      {
        to[index] = NULL;
      }
    }

    raw_dbg = mtdetect_read_msr(MTDETECT_LBR_DEBUGCTL);

    if ((raw_dbg & 0x1ULL) == 0)
    {
      mtdetect_write_msr(MTDETECT_LBR_DEBUGCTL, raw_dbg | 0x1ULL);
    }

    return;
  }

  raw_tos = mtdetect_read_msr(MTDETECT_LBR_TOS) & (ULONG64)mask;

  if (tos)
  {
    *tos = (UCHAR)raw_tos;
  }

  /* Walk back from TOS, top entries are our own NMI dispatch noise. */
  for (index = 0; index < MTDETECT_LBR_DEPTH; index++)
  {
    slot = ((ULONG)raw_tos - index) & mask;

    if (from)
    {
      from[index] = (PVOID)(mtdetect_read_msr(base_from + slot) & 0xffffffffffffULL);
    }

    if (to)
    {
      to[index] = (PVOID)(mtdetect_read_msr(base_to + slot) & 0xffffffffffffULL);
    }
  }

  raw_dbg = mtdetect_read_msr(MTDETECT_LBR_DEBUGCTL);

  if ((raw_dbg & 0x1ULL) == 0)
  {
    mtdetect_write_msr(MTDETECT_LBR_DEBUGCTL, raw_dbg | 0x1ULL);
  }
}

ULONG64 mtdetect_lbr_lstar_baseline(void)
{
  return g_lbr_lstar;
}

BOOLEAN mtdetect_lbr_lstar_now(ULONG64* value)
{
  ULONG64 lstar = 0;

  if (value)
  {
    *value = 0;
  }

  if (KeGetCurrentIrql() != PASSIVE_LEVEL)
  {
    return FALSE;
  }

  __try
  {
    lstar = mtdetect_read_msr(MTDETECT_LBR_LSTAR);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return FALSE;
  }

  if (value)
  {
    *value = lstar;
  }

  return TRUE;
}
