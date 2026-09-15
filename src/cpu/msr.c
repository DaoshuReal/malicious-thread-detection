#include "mtdetect/cpu/msr.h"

ULONG64 mtdetect_read_msr(ULONG msr)
{
  ULONG low = 0;
  ULONG high = 0;

  __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

  return ((ULONG64)high << 32) | low;
}

void mtdetect_write_msr(ULONG msr, ULONG64 value)
{
  ULONG low = (ULONG)value;
  ULONG high = (ULONG)(value >> 32);

  __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}
