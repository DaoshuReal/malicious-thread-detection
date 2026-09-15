#pragma once

enum mtdetect_vendor_e {
  MTDETECT_VENDOR_UNKNOWN = 0,
  MTDETECT_VENDOR_INTEL = 1,
  MTDETECT_VENDOR_AMD = 2
};

enum mtdetect_lbr_flavor_e {
  MTDETECT_LBR_OFF = 0,
  MTDETECT_LBR_INTEL_NHM = 1,
  MTDETECT_LBR_INTEL_CORE2 = 2,
  MTDETECT_LBR_INTEL_P6 = 3,
  MTDETECT_LBR_AMD_LEGACY = 4
};

/* Detect once at load. */
void mtdetect_cpu_init(void);

/* Cached result from init. */
enum mtdetect_vendor_e mtdetect_cpu_vendor(void);

/* LBR layout candidate, probe still decides. OFF means no try. */
enum mtdetect_lbr_flavor_e mtdetect_cpu_lbr_flavor(void);

/* Never returns NULL. */
const char* mtdetect_cpu_vendor_name(enum mtdetect_vendor_e vendor);
