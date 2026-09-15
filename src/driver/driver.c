#include "mtdetect/driver/driver.h"

#include "mtdetect/cpu/cpu.h"

static PDRIVER_OBJECT g_driver = NULL;

PDRIVER_OBJECT mtdetect_driver_object(void)
{
  return g_driver;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING regpath)
{
  (void)regpath;
  (void)driver;

  mtdetect_cpu_init();

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] load vendor=%s\n",
      mtdetect_cpu_vendor_name(mtdetect_cpu_vendor()));

  /*
   * Phase 1 (next): register the NMI callback and start a periodic
   * sweep over every core. Phase 2: validate the interrupted RIP
   * against the loaded-module list and DbgPrint the malicious thread.
   * LBR FROM->TO correlation follows after the NMI path works.
   */

  return STATUS_SUCCESS;
}
