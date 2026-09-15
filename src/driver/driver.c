#include "mtdetect/driver/driver.h"

#include <ntifs.h>

#include "mtdetect/cpu/cpu.h"
#include "mtdetect/nmi/nmi.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING regpath)
{
  (void)driver;
  (void)regpath;

  mtdetect_cpu_init();

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] load vendor=%s\n",
      mtdetect_cpu_vendor_name(mtdetect_cpu_vendor()));

  mtdetect_nmi_init();

  return STATUS_SUCCESS;
}
