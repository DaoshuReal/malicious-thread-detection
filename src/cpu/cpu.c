#include "mtdetect/cpu/cpu.h"

#include <intrin.h>

static enum mtdetect_vendor_e g_vendor = MTDETECT_VENDOR_UNKNOWN;
static enum mtdetect_lbr_flavor_e g_cpu_lbr_flavor = MTDETECT_LBR_OFF;

/* Leaf 0 gives "GenuineIntel" / "AuthenticAMD" in EBX, EDX, ECX. */
static enum mtdetect_vendor_e mtdetect_cpu_detect(void)
{
  int regs[4] = {0, 0, 0, 0};

  __cpuid(regs, 0);

  if (regs[1] == 0x756e6547 && regs[3] == 0x49656e69 && regs[2] == 0x6c65746e)
  {
    return MTDETECT_VENDOR_INTEL;
  }

  if (regs[1] == 0x68747541 && regs[3] == 0x69746e65 && regs[2] == 0x444d4163)
  {
    return MTDETECT_VENDOR_AMD;
  }

  return MTDETECT_VENDOR_UNKNOWN;
}

/* Family/model pick the MSR layout. Probe at load still decides. */
static enum mtdetect_lbr_flavor_e mtdetect_cpu_lbr_pick(enum mtdetect_vendor_e vendor)
{
  int regs[4] = {0, 0, 0, 0};
  unsigned int family = 0;
  unsigned int model = 0;

  if (vendor == MTDETECT_VENDOR_AMD)
  {
    __cpuidex(regs, 0x80000000, 0);

    if ((unsigned int)regs[0] >= 0x80000022UL)
    {
      __cpuidex(regs, 0x80000022, 0);

      /* Zen4+ LBRv2 uses other MSRs, out of scope, stay off. */
      if (regs[0] & 0x2)
      {
        return MTDETECT_LBR_OFF;
      }
    }

    /* Legacy pair, confirmed only by the load-time probe. */
    return MTDETECT_LBR_AMD_LEGACY;
  }

  if (vendor != MTDETECT_VENDOR_INTEL)
  {
    return MTDETECT_LBR_OFF;
  }

  __cpuid(regs, 1);
  family = ((unsigned int)regs[0] >> 8) & 0xfUL;
  model = ((unsigned int)regs[0] >> 4) & 0xfUL;

  if (family == 0x6UL)
  {
    family += ((unsigned int)regs[0] >> 20) & 0xffUL;
    model |= ((unsigned int)regs[0] >> 12) & 0xf0UL;
  }
  else if (family == 0xfUL)
  {
    /* NetBurst layout differs, skip. */
    return MTDETECT_LBR_OFF;
  }
  else
  {
    return MTDETECT_LBR_OFF;
  }

  if (model == 0x0fUL || model == 0x16UL || model == 0x17UL || model == 0x1dUL)
  {
    return MTDETECT_LBR_INTEL_CORE2;
  }

  if (model >= 0x1aUL)
  {
    return MTDETECT_LBR_INTEL_NHM;
  }

  return MTDETECT_LBR_INTEL_P6;
}

void mtdetect_cpu_init(void)
{
  g_vendor = mtdetect_cpu_detect();
  g_cpu_lbr_flavor = mtdetect_cpu_lbr_pick(g_vendor);
}

enum mtdetect_vendor_e mtdetect_cpu_vendor(void)
{
  return g_vendor;
}

enum mtdetect_lbr_flavor_e mtdetect_cpu_lbr_flavor(void)
{
  return g_cpu_lbr_flavor;
}

const char* mtdetect_cpu_vendor_name(enum mtdetect_vendor_e vendor)
{
  switch (vendor)
  {
    case MTDETECT_VENDOR_INTEL:
      return "intel";
    case MTDETECT_VENDOR_AMD:
      return "amd";
    default:
      break;
  }

  return "unknown";
}
