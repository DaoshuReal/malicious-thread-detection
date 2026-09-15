#include <ntifs.h>

static HANDLE g_thread = NULL;

/*
 * Malicious thread: a system thread with no module entry once manually
 * mapped. it calls an ntoskrnl routine in a loop so the detector
 * sees from (mapped) -> to (ntoskrnl) in the lbr.
 */
static void mttest_thread(PVOID context)
{
  ULONG calls = 0;

  (void)context;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] thread running\n");

  while (true)
  {
    ULONG cpu = 0;

    /* ntoskrnl call, the branch the lbr records. */
    cpu = KeGetCurrentProcessorNumberEx(NULL);
    (void)cpu;

    KeStallExecutionProcessor(100);
    calls++;

    if ((calls % 512) == 0)
    {
      LARGE_INTEGER nap;

      nap.QuadPart = -10000LL * 10LL;
      KeDelayExecutionThread(KernelMode, FALSE, &nap);
    }
  }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING regpath)
{
  NTSTATUS status = STATUS_SUCCESS;

  (void)regpath;
  (void)driver;

  status = PsCreateSystemThread(&g_thread, THREAD_ALL_ACCESS, NULL, NULL, NULL, mttest_thread, NULL);

  if (!NT_SUCCESS(status))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
        "[mttest] thread failed %x\n", status);

    return status;
  }

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] mapped, thread started\n");

  return STATUS_SUCCESS;
}
