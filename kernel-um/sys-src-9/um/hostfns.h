/*
 * This file will be automatically processed, and code
 * generated to call host functions and host OS syscalls.
 *
 * Rules:
 *
 * For host function calls, place HOSTLINK right before the
 * function name, and HOSTARGS right after. 
 * The whole prototype has to be on a single line
 * (or at least HOSTLINK and function name and HOSTARGS).
 */

void * HOSTLINK host_alloc HOSTARGS (ulong size);

int HOSTLINK host_write HOSTARGS (char *buf, int length);

void HOSTLINK host_free HOSTARGS (void *p);

int HOSTLINK host_cpufreq HOSTARGS (void);

void HOSTLINK kprintf HOSTARGS (char *fmt, ...);

void HOSTLINK host_getconf HOSTARGS (char *buf, int size);

void HOSTLINK host_memsize HOSTARGS (ulong *pbase, ulong *pnpage);

void HOSTLINK host_confmem HOSTARGS (ulong *pnpage, ulong *pupages);

ulong HOSTLINK host_ustktop HOSTARGS (void);

void HOSTLINK host_timeres HOSTARGS (uvlong *phz, uvlong *pres);

void HOSTLINK host_kpool HOSTARGS (ulong *pkpl, char *kernmax);

