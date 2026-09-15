#pragma once

enum mtdetect_vendor_e {
  MTDETECT_VENDOR_UNKNOWN = 0,
  MTDETECT_VENDOR_INTEL = 1,
  MTDETECT_VENDOR_AMD = 2
};

/* Detect once at load. */
void mtdetect_cpu_init(void);

/* Cached result from init. */
enum mtdetect_vendor_e mtdetect_cpu_vendor(void);

/* Never returns NULL. */
const char* mtdetect_cpu_vendor_name(enum mtdetect_vendor_e vendor);
