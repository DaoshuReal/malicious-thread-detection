#pragma once

#include <ntifs.h>

/*
 * Driver object passed to DriverEntry.
 */
PDRIVER_OBJECT mtdetect_driver_object(void);
