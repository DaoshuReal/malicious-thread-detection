#include "mtdetect/thread/thread.h"

static LONG g_offset = -1;

/* Ends at once, we only need its address. */
static void mtdetect_thread_dummy(PVOID context)
{
  (void)context;
}

void mtdetect_thread_init(void)
{
  HANDLE handle = NULL;
  PETHREAD thread = NULL;
  LONG bytes = 0;
  LONG found = -1;
  LONG hits = 0;

  if (!NT_SUCCESS(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, NULL, NULL, NULL, mtdetect_thread_dummy, NULL)))
  {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] thread init: create failed\n");

    return;
  }

  if (NT_SUCCESS(ObReferenceObjectByHandle(handle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)&thread,
      NULL)))
  {
    for (bytes = 0; bytes < 0x1000; bytes += (LONG)sizeof(PVOID))
    {
      if (*(PVOID*)((PUCHAR)thread + bytes) == (PVOID)mtdetect_thread_dummy)
      {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] thread init: match at 0x%x\n", (int)bytes);

        if (found < 0)
        {
          found = bytes;
        }

        hits++;
      }
    }

    ObDereferenceObject(thread);
  }

  ZwClose(handle);

  /* Twins are the two start fields, same on system threads. Take lower. */
  if (hits == 1 || hits == 2)
  {
    g_offset = found;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[mtdetect] thread start offset=0x%x\n", (int)found);

    return;
  }

  DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[mtdetect] thread init: unsure hits=%d\n", (int)hits);
}

PVOID mtdetect_thread_start(PETHREAD thread)
{
  if (!thread || g_offset < 0)
  {
    return NULL;
  }

  return *(PVOID*)((PUCHAR)thread + g_offset);
}
