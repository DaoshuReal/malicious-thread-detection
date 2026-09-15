#pragma once

enum mtdetect_vendor_e {
  MTDETECT_VENDOR_UNKNOWN = 0,
  MTDETECT_VENDOR_INTEL = 1,
  MTDETECT_VENDOR_AMD = 2
};

/*
 * Detect the vendor once and cache it.
 */
void mtdetect_cpu_init(void);

/*
 * Cached cpu vendor. Valid after mtdetect_cpu_init().
 */
enum mtdetect_vendor_e mtdetect_cpu_vendor(void);

/*
 * Readable vendor.
 */
const char* mtdetect_cpu_vendor_name(enum mtdetect_vendor_e vendor);
