#include "mtdetect/cpu/cpu.h"

#include <intrin.h>

static enum mtdetect_vendor_e g_vendor = MTDETECT_VENDOR_UNKNOWN;

/*
 * CPUID leaf 0 returns the 12-byte vendor string in EBX, EDX, ECX.
 */
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

void mtdetect_cpu_init(void)
{
  g_vendor = mtdetect_cpu_detect();
}

enum mtdetect_vendor_e mtdetect_cpu_vendor(void)
{
  return g_vendor;
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
