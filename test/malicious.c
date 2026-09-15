#include <ntifs.h>

static HANDLE g_thread = NULL;
static HANDLE g_spoofer = NULL;
static LONG g_off1 = -1;
static LONG g_off2 = -1;
static PVOID g_real1 = NULL;
static PVOID g_real2 = NULL;

/* Fake bad thread. Manually mapped, so no module entry. Loops on a
 * kernel call so the detector can spot it. */
static void mttest_thread(PVOID context)
{
  ULONG calls = 0;

  (void)context;

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] thread running\n");

  for (;;)
  {
    ULONG cpu = 0;

    /* Kernel call the detector looks for. */
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

/* Ends at once, we only need its address. */
static void mttest_dummy(PVOID context)
{
  (void)context;
}

/* Learns our own ETHREAD offsets, same scan as the detector. */
static void mttest_learn(void)
{
  HANDLE handle = NULL;
  PETHREAD thread = NULL;
  LONG bytes = 0;

  if (!NT_SUCCESS(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, NULL, NULL, NULL, mttest_dummy, NULL)))
  {
    return;
  }

  if (NT_SUCCESS(ObReferenceObjectByHandle(handle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)&thread,
      NULL)))
  {
    for (bytes = 0; bytes < 0x1000; bytes += (LONG)sizeof(PVOID))
    {
      if (*(PVOID*)((PUCHAR)thread + bytes) == (PVOID)mttest_dummy)
      {
        if (g_off1 < 0)
        {
          g_off1 = bytes;
        }
        else if (g_off2 < 0)
        {
          g_off2 = bytes;

          break;
        }
      }
    }

    ObDereferenceObject(thread);
  }

  ZwClose(handle);
}

static void mttest_write_twins(PVOID first, PVOID second)
{
  PETHREAD thread = NULL;

  if (g_off1 < 0 || !g_thread)
  {
    return;
  }

  if (!NT_SUCCESS(ObReferenceObjectByHandle(g_thread, THREAD_ALL_ACCESS, *PsThreadType, KernelMode,
      (PVOID*)&thread, NULL)))
  {
    return;
  }

  *(PVOID*)((PUCHAR)thread + g_off1) = first;
  if (g_off2 >= 0)
  {
    *(PVOID*)((PUCHAR)thread + g_off2) = second;
  }

  ObDereferenceObject(thread);
}

static void mttest_read_twins(PVOID* first, PVOID* second)
{
  PETHREAD thread = NULL;

  if (first)
  {
    *first = NULL;
  }

  if (second)
  {
    *second = NULL;
  }

  if (g_off1 < 0 || !g_thread)
  {
    return;
  }

  if (!NT_SUCCESS(ObReferenceObjectByHandle(g_thread, THREAD_ALL_ACCESS, *PsThreadType, KernelMode,
      (PVOID*)&thread, NULL)))
  {
    return;
  }

  if (first)
  {
    *first = *(PVOID*)((PUCHAR)thread + g_off1);
  }

  if (second && g_off2 >= 0)
  {
    *second = *(PVOID*)((PUCHAR)thread + g_off2);
  }

  ObDereferenceObject(thread);
}

/* Test only, writes our own ETHREAD. May trip PatchGuard, use a VM. */
static void mttest_spoofer(PVOID context)
{
  LARGE_INTEGER nap;

  (void)context;

  nap.QuadPart = -10000LL * 5000LL;
  KeDelayExecutionThread(KernelMode, FALSE, &nap);

  mttest_read_twins(&g_real1, &g_real2);
  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] real=%p/%p\n", g_real1, g_real2);

  /* Phase one, one twin only, detector should flag twin mismatch. */
  mttest_write_twins((PVOID)KeGetCurrentProcessorNumberEx, g_real2);
  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] spoofed twin one\n");

  nap.QuadPart = -10000LL * 10000LL;
  KeDelayExecutionThread(KernelMode, FALSE, &nap);

  /* Phase two, both twins to legit code, baseline should still flag. */
  mttest_write_twins((PVOID)KeGetCurrentProcessorNumberEx, (PVOID)KeGetCurrentProcessorNumberEx);
  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] spoofed both twins\n");

  nap.QuadPart = -10000LL * 10000LL;
  KeDelayExecutionThread(KernelMode, FALSE, &nap);

  /* Restore so no tamper is left behind. */
  mttest_write_twins(g_real1, g_real2);
  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] restored\n");

  PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING regpath)
{
  NTSTATUS status = STATUS_SUCCESS;

  (void)regpath;
  (void)driver;

  mttest_learn();

  status = PsCreateSystemThread(&g_thread, THREAD_ALL_ACCESS, NULL, NULL, NULL, mttest_thread, NULL);

  if (!NT_SUCCESS(status))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mttest] thread failed %x\n", status);

    return status;
  }

  status = PsCreateSystemThread(&g_spoofer, THREAD_ALL_ACCESS, NULL, NULL, NULL, mttest_spoofer, NULL);

  if (!NT_SUCCESS(status))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mttest] spoofer failed %x\n", status);

    return STATUS_SUCCESS;
  }

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mttest] mapped, thread started off1=0x%x off2=0x%x\n",
      (int)g_off1, (int)g_off2);

  return STATUS_SUCCESS;
}
